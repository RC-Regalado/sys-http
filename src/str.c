#include "inc/str.h"
#include "inc/io.h"
#include "inc/memory.h"

#include <stdarg.h>

unsigned int len(const char *str) {
  int i = 0;
  while (str[i] != 0)
    i++;
  return i;
}

unsigned int nlen(long number) {
  if (number == 0)
    return 1;
  unsigned int size = 0;
  long tmp = number;
  if (tmp < 0) {
    size++;
    tmp = -tmp;
  }
  while (tmp) {
    tmp /= 10;
    size++;
  }
  return size;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }

  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, int size) {
  while (*s1 && (*s1 == *s2) && size--) {
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

int last_index_of(const char *s1, char c) {
  int l = len(s1) - 1;

  if (l <= 0)
    return -1;

  while (s1[l] != c && l > -1)
    l--;

  if (l == -1)
    return -1;

  return l;
}

void substr(const char *s1, char *buffer, int pos, int size) {
  int i = 0;
  unsigned int l = len(s1);

  while (i < size && i < l) {
    buffer[i] = s1[pos + i];
    i++;
  }
}

void tostr(char *buffer, long number, unsigned int size) {
  if (!buffer || size == 0)
    return;
  buffer[size - 1] = '\0';     // terminador
  unsigned int top = size - 2; // escribe desde el final hacia atrás
  unsigned long tmp;
  unsigned int start = 0;
  if (number < 0) {
    buffer[0] = '-';
    start = 1;
    tmp = (unsigned long)(-number);
  } else
    tmp = (unsigned long)number;

  if (tmp == 0) {
    buffer[start] = '0';
    buffer[start + 1] = '\0';
    return;
  }

  while (tmp && top >= start) {
    buffer[top] = (char)('0' + (tmp % 10));
    tmp /= 10;
    if (top == 0)
      break;
    top--;
  }
  // compacta hacia delante si quedó espacio
  if (start < top) {
    unsigned int len = (size - 1) - top;
    for (unsigned int i = 0; i < len; i++)
      buffer[start + i] = buffer[top + 1 + i];
    buffer[start + len] = '\0';
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

void string_pool_reset(string_pool *pool) {
  pool->offset = 0;
  pool->base[pool->offset] = '\0';
}

void string_pool_destroy(string_pool *pool) { sysmap_free(pool->base); }

void string_pool_mark(string_pool *pool) { pool->mark = pool->offset; }

void string_pool_reset_to_mark(string_pool *pool) {
  pool->offset = pool->mark;

  if (pool->offset > 0)
    pool->base[pool->offset - 1] = '\0';
}

int string_pool_format(string_pool *pool, const char *fmt, ...) {
  int res;

  va_list ap;
  va_start(ap, fmt);

  int size;

  format(pool->base, &size, fmt, ap);

  pool->offset = size;

out:
  va_end(ap);
  return res;
}
