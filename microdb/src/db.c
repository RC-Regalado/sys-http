#include "db.h"
#include "crc32.h"
#include "mmap.h"
#include "page.h"
#include "paged_file.h"
#include "util.h"
#include "wal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DB_RECORD_WIRE_VERSION 1u
#define DB_RECORD_WIRE_HDR_BYTES 24u
#define DB_DEFAULT_NAMESPACE "default"
#define DB_DEFAULT_CONTENT_TYPE "application/octet-stream"
#define DB_DEFAULT_INLINE_THRESHOLD 512u

typedef struct {
  uint32_t flags;
  const uint8_t *namespace_name;
  uint32_t namespace_len;
  const uint8_t *content_type;
  uint32_t content_type_len;
  const uint8_t *blob_id;
  uint32_t blob_id_len;
  const uint8_t *data;
  uint32_t data_len;
} record_wire_t;

typedef struct {
  uint8_t *buf;
  size_t len;
} encoded_record_t;

typedef struct {
  paged_file_t pf;
  page_t page;
  uint32_t pid;
  int has_page;
} snapshot_writer_t;

typedef struct {
  uint8_t **keys;
  size_t *lens;
  size_t count;
  size_t cap;
} key_list_t;

typedef struct {
  db_t *db;
  const db_selector_t *selector;
  db_record_cb cb;
  void *cb_ctx;
} iterate_ctx_t;

typedef struct {
  db_t *db;
  key_list_t *keys;
  const db_selector_t *selector;
} collect_ctx_t;

typedef struct {
  void *val;
  size_t vlen;
  int found;
} get_ctx_t;

static int ht_iter_cb_snapshot(void *ctx, const void *key, uint32_t klen,
                               const void *val, uint32_t vlen);
static int db_rebuild_indexes(db_t *db);

static char *db_build_peer_path(const char *path, const char *name) {
  const char *slash = strrchr(path, '/');
  size_t dir_len = slash ? (size_t)(slash - path + 1) : 0;
  size_t name_len = strlen(name);
  char *out = (char *)xmalloc(dir_len + name_len + 1);

  if (dir_len)
    memcpy(out, path, dir_len);
  memcpy(out + dir_len, name, name_len + 1);
  return out;
}

static char *db_make_tmp_path(const char *path) {
  static const char suffix[] = ".tmp";
  size_t path_len = strlen(path);
  char *out = (char *)xmalloc(path_len + sizeof(suffix));
  memcpy(out, path, path_len);
  memcpy(out + path_len, suffix, sizeof(suffix));
  return out;
}

static int db_sync_parent_dir(const char *path) {
  const char *slash = strrchr(path, '/');
  char *dir_path = NULL;
  int dirfd = -1;
  int rc = -1;

  if (slash) {
    size_t dir_len = (size_t)(slash - path);
    dir_path = (char *)xmalloc(dir_len + 1);
    memcpy(dir_path, path, dir_len);
    dir_path[dir_len] = '\0';
  }

  dirfd = open(dir_path ? dir_path : ".", O_DIRECTORY | O_RDONLY);
  if (dirfd < 0)
    goto done;
  if (fsync(dirfd) != 0)
    goto done;

  rc = 0;

done:
  if (dirfd >= 0)
    close(dirfd);
  if (dir_path)
    xfree(dir_path);
  return rc;
}

static int db_sync_if_needed(db_t *db) {
  if (db->sync_each_write) {
    if (fsync(db->fd) < 0)
      return -1;
    db->wal_syncs++;
    db->ops_since_sync = 0;
    return 0;
  }

  db->ops_since_sync++;
  if (db->sync_every_n > 0 &&
      db->ops_since_sync >= (uint64_t)db->sync_every_n) {
    if (fsync(db->fd) < 0)
      return -1;
    db->wal_syncs++;
    db->ops_since_sync = 0;
  }

  return 0;
}

static int db_ensure_blob_dir(db_t *db) {
  if (mkdir(db->blob_dir, 0755) != 0 && errno != EEXIST)
    return -1;
  return 0;
}

