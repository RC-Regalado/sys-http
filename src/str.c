#include "inc/str.h"
#include "inc/memory.h"

unsigned int len(const char *str) {
  int i = 0;
  while (str[i] != 0)
    i++;
  return i;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }

  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int index(const char *s1, const char c) {
  int i = 0;
  unsigned int l = len(s1);

  if (l == 0)
    return -1;

  while (s1[i] != c && i < l)
    i++;

  if (i == l)
    return -1;

  return i;
}

void substr(const char *s1, char *buffer, int pos, int size) {
  int i = 0;
  while (i < size && i < len(s1)) {
    buffer[i] = s1[pos + i];
    i++;
  }
}

int string_pool_init(string_pool *pool, long capacity) {
  pool->base = sysmap_alloc(capacity);
  if (!pool->base)
    return -1;
  pool->capacity = capacity;
  pool->offset = 0;
  return 0;
}

char *string_pool_alloc(string_pool *pool, const char *src, long len) {
  if (pool->offset + len + 1 > pool->capacity)
    return NULL; // no hay espacio

  char *dest = &pool->base[pool->offset];
  for (long i = 0; i < len; i++) {
    dest[i] = src[i];
  }
  dest[len] = '\0';
  pool->offset += len + 1;
  return dest;
}

void string_pool_reset(string_pool *pool) { pool->offset = 0; }

void string_pool_destroy(string_pool *pool) { sysmap_free(pool->base); }
