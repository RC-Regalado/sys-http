#ifndef QUERY_H_
#define QUERY_H_

#include "hashmap.h"
#include "str.h"

// Corta `raw` en el primer '?' (lo reemplaza por '\0') y devuelve un
// puntero al inicio del query string (lo que sigue al '?'), o 0 si no
// hay '?'. Muta `raw` in-place.
char *query_split(char *raw);

// Parsea una query string ya separada (sin '?', ej. "a=1&b=hola%20mundo")
// en pares clave/valor dentro de `map`, decodificando percent-encoding
// y '+' como espacio (application/x-www-form-urlencoded). Las copias
// decodificadas se alojan en `pool`.
void query_parse(hash_map *map, string_pool *pool, const char *query);

#endif // QUERY_H_
