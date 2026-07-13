#include "page.h"
#include <string.h>

static page_hdr_t *hdr(page_t *p) { return (page_hdr_t *)p->bytes; }

static const page_hdr_t *hdr_c(const page_t *p) {
  return (const page_hdr_t *)p->bytes;
}

static page_slot_t *slots(page_t *p) {
  return (page_slot_t *)(p->bytes + sizeof(page_hdr_t));
}

static const page_slot_t *slots_c(const page_t *p) {
  return (const page_slot_t *)(p->bytes + sizeof(page_hdr_t));
}

void page_init(page_t *p, uint16_t type) {
  memset(p->bytes, 0, PAGE_SIZE);
  page_hdr_t *h = hdr(p);
  h->magic = PAGE_MAGIC;
  h->type = type;
  h->slot_count = 0;
  h->free_start = (uint16_t)sizeof(page_hdr_t);
  h->free_end = (uint16_t)PAGE_SIZE;
  h->next_free = 0;
}

int page_try_insert(page_t *p, const void *rec, uint16_t rec_len,
                    uint16_t *out_slot) {
  page_hdr_t *h = hdr(p);
  if (h->magic != PAGE_MAGIC || h->type != PAGE_DATA)
    return -1;

  uint16_t need = (uint16_t)(sizeof(page_slot_t) + rec_len);
  if (h->free_start + need > h->free_end)
    return -1; // no cabe

  // Reservar payload desde el final
  h->free_end -= rec_len;
  memcpy(p->bytes + h->free_end, rec, rec_len);

  // Crear slot
  page_slot_t *s = slots(p) + h->slot_count;
  s->off = h->free_end;
  s->len = rec_len;
  s->flags = 0;
  s->rsv = 0;

  if (out_slot)
    *out_slot = h->slot_count;

  h->slot_count++;
  h->free_start += (uint16_t)sizeof(page_slot_t);
  return 0;
}

int page_get(const page_t *p, uint16_t slot, const void **out_ptr,
             uint16_t *out_len) {
  const page_hdr_t *h = hdr_c(p);
  if (slot >= h->slot_count)
    return -1;

  const page_slot_t *s = slots_c(p) + slot;
  if (s->flags & 0x1)
    return -1; // tombstone

  if (out_ptr)
    *out_ptr = p->bytes + s->off;
  if (out_len)
    *out_len = s->len;
  return 0;
}

int page_delete(page_t *p, uint16_t slot) {
  page_hdr_t *h = hdr(p);
  if (slot >= h->slot_count)
    return -1;
  page_slot_t *s = slots(p) + slot;
  s->flags |= 0x1; // marcar tombstone
  return 0;
}
