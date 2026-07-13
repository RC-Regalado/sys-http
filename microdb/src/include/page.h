#pragma once
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096u
#define PAGE_MAGIC 0x50414745u /* 'PAGE' */

enum page_type {
  PAGE_FREE = 0,
  PAGE_DATA = 1,
};

typedef struct {
  uint32_t magic;
  uint16_t type;
  uint16_t slot_count;
  uint16_t free_start; // fin del header + slots
  uint16_t free_end;   // inicio del payload (desde el final)
  uint32_t next_free;  // solo si PAGE_FREE
} page_hdr_t;

typedef struct {
  uint16_t off;   // offset dentro de la page
  uint16_t len;   // longitud del record
  uint16_t flags; // bit 0 = tombstone
  uint16_t rsv;
} page_slot_t;

typedef struct {
  uint8_t bytes[PAGE_SIZE];
} page_t;

/* API */
void page_init(page_t *p, uint16_t type);
int page_try_insert(page_t *p, const void *rec, uint16_t rec_len,
                    uint16_t *out_slot);
int page_get(const page_t *p, uint16_t slot, const void **out_ptr,
             uint16_t *out_len);
int page_delete(page_t *p, uint16_t slot);
