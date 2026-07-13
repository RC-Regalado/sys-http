#pragma once
#include <stddef.h>
#include <stdint.h>

void crc32_init(void);
uint32_t crc32_begin(void);
uint32_t crc32_update(uint32_t crc, const void *data, size_t n);
uint32_t crc32_end(uint32_t crc);
