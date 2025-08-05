#ifndef __HASHMAP_H
#define MAX_BUCKETS 32
#define MAX_ENTRIES 64

typedef struct entry {
  const char *key;
  const char *value;
  struct entry *next;
} entry;

typedef struct {
  entry *buckets[MAX_BUCKETS];
  entry pool[MAX_ENTRIES];
  int pool_index;
} hash_map;

unsigned long djb2_hash(const char *str);
void hashmap_put(hash_map *map, const char *key, const char *value);
const char *hashmap_get(hash_map *map, const char *key);

#endif // !__HASHMAP_H