static int record_decode(const void *buf, size_t len, record_wire_t *out) {
  const uint8_t *p = (const uint8_t *)buf;
  uint32_t version;
  uint32_t flags;
  uint32_t namespace_len;
  uint32_t content_type_len;
  uint32_t blob_id_len;
  uint32_t data_len;
  size_t expect;

  if (len < DB_RECORD_WIRE_HDR_BYTES)
    return -1;

  version = le_to_u32(p + 0);
  flags = le_to_u32(p + 4);
  namespace_len = le_to_u32(p + 8);
  content_type_len = le_to_u32(p + 12);
  blob_id_len = le_to_u32(p + 16);
  data_len = le_to_u32(p + 20);

  if (version != DB_RECORD_WIRE_VERSION)
    return -1;

  expect = DB_RECORD_WIRE_HDR_BYTES + (size_t)namespace_len +
           (size_t)content_type_len + (size_t)blob_id_len;
  if (!(flags & DB_RECORD_BLOB))
    expect += (size_t)data_len;
  if (expect != len)
    return -1;

  p += DB_RECORD_WIRE_HDR_BYTES;
  out->flags = flags;
  out->namespace_name = p;
  out->namespace_len = namespace_len;
  p += namespace_len;
  out->content_type = p;
  out->content_type_len = content_type_len;
  p += content_type_len;
  out->blob_id = p;
  out->blob_id_len = blob_id_len;
  p += blob_id_len;
  out->data = p;
  out->data_len = data_len;
  return 0;
}

static int record_matches_selector(const void *key, size_t klen,
                                   const record_wire_t *wire,
                                   const db_selector_t *selector) {
  if (!selector)
    return 1;
  if (selector->key &&
      !(selector->klen == klen && memcmp(selector->key, key, klen) == 0))
    return 0;
  if (selector->namespace_name) {
    size_t nlen = strlen(selector->namespace_name);
    if (!(wire->namespace_len == nlen &&
          memcmp(wire->namespace_name, selector->namespace_name, nlen) == 0))
      return 0;
  }
  if (selector->content_type) {
    size_t tlen = strlen(selector->content_type);
    if (!(wire->content_type_len == tlen &&
          memcmp(wire->content_type, selector->content_type, tlen) == 0))
      return 0;
  }
  if ((wire->flags & selector->flags_mask) != selector->flags_value)
    return 0;
  return 1;
}

static int key_list_push(key_list_t *list, const void *key, size_t klen) {
  uint8_t *copy;
  if (list->count == list->cap) {
    size_t new_cap = list->cap ? list->cap * 2 : 8;
    list->keys =
        (uint8_t **)xrealloc(list->keys, new_cap * sizeof(*list->keys));
    list->lens = (size_t *)xrealloc(list->lens, new_cap * sizeof(*list->lens));
    list->cap = new_cap;
  }
  copy = (uint8_t *)xmalloc(klen ? klen : 1);
  if (klen)
    memcpy(copy, key, klen);
  list->keys[list->count] = copy;
  list->lens[list->count] = klen;
  list->count++;
  return 0;
}

static void key_list_destroy(key_list_t *list) {
  size_t i;
  for (i = 0; i < list->count; i++)
    xfree(list->keys[i]);
  xfree(list->keys);
  xfree(list->lens);
  memset(list, 0, sizeof *list);
}

static char *db_hex_id(const void *key, size_t klen, const char *ns,
                       const char *ct, uint32_t flags, const void *val,
                       size_t vlen) {
  static const char hex[] = "0123456789abcdef";
  uint32_t h1 = fnv1a_32(key, klen);
  uint32_t h2;
  uint32_t crc;
  char *out = (char *)xmalloc(25);
  int i;

  crc32_init();
  crc = crc32_begin();
  if (klen)
    crc = crc32_update(crc, key, klen);
  if (ns)
    crc = crc32_update(crc, ns, strlen(ns));
  if (ct)
    crc = crc32_update(crc, ct, strlen(ct));
  crc = crc32_update(crc, &flags, sizeof flags);
  if (vlen)
    crc = crc32_update(crc, val, vlen);
  crc = crc32_end(crc);
  h2 = fnv1a_32(ct ? ct : "", ct ? strlen(ct) : 0) ^ (uint32_t)vlen;

  for (i = 0; i < 8; i++)
    out[i] = hex[(h1 >> (28 - i * 4)) & 0xFu];
  for (i = 0; i < 8; i++)
    out[8 + i] = hex[(crc >> (28 - i * 4)) & 0xFu];
  for (i = 0; i < 8; i++)
    out[16 + i] = hex[(h2 >> (28 - i * 4)) & 0xFu];
  out[24] = '\0';
  return out;
}

static char *db_blob_path(const db_t *db, const uint8_t *blob_id,
                          size_t blob_id_len) {
  size_t dir_len = strlen(db->blob_dir);
  char *path = (char *)xmalloc(dir_len + 1 + blob_id_len + 1);
  memcpy(path, db->blob_dir, dir_len);
  path[dir_len] = '/';
  memcpy(path + dir_len + 1, blob_id, blob_id_len);
  path[dir_len + 1 + blob_id_len] = '\0';
  return path;
}

