#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t hash;
  uint8_t state; // 0=empty, 1=full, 2=tomb
  uint32_t klen;
  uint32_t vlen;
  void *key;
  void *val;
} ht_entry_t;

typedef struct {
  ht_entry_t *entries;
  size_t cap; // potencia de 2
  size_t len; // count de full
} ht_t;

typedef int (*ht_on_kv_fn)(void *ctx, const void *key, uint32_t klen,
                           const void *val, uint32_t vlen);
int ht_iterate(const ht_t *ht, ht_on_kv_fn cb, void *cb_ctx);

int ht_init(ht_t *ht, size_t initial_cap_pow2);
void ht_destroy(ht_t *ht);

int ht_set(ht_t *ht, const void *key, size_t klen, const void *val,
           size_t vlen);
int ht_get(const ht_t *ht, const void *key, size_t klen, const void **out_val,
           size_t *out_vlen);
int ht_del(ht_t *ht, const void *key, size_t klen);
