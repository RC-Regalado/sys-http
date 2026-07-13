#include "database.h"

#include "io.h"
#include "json.h"
#include "requests.h"
#include "str.h"

#include "db.h"

#define DATABASE_WAL_PATH "./data.wal"
#define DATABASE_NAMESPACE "database"
#define NOTES_NAMESPACE "notes"
#define DATABASE_LIST_ID_CAP 256

static int database_open(db_t *db) {
  return db_open(db, DATABASE_WAL_PATH, 0);
}

static int load_record(void *ctx, const db_record_view_t *record) {
  json_object *obj = (json_object *)ctx;

  json_add_bool(obj, "found", 1);

  if (record->flags & DB_RECORD_JSON) {
    json_add_object(obj, "value", record->data);
    json_add_bool(obj, "json", 1);

    return 0;
  }

  json_add_string(obj, "value", record->data);
  json_add_bool(obj, "json", 0);
  return 0;
}

int database_set(const char *key, const char *payload,
                 unsigned int payload_len) {

  char *namespace_name = DATABASE_NAMESPACE;
  char *content_type = "json";
  db_record_meta_t meta;
  db_t db;
  int result;

  if (database_open(&db))
    return 1;

  if (!key || !namespace_name || !content_type || !payload) {
    db_close(&db);
    return 1;
  }

  meta.namespace_name = namespace_name;
  meta.content_type = content_type;
  meta.flags = 0;
  meta.flags |= DB_RECORD_JSON;

  result =
      db_write(&db, key, len(key), &meta, payload, payload_len, NULL, NULL);
  db_close(&db);
  return result;
}

typedef struct {
  long count;
} count_ctx_t;

static int count_record(void *ctx, const db_record_view_t *record) {
  count_ctx_t *cc = (count_ctx_t *)ctx;
  cc->count++;
  return 0;
}

int database_add_note(const char *markdown, unsigned int markdown_len,
                      string_pool *out_key) {
  db_record_meta_t meta;
  db_selector_t selector;
  count_ctx_t ctx;
  db_t db;
  char *key;
  int result;

  if (!markdown || markdown_len == 0 || !out_key)
    return 1;

  if (database_open(&db))
    return 1;

  selector.key = 0;
  selector.klen = 0;
  selector.namespace_name = NOTES_NAMESPACE;
  selector.content_type = 0;
  selector.flags_mask = 0;
  selector.flags_value = 0;
  selector.skip_blob = 1; // count_record no usa record->data

  ctx.count = 0;
  if (db_read(&db, &selector, count_record, &ctx) < 0) {
    db_close(&db);
    return 1;
  }

  string_pool_reset(out_key);
  if (string_pool_format(out_key, "note-%ld", ctx.count + 1) < 0) {
    db_close(&db);
    return 1;
  }
  key = out_key->base;

  meta.namespace_name = NOTES_NAMESPACE;
  meta.content_type = "text/markdown";
  meta.flags = 0;

  result =
      db_write(&db, key, len(key), &meta, markdown, markdown_len, NULL, NULL);
  db_close(&db);
  return result;
}

extern int database_route(client *cl, const char *id) {
  string_pool json_storage;
  string_pool response;
  json_object obj;
  db_t db;
  int db_open_ok = 0;

  db_selector_t selector;
  int result = 0;
  int lookup = 0;

  selector.key = 0;
  selector.klen = 0;
  selector.namespace_name = DATABASE_NAMESPACE;
  selector.content_type = 0;
  selector.flags_mask = 0;
  selector.flags_value = 0;
  selector.skip_blob = 0; // busqueda exacta: se necesita el valor completo

  if (string_pool_init(&json_storage, 1024) < 0 ||
      string_pool_init(&response, 2048) < 0) {
    write_headers(cl->fd, INTERNAL_ERROR);
    return -1;
  }
  if (json_object_init(&obj, &json_storage) < 0) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }

  if (database_open(&db)) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }
  db_open_ok = 1;

  selector.key = id;
  selector.klen = len(id);
  lookup = db_read(&db, &selector, load_record, (void *)&obj);

  if (lookup < 0) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = lookup;
    goto out;
  }

  if (!json_get(&obj, "found")) {
    write_headers(cl->fd, NOT_FOUND);
    goto out;
  }

  json_add_string(&obj, "id", id);

  if (json_serialize(&obj, &response) < 0) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }

  write_headers(cl->fd, OK);
  writef(cl->fd, "Content-Type: application/json\r\n");
  writef(cl->fd, "Content-Length: %ld\r\n", len(response.base));
  writef(cl->fd, "Connection: close\r\n\r\n");
  write(cl->fd, response.base, len(response.base));

