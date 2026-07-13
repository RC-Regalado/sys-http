#pragma once
#include <stddef.h>
#include <stdint.h>

#define KV_MAGIC 0xBADC0FFEu

int write_full(int fd, const void *buf, size_t n);
int read_full(int fd, void *buf, size_t n);
int read_exact_or_eof(int fd, void *buf, size_t n, int *out_eof);

void u32_to_le(uint32_t v, uint8_t out[4]);
uint32_t le_to_u32(const uint8_t in[4]);

void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
void xfree(void *p);

uint32_t fnv1a_32(const void *data, size_t n);
