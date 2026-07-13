#include "util.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int write_full(int fd, const void *buf, size_t n) {
  const uint8_t *p = (const uint8_t *)buf;
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(fd, p + off, n - off);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    off += (size_t)w;
  }
  return 0;
}

int read_full(int fd, void *buf, size_t n) {
  uint8_t *p = (uint8_t *)buf;
  size_t off = 0;
  while (off < n) {
    ssize_t r = read(fd, p + off, n - off);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0)
      return -2; // EOF inesperado
    off += (size_t)r;
  }
  return 0;
}

// Lee n bytes. Si EOF antes de leer cualquier byte => out_eof=1 y retorna 0.
// Si EOF a mitad => retorna -2 (registro truncado).
int read_exact_or_eof(int fd, void *buf, size_t n, int *out_eof) {
  *out_eof = 0;
  uint8_t *p = (uint8_t *)buf;
  size_t off = 0;

  while (off < n) {
    ssize_t r = read(fd, p + off, n - off);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0) {
      if (off == 0) {
        *out_eof = 1;
        return 0;
      }
      return -2; // truncado
    }
    off += (size_t)r;
  }
  return 0;
}

void u32_to_le(uint32_t v, uint8_t out[4]) {
  out[0] = (uint8_t)(v & 0xFFu);
  out[1] = (uint8_t)((v >> 8) & 0xFFu);
  out[2] = (uint8_t)((v >> 16) & 0xFFu);
  out[3] = (uint8_t)((v >> 24) & 0xFFu);
}

uint32_t le_to_u32(const uint8_t in[4]) {
  return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) |
         ((uint32_t)in[3] << 24);
}

void *xmalloc(size_t n) {
  void *p = malloc(n ? n : 1);
  if (!p)
    abort();
  return p;
}

void *xrealloc(void *p, size_t n) {
  void *q = realloc(p, n ? n : 1);
  if (!q)
    abort();
  return q;
}

void xfree(void *p) { free(p); }

uint32_t fnv1a_32(const void *data, size_t n) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}