static int db_write_blob_file(db_t *db, const char *blob_id, const void *data,
                              size_t dlen) {
  char *path = db_blob_path(db, (const uint8_t *)blob_id, strlen(blob_id));
  char *tmp_path = db_make_tmp_path(path);
  int fd = -1;
  int rc = -1;

  if (db_ensure_blob_dir(db) != 0)
    goto done;

  fd = open(tmp_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0)
    goto done;
  if (dlen && write_full(fd, data, dlen) != 0)
    goto done;
  if (fdatasync(fd) != 0)
    goto done;
  if (close(fd) != 0) {
    fd = -1;
    goto done;
  }
  fd = -1;

  if (rename(tmp_path, path) != 0)
    goto done;
  if (db_sync_parent_dir(path) != 0)
    goto done;

  rc = 0;

done:
  if (fd >= 0)
    close(fd);
  if (rc != 0)
    (void)unlink(tmp_path);
  xfree(path);
  xfree(tmp_path);
  return rc;
}

static int db_read_blob_file(db_t *db, const uint8_t *blob_id,
                             size_t blob_id_len, void **out_data,
                             size_t *out_len) {
  char *path = db_blob_path(db, blob_id, blob_id_len);
  struct stat st;
  int fd = -1;
  void *buf = NULL;
  int rc = -1;

  fd = open(path, O_RDONLY);
  if (fd < 0)
    goto done;
  if (fstat(fd, &st) != 0)
    goto done;
  if (st.st_size < 0)
    goto done;
  buf = xmalloc((size_t)st.st_size ? (size_t)st.st_size : 1);
  if (st.st_size > 0 && read_full(fd, buf, (size_t)st.st_size) != 0)
    goto done;

  *out_data = buf;
  *out_len = (size_t)st.st_size;
  buf = NULL;
  rc = 0;

done:
  if (fd >= 0)
    close(fd);
  if (buf)
    xfree(buf);
  xfree(path);
  return rc;
}

static void db_delete_blob_file(db_t *db, const uint8_t *blob_id,
                                size_t blob_id_len) {
  char *path = db_blob_path(db, blob_id, blob_id_len);
  if (unlink(path) == 0)
    (void)db_sync_parent_dir(path);
  xfree(path);
}

static int record_encode(const db_record_meta_t *meta, const void *key,
                         size_t klen, const void *val, size_t vlen,
                         size_t inline_threshold, encoded_record_t *out,
                         char **out_blob_id) {
  const char *namespace_name = (meta && meta->namespace_name)
                                   ? meta->namespace_name
                                   : DB_DEFAULT_NAMESPACE;
  const char *content_type = (meta && meta->content_type)
                                 ? meta->content_type
                                 : DB_DEFAULT_CONTENT_TYPE;
  uint32_t flags = meta ? meta->flags : 0;
  int use_blob = (flags & DB_RECORD_BLOB) != 0 || vlen > inline_threshold;
  size_t namespace_len = strlen(namespace_name);
  size_t content_type_len = strlen(content_type);
  char *blob_id = NULL;
  size_t blob_id_len = 0;
  size_t inline_len = use_blob ? 0 : vlen;
  uint8_t *buf;
  uint8_t *p;

  if (namespace_len > 0xFFFFFFFFu || content_type_len > 0xFFFFFFFFu ||
      vlen > 0xFFFFFFFFu)
    return -1;

  if (use_blob) {
    flags |= DB_RECORD_BLOB;
    blob_id =
        db_hex_id(key, klen, namespace_name, content_type, flags, val, vlen);
    blob_id_len = strlen(blob_id);
  }

  buf = (uint8_t *)xmalloc(DB_RECORD_WIRE_HDR_BYTES + namespace_len +
                           content_type_len + blob_id_len + inline_len);
  p = buf;
  u32_to_le(DB_RECORD_WIRE_VERSION, p);
  p += 4;
  u32_to_le(flags, p);
  p += 4;
  u32_to_le((uint32_t)namespace_len, p);
  p += 4;
  u32_to_le((uint32_t)content_type_len, p);
  p += 4;
  u32_to_le((uint32_t)blob_id_len, p);
  p += 4;
  u32_to_le((uint32_t)vlen, p);
  p += 4;

  if (namespace_len) {
    memcpy(p, namespace_name, namespace_len);
    p += namespace_len;
  }
  if (content_type_len) {
    memcpy(p, content_type, content_type_len);
    p += content_type_len;
  }
  if (blob_id_len) {
    memcpy(p, blob_id, blob_id_len);
    p += blob_id_len;
  }
  if (inline_len)
    memcpy(p, val, inline_len);

  out->buf = buf;
  out->len = DB_RECORD_WIRE_HDR_BYTES + namespace_len + content_type_len +
             blob_id_len + inline_len;
  if (out_blob_id)
    *out_blob_id = blob_id;
  else if (blob_id)
    xfree(blob_id);
  return 0;
}

