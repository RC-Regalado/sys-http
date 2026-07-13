#pragma once
#include "hash.h"
#include <stddef.h>
#include <stdint.h>

#define DB_RECORD_BLOB 0x00000001u
#define DB_RECORD_JSON 0x00000002u
#define DB_RECORD_PROTOBUF 0x00000004u
#define DB_RECORD_FILE 0x00000008u

typedef struct {
  const char *namespace_name;
  const char *content_type;
  uint32_t flags;
} db_record_meta_t;

typedef struct {
  const void *key;
  size_t klen;
  const char *namespace_name;
  size_t namespace_len;
  const char *content_type;
  size_t content_type_len;
  uint32_t flags;
  const char *blob_id;
  size_t blob_id_len;
  const void *data;
  size_t dlen;
} db_record_view_t;

typedef struct {
  const void *key;
  size_t klen;
  const char *namespace_name;
  const char *content_type;
  uint32_t flags_mask;
  uint32_t flags_value;
  /* Si es != 0, omite cargar a memoria el contenido de registros
   * blob (DB_RECORD_BLOB); util para listar/contar por namespace sin
   * pagar disco+xmalloc por cada blob. Registros inline no se ven
   * afectados. db_record_view_t.data queda NULL/dlen=0 para blobs. */
  int skip_blob;
} db_selector_t;

typedef int (*db_record_cb)(void *ctx, const db_record_view_t *record);

typedef struct {
  int fd;
  char *path;
  char *base_path;
  char *blob_dir;

  uint8_t *base_map;
  size_t base_map_size;

  int sync_each_write;
  int sync_every_n;
  uint64_t ops_since_sync;

  uint64_t wal_bytes;
  uint64_t wal_writes;
  uint64_t wal_syncs;

  size_t inline_threshold;

  ht_t index;
  ht_t namespace_index;
  ht_t content_type_index;
} db_t;

int db_open(db_t *db, const char *wal_path, int sync_each_write);
void db_close(db_t *db);

int db_write(db_t *db, const void *key, size_t klen, const db_record_meta_t *meta,
             const void *val, size_t vlen, db_record_cb cb, void *cb_ctx);
int db_read(db_t *db, const db_selector_t *selector, db_record_cb cb,
            void *cb_ctx);
int db_delete_where(db_t *db, const db_selector_t *selector, db_record_cb cb,
                    void *cb_ctx);

int db_compact(db_t *db);

int db_set(db_t *db, const void *key, size_t klen, const void *val,
           size_t vlen);
int db_get(db_t *db, const void *key, size_t klen, void **out_val,
           size_t *out_vlen);
int db_del(db_t *db, const void *key, size_t klen);
