#pragma once
#include <stddef.h>
#include <stdint.h>

enum wal_op { WAL_SET = 1, WAL_DEL = 2 };

struct wal_record {
  uint8_t op;
  uint32_t klen;
  uint32_t vlen;  // 0 para DEL
  uint32_t crc32; // sobre (key||value)
};

int wal_append_set(int fd, const void *key, size_t klen, const void *val,
                   size_t vlen, int do_fsync);
int wal_append_del(int fd, const void *key, size_t klen, int do_fsync);

// Replay: llama a callbacks por cada registro válido.
// Si hay truncado al final, se detiene sin error duro (retorna 0).
typedef int (*wal_on_set_fn)(void *ctx, const void *key, uint32_t klen,
                             const void *val, uint32_t vlen);
typedef int (*wal_on_del_fn)(void *ctx, const void *key, uint32_t klen);

int wal_replay(int fd, void *ctx, wal_on_set_fn on_set, wal_on_del_fn on_del);