out:
  if (db_open_ok)
    db_close(&db);
  string_pool_destroy(&json_storage);
  string_pool_destroy(&response);

  return result;
}

typedef struct {
  json_array *arr;
  string_pool *item_pool;
  string_pool *item_text;
  int failed;
} list_ctx_t;

static int load_list_item(void *ctx, const db_record_view_t *record) {
  list_ctx_t *lc = (list_ctx_t *)ctx;
  json_object item;
  char id[DATABASE_LIST_ID_CAP];
  long idlen = (long)record->klen < (long)sizeof(id) - 1
                  ? (long)record->klen
                  : (long)sizeof(id) - 1;
  char *value = NULL;

  string_pool_reset(lc->item_pool);
  string_pool_reset(lc->item_text);

  if (json_object_init(&item, lc->item_pool) < 0) {
    lc->failed = 1;
    return -1;
  }

  for (long i = 0; i < idlen; i++)
    id[i] = ((const char *)record->key)[i];
  id[idlen] = '\0';

  // El selector de listado pide skip_blob=1: los registros DB_RECORD_BLOB
  // llegan con data=NULL/dlen=0 (ver microdb/src/include/db.h). No se carga
  // el contenido completo solo para listar.
  if (record->data) {
    value = string_pool_nalloc(lc->item_pool, (const char *)record->data,
                               (long)record->dlen);
    if (!value) {
      lc->failed = 1;
      return -1;
    }
  }

  json_add_string(&item, "id", id);
  json_add_bool(&item, "json", (record->flags & DB_RECORD_JSON) ? 1 : 0);
  json_add_bool(&item, "blob", (record->flags & DB_RECORD_BLOB) ? 1 : 0);

  if (!value)
    json_add_null(&item, "value");
  else if (record->flags & DB_RECORD_JSON)
    json_add_object(&item, "value", value);
  else
    json_add_string(&item, "value", value);

  if (json_serialize(&item, lc->item_text) < 0) {
    lc->failed = 1;
    return -1;
  }

  if (json_array_add_object(lc->arr, lc->item_text->base) < 0) {
    lc->failed = 1;
    return -1;
  }

  return 0;
}

int database_list(client *cl, const char *namespace_name) {
  string_pool item_pool;
  string_pool item_text;
  string_pool array_storage;
  string_pool array_text;
  string_pool meta_storage;
  string_pool response;
  json_array arr;
  json_object meta;
  db_t db;
  db_selector_t selector;
  list_ctx_t ctx;
  int result = 0;
  int db_open_ok = 0;

  if (string_pool_init(&item_pool, 1024) < 0 ||
      string_pool_init(&item_text, 1024) < 0 ||
      string_pool_init(&array_storage, 2048) < 0 ||
      string_pool_init(&array_text, 2048) < 0 ||
      string_pool_init(&meta_storage, 512) < 0 ||
      string_pool_init(&response, 4096) < 0) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }

  if (json_array_init(&arr, &array_storage) < 0 ||
      json_object_init(&meta, &meta_storage) < 0) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }

  if (database_open(&db)) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }
  db_open_ok = 1;

  selector.key = 0;
  selector.klen = 0;
  selector.namespace_name = namespace_name;
  selector.content_type = 0;
  selector.flags_mask = 0;
  selector.flags_value = 0;
  selector.skip_blob = 1; // listar no debe cargar cada blob a memoria

  ctx.arr = &arr;
  ctx.item_pool = &item_pool;
  ctx.item_text = &item_text;
  ctx.failed = 0;

  if (db_read(&db, &selector, load_list_item, &ctx) < 0 || ctx.failed) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }

  if (json_array_serialize(&arr, &array_text) < 0) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }

  json_add_number(&meta, "count", arr.count);
  json_add_string(&meta, "namespace", namespace_name);
  json_add_object(&meta, "items", array_text.base);

  if (json_serialize(&meta, &response) < 0) {
    write_headers(cl->fd, INTERNAL_ERROR);
    result = -1;
    goto out;
  }

  write_headers(cl->fd, OK);
  writef(cl->fd, "Content-Type: application/json\r\n");
  writef(cl->fd, "Content-Length: %ld\r\n", len(response.base));
  writef(cl->fd, "Connection: close\r\n\r\n");
  write(cl->fd, response.base, len(response.base));

out:
  if (db_open_ok)
    db_close(&db);
  string_pool_destroy(&item_pool);
  string_pool_destroy(&item_text);
  string_pool_destroy(&array_storage);
  string_pool_destroy(&array_text);
  string_pool_destroy(&meta_storage);
  string_pool_destroy(&response);

  return result;
}