static int index_bucket_add(ht_t *index, const void *name, size_t name_len,
                            const void *key, size_t klen) {
  const void *old = NULL;
  size_t old_len = 0;
  const uint8_t *p;
  uint8_t *buf;
  size_t off = 0;

  if (ht_get(index, name, name_len, &old, &old_len) == 0) {
    p = (const uint8_t *)old;
    while (off < old_len) {
      uint32_t item_len;
      if (old_len - off < 4)
        return -1;
      item_len = le_to_u32(p + off);
      off += 4;
      if (old_len - off < item_len)
        return -1;
      if (item_len == klen && memcmp(p + off, key, klen) == 0)
        return 0;
      off += item_len;
    }
  }

  buf = (uint8_t *)xmalloc(old_len + 4 + klen);
  if (old_len)
    memcpy(buf, old, old_len);
  u32_to_le((uint32_t)klen, buf + old_len);
  if (klen)
    memcpy(buf + old_len + 4, key, klen);

  if (ht_set(index, name, name_len, buf, old_len + 4 + klen) != 0) {
    xfree(buf);
    return -1;
  }
  xfree(buf);
  return 0;
}

static int index_bucket_remove(ht_t *index, const void *name, size_t name_len,
                               const void *key, size_t klen) {
  const void *old = NULL;
  size_t old_len = 0;
  const uint8_t *p;
  uint8_t *buf = NULL;
  size_t off = 0;
  size_t out_off = 0;
  int removed = 0;

  if (ht_get(index, name, name_len, &old, &old_len) != 0)
    return 0;

  p = (const uint8_t *)old;
  buf = (uint8_t *)xmalloc(old_len ? old_len : 1);
  while (off < old_len) {
    uint32_t item_len;
    if (old_len - off < 4)
      goto fail;
    item_len = le_to_u32(p + off);
    if (old_len - off - 4 < item_len)
      goto fail;
    if (item_len == klen && memcmp(p + off + 4, key, klen) == 0) {
      removed = 1;
      off += 4 + item_len;
      continue;
    }
    memcpy(buf + out_off, p + off, 4 + item_len);
    out_off += 4 + item_len;
    off += 4 + item_len;
  }

  if (!removed) {
    xfree(buf);
    return 0;
  }
  if (out_off == 0) {
    xfree(buf);
    return ht_del(index, name, name_len) == 0 ? 0 : 0;
  }
  if (ht_set(index, name, name_len, buf, out_off) != 0)
    goto fail;
  xfree(buf);
  return 0;

fail:
  if (buf)
    xfree(buf);
  return -1;
}

static int index_iterate(ht_t *index, const void *name, size_t name_len,
                         int (*cb)(void *ctx, const void *key, size_t klen),
                         void *ctx) {
  const void *bucket = NULL;
  size_t bucket_len = 0;
  const uint8_t *p;
  size_t off = 0;

  if (ht_get(index, name, name_len, &bucket, &bucket_len) != 0)
    return 0;

  p = (const uint8_t *)bucket;
  while (off < bucket_len) {
    uint32_t item_len;
    if (bucket_len - off < 4)
      return -1;
    item_len = le_to_u32(p + off);
    off += 4;
    if (bucket_len - off < item_len)
      return -1;
    if (cb(ctx, p + off, item_len) != 0)
      return -1;
    off += item_len;
  }
  return 0;
}

static int db_index_record(db_t *db, const void *key, size_t klen,
                           const record_wire_t *wire) {
  if (index_bucket_add(&db->namespace_index, wire->namespace_name,
                       wire->namespace_len, key, klen) != 0)
    return -1;
  if (index_bucket_add(&db->content_type_index, wire->content_type,
                       wire->content_type_len, key, klen) != 0)
    return -1;
  return 0;
}

static int db_unindex_record(db_t *db, const void *key, size_t klen,
                             const record_wire_t *wire) {
  if (index_bucket_remove(&db->namespace_index, wire->namespace_name,
                          wire->namespace_len, key, klen) != 0)
    return -1;
  if (index_bucket_remove(&db->content_type_index, wire->content_type,
                          wire->content_type_len, key, klen) != 0)
    return -1;
  return 0;
}

