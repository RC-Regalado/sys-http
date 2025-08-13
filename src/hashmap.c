#include "inc/hashmap.h"
#include "inc/str.h"

unsigned long djb2_hash(const char *str) {
  unsigned long h = 5381;

  char c;
  while ((c = *str++))
    h = ((h << 5) + h) + c;

  return h;
}

void hashmap_put(hash_map *map, const char *key, const char *value) {
  unsigned long h = djb2_hash(key) % MAX_BUCKETS;

  entry *e = map->buckets[h];

  while (e) {
    if (strcmp(e->key, key) == 0) {
      e->value = value;
      return;
    }
    e = e->next;
  }

  if (map->pool_index >= MAX_ENTRIES)
    return;

  entry *new_entry = &map->pool[map->pool_index++];
  new_entry->key = key;
  new_entry->value = value;

  new_entry->next = map->buckets[h];
  map->buckets[h] = new_entry;
}

const char *hashmap_get(hash_map *map, const char *key) {
  unsigned long h = djb2_hash(key) % MAX_BUCKETS;

  entry *e = map->buckets[h];

  while (e) {
    if (strcmp(e->key, key) == 0)
      return e->value;

    e = e->next;
  }

  return 0;
}
