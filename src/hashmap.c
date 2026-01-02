#include "hashmap.h"
#include "str.h"

/**
 * @brief Genera un hash a partir de la cadena que se le pase como parámetro.
 *
 * @param str Cadena base del hash.
 */
unsigned long djb2_hash(const char *str) {
  unsigned long h = 5381;

  char c;
  while ((c = *str++))
    h = ((h << 5) + h) + c;

  return h;
}

/**
 * @brief Agregra un nuevo campo clave-valor dentro del hashmap.
 *
 * @param map Puntero al mapa hash a utilizar.
 * @param key Cadena que se valida como clave de acceso.
 * @param value Cadena que se almacena, accesible mediante key.
 */
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

/**
 * @brief Busca entre los datos almacenados un hash que concuerde con la clave.
 *
 * @param map Puntero al mapa hash a utilizar .
 * @param key Cadena utilizada para acceder al valor almacenado.
 */
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

/**
 * @brief Inicializa un hashmap con todos sus buckets vacíos.
 *
 * @param map Puntero al mapa hash a inicializar.
 */
void hashmap_init(hash_map *map) {
  for (int i = 0; i < MAX_BUCKETS; i++) {
    map->buckets[i] = 0x0;
  }

  for (int i = 0; i < MAX_ENTRIES; i++) {
    map->pool[i].key = 0x0;
    map->pool[i].value = 0x0;
    map->pool[i].next = 0x0;
  }

  map->pool_index = 0;
}

void hashmap_reset(hash_map *map) { hashmap_init(map); }
