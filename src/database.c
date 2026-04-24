#include "database.h"

#include "io.h"
#include "json.h"
#include "requests.h"
#include "str.h"

#include "db.h"

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

  char *namespace_name = "database";
  char *content_type = "json";
  db_record_meta_t meta = {0};
  db_t db;

  if (db_open(&db, "./data.wal", 0)) {
    return 1;
  }

  if (!key || !namespace_name || !content_type || !payload) {
    return 1;
  }

  meta.namespace_name = namespace_name;
  meta.content_type = content_type;
  meta.flags = 0;
  meta.flags |= DB_RECORD_JSON;

  return db_write(&db, key, len(key), &meta, payload, payload_len, NULL, NULL);
}

extern int database_route(client *cl, const char *id) {
  string_pool json_storage;
  string_pool response;
  json_object obj;
  db_t db;

  db_selector_t selector = {0};
  int result = 0;
  int lookup = 0;

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

  if (db_open(&db, "./data.wal", 0)) {
    result = -1;
    goto out;
  }

  selector.key = id;
  selector.klen = len(id);
  lookup = db_read(&db, &selector, load_record, (void *)&obj);

  if (lookup < 0) {
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

  if (lookup == 0)
    write_headers(cl->fd, OK);
  else if (lookup == 1)
    write_headers(cl->fd, NOT_FOUND);
  else
    write_headers(cl->fd, INTERNAL_ERROR);

  writef(cl->fd, "Content-Type: application/json\r\n");
  writef(cl->fd, "Content-Length: %ld\r\n", len(response.base));
  writef(cl->fd, "Connection: close\r\n\r\n");
  write(cl->fd, response.base, len(response.base));

out:
  string_pool_destroy(&json_storage);
  string_pool_destroy(&response);

  return result;
}
