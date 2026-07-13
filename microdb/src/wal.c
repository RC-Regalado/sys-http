#include "wal.h"
#include "crc32.h"
#include "util.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define HDR_BYTES                                                              \
  (4 + 1 + 1 + 2 + 4 + 4 + 4) // magic,u8 op,u8 flags,u16 rsv,klen,vlen,crc32

static int wal_write_record(int fd, uint8_t op, const void *key, uint32_t klen,
                            const void *val, uint32_t vlen, int do_fsync) {
  uint8_t hdr[HDR_BYTES];
  uint8_t *p = hdr;

  // magic
  u32_to_le(KV_MAGIC, p);
  p += 4;

  // op, flags
  *p++ = op;
  *p++ = 0; // flags

  // reserved (u16)
  *p++ = 0;
  *p++ = 0;

  // lengths
  u32_to_le(klen, p);
  p += 4;
  u32_to_le(vlen, p);
  p += 4;

  // crc32(key||value)
  crc32_init();
  uint32_t crc = crc32_begin();
  if (klen)
    crc = crc32_update(crc, key, klen);
  if (vlen)
    crc = crc32_update(crc, val, vlen);
  crc = crc32_end(crc);

  u32_to_le(crc, p);
  p += 4;

  if (write_full(fd, hdr, sizeof hdr) < 0)
    return -1;
  if (klen && write_full(fd, key, klen) < 0)
    return -1;
  if (vlen && write_full(fd, val, vlen) < 0)
    return -1;

  if (do_fsync) {
    if (fsync(fd) < 0)
      return -1;
  }
  return 0;
}

int wal_append_set(int fd, const void *key, size_t klen, const void *val,
                   size_t vlen, int do_fsync) {
  if (klen > 0xFFFFFFFFu || vlen > 0xFFFFFFFFu)
    return -1;
  return wal_write_record(fd, WAL_SET, key, (uint32_t)klen, val, (uint32_t)vlen,
                          do_fsync);
}

int wal_append_del(int fd, const void *key, size_t klen, int do_fsync) {
  if (klen > 0xFFFFFFFFu)
    return -1;
  return wal_write_record(fd, WAL_DEL, key, (uint32_t)klen, NULL, 0, do_fsync);
}

int wal_replay(int fd, void *ctx, wal_on_set_fn on_set, wal_on_del_fn on_del) {
  // Obtener tamaño actual del archivo
  struct stat st;
  if (fstat(fd, &st) < 0)
    return -1;

  // Archivo vacío => nada que replay
  if (st.st_size == 0)
    return 0;

  // Mapear solo lectura. MAP_PRIVATE evita que modificaciones accidentales se
  // reflejen al archivo.
  size_t fsz = (size_t)st.st_size;
  uint8_t *base = (uint8_t *)mmap(NULL, fsz, PROT_READ, MAP_PRIVATE, fd, 0);
  if (base == MAP_FAILED)
    return -1;

  // Iterador por offsets dentro del mapeo
  size_t off = 0;

  while (off < fsz) {
    // ¿hay suficientes bytes para header?
    if (fsz - off < (size_t)HDR_BYTES) {
      // truncado al final => parar “limpio”
      (void)munmap(base, fsz);
      return 0;
    }

    const uint8_t *p = base + off;

    uint32_t magic = le_to_u32(p);
    p += 4;
    uint8_t op = *p++;
    (void)*p++; // flags
    p += 2;     // reserved
    uint32_t klen = le_to_u32(p);
    p += 4;
    uint32_t vlen = le_to_u32(p);
    p += 4;
    uint32_t crc_expect = le_to_u32(p);
    p += 4;

    if (magic != KV_MAGIC) {
      (void)munmap(base, fsz);
      return -1;
    }
    if (!(op == WAL_SET || op == WAL_DEL)) {
      (void)munmap(base, fsz);
      return -1;
    }
    if (op == WAL_DEL && vlen != 0) {
      (void)munmap(base, fsz);
      return -1;
    }

    // ¿hay suficientes bytes para payload?
    // OJO: proteger overflow en klen+vlen
    uint64_t total64 = (uint64_t)klen + (uint64_t)vlen;
    if (total64 > (uint64_t)(SIZE_MAX)) {
      (void)munmap(base, fsz);
      return -1;
    }
    size_t total = (size_t)total64;

    size_t rec_total = (size_t)HDR_BYTES + total;
    if (fsz - off < rec_total) {
      // truncado al final => parar “limpio”
      (void)munmap(base, fsz);
      return 0;
    }

    const void *key = base + off + HDR_BYTES;
    const void *val = (const uint8_t *)key + klen;

    // Verificar CRC32 incremental sobre key||value (sin copiar)
    crc32_init();
    uint32_t crc = crc32_begin();
    if (klen)
      crc = crc32_update(crc, key, klen);
    if (vlen)
      crc = crc32_update(crc, val, vlen);
    crc = crc32_end(crc);

    if (crc != crc_expect) {
      (void)munmap(base, fsz);
      return -1;
    }

    // Aplicar callbacks
    if (op == WAL_SET) {
      if (on_set && on_set(ctx, key, klen, val, vlen) != 0) {
        (void)munmap(base, fsz);
        return -1;
      }
    } else { // WAL_DEL
      if (on_del && on_del(ctx, key, klen) != 0) {
        (void)munmap(base, fsz);
        return -1;
      }
    }

    off += rec_total;
  }

  (void)munmap(base, fsz);
  return 0;
}
