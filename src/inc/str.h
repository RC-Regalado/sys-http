#ifndef __STR_H
#define __STR_H

typedef struct {
  char *base;    // Memoria asignada por sysmap_alloc()
  long capacity; // Tamaño total (en bytes)
  long offset;   // Cuánto se ha usado
} string_pool;

unsigned int len(const char *string);
int strcmp(const char *s1, const char *s2);
int index(const char *s1, const char c);
void substr(const char *s1, char *buffer, int pos, int size);

// Funciones para uso optimizado de memoria
int string_pool_init(string_pool *pool, long capacity);
char *string_pool_alloc(string_pool *pool, const char *src, long len);

void string_pool_reset(string_pool *pool);
void string_pool_destroy(string_pool *pool);

#endif // __STR_H