static int db_invoke_record_cb(db_t *db, const void *key, size_t klen,
                               const record_wire_t *wire, int skip_blob,
                               db_record_cb cb, void *cb_ctx) {
  db_record_view_t view;
  void *owned_blob = NULL;
  size_t blob_len = 0;

  if (!cb)
    return 0;

  memset(&view, 0, sizeof view);
  view.key = key;
  view.klen = klen;
  view.namespace_name = (const char *)wire->namespace_name;
  view.namespace_len = wire->namespace_len;
  view.content_type = (const char *)wire->content_type;
  view.content_type_len = wire->content_type_len;
  view.flags = wire->flags;
  view.blob_id = (const char *)wire->blob_id;
  view.blob_id_len = wire->blob_id_len;

  if (wire->flags & DB_RECORD_BLOB) {
    if (skip_blob) {
      view.data = NULL;
      view.dlen = 0;
    } else if (db_read_blob_file(db, wire->blob_id, wire->blob_id_len,
                                 &owned_blob, &blob_len) != 0) {
      return -1;
    } else {
      view.data = owned_blob;
      view.dlen = blob_len;
    }
  } else {
    view.data = wire->data;
    view.dlen = wire->data_len;
  }

  if (cb(cb_ctx, &view) != 0) {
    if (owned_blob)
      xfree(owned_blob);
    return -1;
  }

  if (owned_blob)
    xfree(owned_blob);
  return 0;
}

static int db_iterate_primary_cb(void *ctx, const void *key, uint32_t klen,
                                 const void *val, uint32_t vlen) {
  iterate_ctx_t *it = (iterate_ctx_t *)ctx;
  record_wire_t wire;
  int skip_blob = it->selector ? it->selector->skip_blob : 0;

  if (record_decode(val, vlen, &wire) != 0)
    return -1;
  if (!record_matches_selector(key, klen, &wire, it->selector))
    return 0;
  return db_invoke_record_cb(it->db, key, klen, &wire, skip_blob, it->cb,
                             it->cb_ctx);
}

static int db_collect_candidate_cb(void *ctx, const void *key, size_t klen) {
  collect_ctx_t *cc = (collect_ctx_t *)ctx;
  const void *val = NULL;
  size_t vlen = 0;
  record_wire_t wire;

  if (ht_get(&cc->db->index, key, klen, &val, &vlen) != 0)
    return -1;
  if (record_decode(val, vlen, &wire) != 0)
    return -1;
  if (!record_matches_selector(key, klen, &wire, cc->selector))
    return 0;
  return key_list_push(cc->keys, key, klen);
}

static int db_collect_all_cb(void *ctx, const void *key, uint32_t klen,
                             const void *val, uint32_t vlen) {
  collect_ctx_t *cc = (collect_ctx_t *)ctx;
  record_wire_t wire;
  if (record_decode(val, vlen, &wire) != 0)
    return -1;
  if (!record_matches_selector(key, klen, &wire, cc->selector))
    return 0;
  return key_list_push(cc->keys, key, klen);
}

static int db_collect_keys(db_t *db, const db_selector_t *selector,
                           key_list_t *out) {
  collect_ctx_t ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.db = db;
  ctx.keys = out;
  ctx.selector = selector;

  if (selector && selector->key)
    return key_list_push(out, selector->key, selector->klen);
  if (selector && selector->namespace_name)
    return index_iterate(&db->namespace_index, selector->namespace_name,
                         strlen(selector->namespace_name),
                         db_collect_candidate_cb, &ctx);
  if (selector && selector->content_type)
    return index_iterate(&db->content_type_index, selector->content_type,
                         strlen(selector->content_type),
                         db_collect_candidate_cb, &ctx);
  return ht_iterate(&db->index, db_collect_all_cb, &ctx);
}

static int snapshot_flush_page(snapshot_writer_t *wr) {
  if (!wr->has_page)
    return 0;
  if (pf_write_page(&wr->pf, wr->pid, &wr->page) != 0)
    return -1;
  return 0;
}

