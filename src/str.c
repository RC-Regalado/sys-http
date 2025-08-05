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
    return NOMEM;
  pool->capacity = capacity;
  pool->offset = 0;
  return 0;
}

char *string_pool_alloc(string_pool *pool, const char *src) {
  long size = len(src);
  return string_pool_nalloc(pool, src, size);
}

char *string_pool_nalloc(string_pool *pool, const char *src, long size) {
  if (pool->offset + size + 1 > pool->capacity)
    return NULL; // no hay espacio

  char *dest = &pool->base[pool->offset];
  for (long i = 0; i < size; i++) {
    dest[i] = src[i];
  }
  dest[size] = '\0';
  pool->offset += size + 1;
  return dest;
}

int string_pool_relloc(string_pool *pool, long new_capacity) {
  if (!pool->base)
    return NOMEM;

  if (pool->capacity > new_capacity)
    return SIZEERR;

  char *old_base = pool->base;

  pool->base = sysmap_alloc(new_capacity);

  if (!pool->base)
    return NOMEM;

  for (long i = 0; i < pool->offset; i++) {
    pool->base[i] = old_base[i];
  }

  pool->capacity = new_capacity;
  sysmap_free(old_base);
  return 0;
}

char *string_pool_append(string_pool *pool, const char *src,
                         unsigned char realloc) {
  long size = len(src);

  if (pool->offset + size > pool->capacity) {
    if (!realloc)
      return NULL; // no hay espacio

    if (string_pool_relloc(pool, pool->capacity * 2) != 0)
      return NULL;
  }

  long index = pool->offset == 0 ? 0 : pool->offset - 1;

  char *dest = &pool->base[index];

  for (long i = 0; i < size; i++) {
    dest[i] = src[i];
  }
  dest[size] = '\0';
  pool->offset += size + 1;

  if (index > 0) {
    // Encontrar 0 o el último corte del pool
    while (pool->base[index] != '\0' && index > 0) {
      index--;
    }
    return &pool->base[index];
  }

  return dest;
}

void string_pool_reset(string_pool *pool) { pool->offset = 0; }

void string_pool_destroy(string_pool *pool) { sysmap_free(pool->base); }

void string_pool_mark(string_pool *pool) { pool->mark = pool->offset; }

void string_pool_reset_to_mark(string_pool *pool) {
  pool->offset = pool->mark;

  if (pool->offset > 0)
    pool->base[pool->offset - 1] = '\0';
}
