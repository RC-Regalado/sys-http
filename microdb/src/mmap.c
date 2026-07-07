#include "mmap.h"
#include "crc32.h"
#include "page.h"
#include "paged_file.h"
#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int decode_record(const uint8_t *rec, uint16_t rec_len,
                         const void **out_key, uint32_t *out_klen,
                         const void **out_val, uint32_t *out_vlen) {
  if (rec_len < 12)
    return -1;

  uint32_t klen = le_to_u32(rec + 0);
  uint32_t vlen = le_to_u32(rec + 4);
  uint32_t crc_expect = le_to_u32(rec + 8);

  uint64_t total64 = 12ull + (uint64_t)klen + (uint64_t)vlen;
  if (total64 != (uint64_t)rec_len)
    return -1;

  const void *key = rec + 12;
  const void *val = rec + 12 + klen;

  crc32_init();
  uint32_t crc = crc32_begin();
  if (klen)
    crc = crc32_update(crc, key, klen);
  if (vlen)
    crc = crc32_update(crc, val, vlen);
  crc = crc32_end(crc);

  if (crc != crc_expect)
    return -1;

  *out_key = key;
  *out_klen = klen;
  *out_val = val;
  *out_vlen = vlen;
  return 0;
}

int mmap_load_into_ht(const char *path, ht_t *ht) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    if (errno == ENOENT)
      return 0;

    printf("NULL\n");
    return -1;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    close(fd);
    return -1;
  }
  if (st.st_size < PAGE_SIZE) {
    close(fd);
    return 0;
  }

  size_t fsz = (size_t)st.st_size;

  uint8_t *base = mmap(NULL, fsz, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (base == MAP_FAILED)
    return -1;

  // Page 0: superblock
  const superblock_t *sb = (const superblock_t *)(base + 0 * PAGE_SIZE);
  if (sb->magic != SB_MAGIC || sb->version != SB_VERSION) {
    munmap(base, fsz);
    return -1;
  }

  uint32_t page_count = sb->page_count;
  if ((uint64_t)page_count * PAGE_SIZE > fsz) {
    munmap(base, fsz);
    return -1;
  }

  // Recorrer páginas de datos
  for (uint32_t pid = 1; pid < page_count; pid++) {
    const page_t *p = (const page_t *)(base + pid * PAGE_SIZE);
    const page_hdr_t *h = (const page_hdr_t *)p->bytes;

    if (h->magic != PAGE_MAGIC) {
      munmap(base, fsz);
      return -1;
    }
    if (h->type != PAGE_DATA)
      continue;

    const page_slot_t *slots =
        (const page_slot_t *)(p->bytes + sizeof(page_hdr_t));

    for (uint16_t i = 0; i < h->slot_count; i++) {
      if (slots[i].flags & 0x1)
        continue; // tombstone

      uint16_t off = slots[i].off;
      uint16_t len = slots[i].len;
      if ((uint32_t)off + (uint32_t)len > PAGE_SIZE) {
        munmap(base, fsz);
        return -1;
      }

      const uint8_t *rec = p->bytes + off;

      const void *key, *val;
      uint32_t klen, vlen;
      if (decode_record(rec, len, &key, &klen, &val, &vlen) != 0) {
        munmap(base, fsz);
        return -1;
      }

      // Inserta/overwrite en HT (copia a heap)
      if (ht_set(ht, key, klen, val, vlen) != 0) {
        munmap(base, fsz);
        return -1;
      }
    }
  }

  munmap(base, fsz);
  return 0;
}