static int ht_iter_cb_snapshot(void *ctx, const void *key, uint32_t klen,
                               const void *val, uint32_t vlen) {
  snapshot_writer_t *wr = (snapshot_writer_t *)ctx;
  uint64_t rec_len64 = 12ull + (uint64_t)klen + (uint64_t)vlen;
  uint8_t *rec = NULL;
  uint8_t *p;
  uint32_t crc;

  if (rec_len64 > 0xFFFFu)
    return -1;

  rec = (uint8_t *)xmalloc((size_t)rec_len64);
  p = rec;
  u32_to_le(klen, p);
  p += 4;
  u32_to_le(vlen, p);
  p += 4;

  crc32_init();
  crc = crc32_begin();
  if (klen)
    crc = crc32_update(crc, key, klen);
  if (vlen)
    crc = crc32_update(crc, val, vlen);
  crc = crc32_end(crc);

  u32_to_le(crc, p);
  p += 4;
  if (klen)
    memcpy(p, key, klen);
  p += klen;
  if (vlen)
    memcpy(p, val, vlen);

  if (!wr->has_page) {
    if (pf_alloc_page(&wr->pf, &wr->pid, &wr->page) != 0)
      goto fail;
    wr->has_page = 1;
  }

  if (page_try_insert(&wr->page, rec, (uint16_t)rec_len64, NULL) != 0) {
    if (snapshot_flush_page(wr) != 0)
      goto fail;
    if (pf_alloc_page(&wr->pf, &wr->pid, &wr->page) != 0)
      goto fail;
    wr->has_page = 1;
    if (page_try_insert(&wr->page, rec, (uint16_t)rec_len64, NULL) != 0)
      goto fail;
  }

  xfree(rec);
  return 0;

fail:
  if (rec)
    xfree(rec);
  return -1;
}

static int db_write_snapshot(const char *path, const ht_t *ht) {
  snapshot_writer_t wr;
  memset(&wr, 0, sizeof wr);

  if (pf_open(&wr.pf, path, 1) != 0)
    return -1;
  if (ht_iterate(ht, ht_iter_cb_snapshot, &wr) != 0) {
    pf_close(&wr.pf);
    return -1;
  }
  if (snapshot_flush_page(&wr) != 0) {
    pf_close(&wr.pf);
    return -1;
  }
  if (pf_sync(&wr.pf) != 0) {
    pf_close(&wr.pf);
    return -1;
  }
  if (pf_close(&wr.pf) != 0)
    return -1;
  return 0;
}

static int db_index_rebuild_cb(void *ctx, const void *key, uint32_t klen,
                               const void *val, uint32_t vlen) {
  db_t *db = (db_t *)ctx;
  record_wire_t wire;

  if (record_decode(val, vlen, &wire) != 0)
    return -1;
  return db_index_record(db, key, klen, &wire);
}

static int db_rebuild_indexes(db_t *db) {
  ht_destroy(&db->namespace_index);
  ht_destroy(&db->content_type_index);
  if (ht_init(&db->namespace_index, 128) != 0)
    return -1;
  if (ht_init(&db->content_type_index, 128) != 0)
    return -1;
  return ht_iterate(&db->index, db_index_rebuild_cb, db);
}

static int on_set_cb(void *ctx, const void *key, uint32_t klen, const void *val,
                     uint32_t vlen) {
  db_t *db = (db_t *)ctx;
  return ht_set(&db->index, key, klen, val, vlen);
}

static int on_del_cb(void *ctx, const void *key, uint32_t klen) {
  db_t *db = (db_t *)ctx;
  (void)ht_del(&db->index, key, klen);
  return 0;
}

int db_open(db_t *db, const char *wal_path, int sync_each_write) {
  memset(db, 0, sizeof *db);
  db->fd = -1;
  db->sync_each_write = sync_each_write ? 1 : 0;
  db->sync_every_n = db->sync_each_write ? 0 : 5;
  db->inline_threshold = DB_DEFAULT_INLINE_THRESHOLD;

  db->path = (char *)xmalloc(strlen(wal_path) + 1);
  strcpy(db->path, wal_path);
  db->base_path = db_build_peer_path(wal_path, "base.db");
  db->blob_dir = db_build_peer_path(wal_path, "blobs");

  db->fd = open(wal_path, O_CREAT | O_RDWR, 0644);
  if (db->fd < 0)
    return -1;
  if (db_ensure_blob_dir(db) != 0)
    return -2;

  if (ht_init(&db->index, 1024) != 0)
    return -3;
  if (ht_init(&db->namespace_index, 128) != 0)
    return -4;
  if (ht_init(&db->content_type_index, 128) != 0)
    return -5;

  if (mmap_load_into_ht(db->base_path, &db->index) != 0)
    return -6;
  if (lseek(db->fd, 0, SEEK_SET) < 0)
    return -7;
  if (wal_replay(db->fd, db, on_set_cb, on_del_cb) != 0)
    return -8;
  if (lseek(db->fd, 0, SEEK_END) < 0)
    return -9;
  if (db_rebuild_indexes(db) != 0)
    return -10;

  return 0;
}

