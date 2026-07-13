#include "crc32.h"

static uint32_t table[256];
static int ready = 0;

void crc32_init(void) {
  if (ready)
    return;
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) {
      c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    }
    table[i] = c;
  }
  ready = 1;
}

uint32_t crc32_begin(void) { return 0xFFFFFFFFu; }

uint32_t crc32_update(uint32_t crc, const void *data, size_t n) {
  const unsigned char *p = (const unsigned char *)data;
  for (size_t i = 0; i < n; i++) {
    crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
  }
  return crc;
}

uint32_t crc32_end(uint32_t crc) { return crc ^ 0xFFFFFFFFu; }
