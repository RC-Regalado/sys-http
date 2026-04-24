#ifndef JSON_H_
#define JSON_H_

#include "str.h"

#define JSON_MAX_FIELDS 32

enum json_type { JSON_STRING, JSON_NUMBER, JSON_BOOL, JSON_OBJECT, JSON_NULL };

typedef struct {
  const char *key;
  enum json_type type;
  union {
    const char *string;
    long number;
    int boolean;
  } as;
} json_field;

typedef struct {
  json_field fields[JSON_MAX_FIELDS];
  int count;
  string_pool *storage;
} json_object;

int json_object_init(json_object *obj, string_pool *storage);
void json_object_reset(json_object *obj);

int json_add_string(json_object *obj, const char *key, const char *value);
int json_add_number(json_object *obj, const char *key, long value);
int json_add_bool(json_object *obj, const char *key, int value);
int json_add_object(json_object *obj, const char *key, const char *value);

int json_add_null(json_object *obj, const char *key);

const json_field *json_get(const json_object *obj, const char *key);
int json_serialize(const json_object *obj, string_pool *out);

#endif // JSON_H_