void db_close(db_t *db) {
  if (!db)
    return;
  if (db->fd >= 0 && db->ops_since_sync > 0) {
    (void)fsync(db->fd);
    db->wal_syncs++;
    db->ops_since_sync = 0;
  }
  ht_destroy(&db->index);
  ht_destroy(&db->namespace_index);
  ht_destroy(&db->content_type_index);
  if (db->fd >= 0)
    close(db->fd);
  if (db->path)
    xfree(db->path);
  if (db->base_path)
    xfree(db->base_path);
  if (db->blob_dir)
    xfree(db->blob_dir);
  memset(db, 0, sizeof *db);
  db->fd = -1;
}

int db_write(db_t *db, const void *key, size_t klen,
             const db_record_meta_t *meta, const void *val, size_t vlen,
             db_record_cb cb, void *cb_ctx) {
  encoded_record_t encoded;
  char *blob_id = NULL;
  const void *old_val = NULL;
  size_t old_vlen = 0;
  record_wire_t new_wire;
  record_wire_t old_wire;
  int had_old = 0;
  int rc = -1;

  memset(&encoded, 0, sizeof encoded);
  memset(&new_wire, 0, sizeof new_wire);
  memset(&old_wire, 0, sizeof old_wire);

  if (record_encode(meta, key, klen, val, vlen, db->inline_threshold, &encoded,
                    &blob_id) != 0)
    goto done;
  if (record_decode(encoded.buf, encoded.len, &new_wire) != 0)
    goto done;
  if ((new_wire.flags & DB_RECORD_BLOB) &&
      db_write_blob_file(db, blob_id, val, vlen) != 0)
    goto done;

  if (ht_get(&db->index, key, klen, &old_val, &old_vlen) == 0) {
    if (record_decode(old_val, old_vlen, &old_wire) != 0)
      goto done;
    had_old = 1;
  }

  if (wal_append_set(db->fd, key, klen, encoded.buf, encoded.len, 0) != 0)
    goto done;
  if (ht_set(&db->index, key, klen, encoded.buf, encoded.len) != 0)
    goto done;
  if (had_old && db_unindex_record(db, key, klen, &old_wire) != 0)
    goto rebuild_fail;
  if (db_index_record(db, key, klen, &new_wire) != 0)
    goto rebuild_fail;
  if (db_sync_if_needed(db) != 0)
    goto done;
  if (had_old && (old_wire.flags & DB_RECORD_BLOB) &&
      !(old_wire.blob_id_len == new_wire.blob_id_len &&
        memcmp(old_wire.blob_id, new_wire.blob_id, old_wire.blob_id_len) == 0))
    db_delete_blob_file(db, old_wire.blob_id, old_wire.blob_id_len);
  if (db_invoke_record_cb(db, key, klen, &new_wire, 0, cb, cb_ctx) != 0)
    goto done;
  rc = 0;
  goto done;

rebuild_fail:
  (void)db_rebuild_indexes(db);

done:
  if (rc != 0 && blob_id)
    db_delete_blob_file(db, (const uint8_t *)blob_id, strlen(blob_id));
  if (blob_id)
    xfree(blob_id);
  if (encoded.buf)
    xfree(encoded.buf);
  return rc;
}

int db_read(db_t *db, const db_selector_t *selector, db_record_cb cb,
            void *cb_ctx) {
  iterate_ctx_t ctx;
  key_list_t keys;
  size_t i;

  if (selector && selector->key) {
    const void *val = NULL;
    size_t vlen = 0;
    record_wire_t wire;
    if (ht_get(&db->index, selector->key, selector->klen, &val, &vlen) != 0)
      return 0;
    if (record_decode(val, vlen, &wire) != 0)
      return -1;
    if (!record_matches_selector(selector->key, selector->klen, &wire,
                                 selector))
      return 0;
    return db_invoke_record_cb(db, selector->key, selector->klen, &wire,
                               selector->skip_blob, cb, cb_ctx);
  }

  if (selector && (selector->namespace_name || selector->content_type)) {
    memset(&keys, 0, sizeof keys);
    if (db_collect_keys(db, selector, &keys) != 0) {
      key_list_destroy(&keys);
      return -1;
    }
    for (i = 0; i < keys.count; i++) {
      db_selector_t exact = *selector;
      exact.key = keys.keys[i];
      exact.klen = keys.lens[i];
      if (db_read(db, &exact, cb, cb_ctx) != 0) {
        key_list_destroy(&keys);
        return -1;
      }
    }
    key_list_destroy(&keys);
    return 0;
  }

  memset(&ctx, 0, sizeof ctx);
  ctx.db = db;
  ctx.selector = selector;
  ctx.cb = cb;
  ctx.cb_ctx = cb_ctx;
  return ht_iterate(&db->index, db_iterate_primary_cb, &ctx);
}

