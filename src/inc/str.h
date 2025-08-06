#ifndef __STR_H
#define __STR_H

// ERRORS
#define NOMEM -1
#define SIZEERR -2

typedef struct {
  char *base;    // Memoria asignada por sysmap_alloc()
  long capacity; // Tamaño total (en bytes)
  long offset;   // Cuánto se ha usado
  long mark;     //
} string_pool;

unsigned int len(const char *string);
unsigned int nlen(long number);

int strcmp(const char *s1, const char *s2);
int index(const char *s1, const char c);
void substr(const char *s1, char *buffer, int pos, int size);
void tostr(char *buffer, int number, unsigned int size);

// Funciones para uso optimizado de memoria
int string_pool_init(string_pool *pool, long capacity);

char *string_pool_alloc(string_pool *pool, const char *src);
char *string_pool_nalloc(string_pool *pool, const char *src, long size);
int string_pool_relloc(string_pool *pool, long new_capacity);

char *string_pool_append(string_pool *pool, const char *src,
                         unsigned char realloc);

void string_pool_reset(string_pool *pool);
void string_pool_destroy(string_pool *pool);

void string_pool_mark(string_pool *pool);
void string_pool_reset_to_mark(string_pool *pool);
#endif // __STR_H
