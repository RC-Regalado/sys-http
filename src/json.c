#include "json.h"

static int json_add_field(json_object *obj, const char *key) {
  if (!obj || !obj->storage || !key || obj->count >= JSON_MAX_FIELDS)
    return -1;

  char *k = string_pool_alloc(obj->storage, key);
  if (!k)
    return -1;

  obj->fields[obj->count].key = k;
  return obj->count++;
}

int json_object_init(json_object *obj, string_pool *storage) {
  if (!obj || !storage)
    return -1;

  obj->count = 0;
  obj->storage = storage;
  return 0;
}

void json_object_reset(json_object *obj) {
  if (!obj)
    return;
  obj->count = 0;
}

int json_add_string(json_object *obj, const char *key, const char *value) {
  int idx = json_add_field(obj, key);
  if (idx < 0)
    return -1;

  char *v = string_pool_alloc(obj->storage, value ? value : "");
  if (!v)
    return -1;

  obj->fields[idx].type = JSON_STRING;
  obj->fields[idx].as.string = v;
  return 0;
}

int json_add_number(json_object *obj, const char *key, long value) {
  int idx = json_add_field(obj, key);
  if (idx < 0)
    return -1;

  obj->fields[idx].type = JSON_NUMBER;
  obj->fields[idx].as.number = value;
  return 0;
}

int json_add_bool(json_object *obj, const char *key, int value) {
  int idx = json_add_field(obj, key);
  if (idx < 0)
    return -1;

  obj->fields[idx].type = JSON_BOOL;
  obj->fields[idx].as.boolean = value ? 1 : 0;
  return 0;
}

int json_add_object(json_object *obj, const char *key, const char *value) {
  int idx = json_add_field(obj, key);
  if (idx < 0)
    return -1;

  char *v = string_pool_alloc(obj->storage, value ? value : "");
  if (!v)
    return -1;

  obj->fields[idx].type = JSON_OBJECT;
  obj->fields[idx].as.string = v;
  return 0;
}

int json_add_null(json_object *obj, const char *key) {
  int idx = json_add_field(obj, key);
  if (idx < 0)
    return -1;

  obj->fields[idx].type = JSON_NULL;
  return 0;
}

const json_field *json_get(const json_object *obj, const char *key) {
  if (!obj || !key)
    return 0;

  for (int i = 0; i < obj->count; ++i) {
    if (strcmp(obj->fields[i].key, key) == 0)
      return &obj->fields[i];
  }
  return 0;
}

static int append_text(string_pool *pool, const char *text) {
  return string_pool_append(pool, text, 1) ? 0 : -1;
}

static int append_escaped_string(string_pool *pool, const char *value) {
  if (append_text(pool, "\"") < 0)
    return -1;

  for (int i = 0; value && value[i] != '\0'; ++i) {
    char c = value[i];
    if (c == '"' || c == '\\') {
      char escaped[3];
      escaped[0] = '\\';
      escaped[1] = c;
      escaped[2] = '\0';
      if (append_text(pool, escaped) < 0)
        return -1;
      continue;
    }

    char one[2];
    one[0] = c;
    one[1] = '\0';
    if (append_text(pool, one) < 0)
      return -1;
  }

  return append_text(pool, "\"");
}

int json_serialize(const json_object *obj, string_pool *out) {
  if (!obj || !out)
    return -1;

  if (append_text(out, "{") < 0)
    return -1;

  for (int i = 0; i < obj->count; ++i) {
    const json_field *field = &obj->fields[i];

    if (i > 0 && append_text(out, ",") < 0)
      return -1;

    if (append_escaped_string(out, field->key) < 0)
      return -1;
    if (append_text(out, ":") < 0)
      return -1;

    if (field->type == JSON_STRING) {
      if (append_escaped_string(out, field->as.string) < 0)
        return -1;
      continue;
    }

    if (field->type == JSON_NUMBER) {
      if (string_pool_format(out, "%ld", field->as.number) < 0)
        return -1;
      continue;
    }

    if (field->type == JSON_BOOL) {
      if (append_text(out, field->as.boolean ? "true" : "false") < 0)
        return -1;
      continue;
    }

    if (field->type == JSON_OBJECT) {
      if (append_text(out, field->as.string) < 0)
        return -1;
      continue;
    }

    if (append_text(out, "null") < 0)
      return -1;
  }

  return append_text(out, "}");
}