int db_delete_where(db_t *db, const db_selector_t *selector, db_record_cb cb,
                    void *cb_ctx) {
  key_list_t keys;
  size_t i;

  memset(&keys, 0, sizeof keys);
  if (db_collect_keys(db, selector, &keys) != 0) {
    key_list_destroy(&keys);
    return -1;
  }

  for (i = 0; i < keys.count; i++) {
    const void *val = NULL;
    size_t vlen = 0;
    record_wire_t wire;

    if (ht_get(&db->index, keys.keys[i], keys.lens[i], &val, &vlen) != 0)
      continue;
    if (record_decode(val, vlen, &wire) != 0) {
      key_list_destroy(&keys);
      return -1;
    }
    if (!record_matches_selector(keys.keys[i], keys.lens[i], &wire, selector))
      continue;
    if (db_invoke_record_cb(db, keys.keys[i], keys.lens[i], &wire,
                            selector ? selector->skip_blob : 0, cb,
                            cb_ctx) != 0) {
      key_list_destroy(&keys);
      return -1;
    }
    if (wal_append_del(db->fd, keys.keys[i], keys.lens[i], 0) != 0) {
      key_list_destroy(&keys);
      return -1;
    }
    if (ht_del(&db->index, keys.keys[i], keys.lens[i]) != 0) {
      key_list_destroy(&keys);
      return -1;
    }
    if (db_unindex_record(db, keys.keys[i], keys.lens[i], &wire) != 0)
      (void)db_rebuild_indexes(db);
    if (db_sync_if_needed(db) != 0) {
      key_list_destroy(&keys);
      return -1;
    }
    if (wire.flags & DB_RECORD_BLOB)
      db_delete_blob_file(db, wire.blob_id, wire.blob_id_len);
  }

  key_list_destroy(&keys);
  return 0;
}

int db_compact(db_t *db) {
  char *tmp_path;
  int rc = -1;

  if (!db || !db->base_path)
    return -1;
  if (db->ops_since_sync > 0) {
    if (fdatasync(db->fd) < 0)
      return -1;
    db->wal_syncs++;
    db->ops_since_sync = 0;
  }

  tmp_path = db_make_tmp_path(db->base_path);
  if (db_write_snapshot(tmp_path, &db->index) != 0)
    goto done;
  if (rename(tmp_path, db->base_path) != 0)
    goto done;
  if (db_sync_parent_dir(db->base_path) != 0)
    goto done;
  if (ftruncate(db->fd, 0) != 0)
    goto done;
  if (lseek(db->fd, 0, SEEK_SET) < 0)
    goto done;
  if (fdatasync(db->fd) != 0)
    goto done;
  db->wal_syncs++;
  if (lseek(db->fd, 0, SEEK_END) < 0)
    goto done;

  rc = 0;

done:
  if (rc != 0 && tmp_path)
    (void)unlink(tmp_path);
  if (tmp_path)
    xfree(tmp_path);
  return rc;
}

static int db_get_cb(void *ctx, const db_record_view_t *record) {
  get_ctx_t *gc = (get_ctx_t *)ctx;
  gc->val = xmalloc(record->dlen ? record->dlen : 1);
  if (record->dlen)
    memcpy(gc->val, record->data, record->dlen);
  gc->vlen = record->dlen;
  gc->found = 1;
  return 0;
}

int db_set(db_t *db, const void *key, size_t klen, const void *val,
           size_t vlen) {
  db_record_meta_t meta;
  meta.namespace_name = DB_DEFAULT_NAMESPACE;
  meta.content_type = DB_DEFAULT_CONTENT_TYPE;
  meta.flags = 0;
  return db_write(db, key, klen, &meta, val, vlen, NULL, NULL);
}

int db_get(db_t *db, const void *key, size_t klen, void **out_val,
           size_t *out_vlen) {
  db_selector_t selector;
  get_ctx_t ctx;
  memset(&selector, 0, sizeof selector);
  memset(&ctx, 0, sizeof ctx);
  selector.key = key;
  selector.klen = klen;
  if (db_read(db, &selector, db_get_cb, &ctx) != 0 || !ctx.found)
    return -1;
  *out_val = ctx.val;
  *out_vlen = ctx.vlen;
  return 0;
}

int db_del(db_t *db, const void *key, size_t klen) {
  db_selector_t selector;
  memset(&selector, 0, sizeof selector);
  selector.key = key;
  selector.klen = klen;
  return db_delete_where(db, &selector, NULL, NULL);
}
