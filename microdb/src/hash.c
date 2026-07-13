#include "hash.h"
#include "util.h"
#include <string.h>

static size_t next_pow2(size_t x) {
  size_t p = 1;
  while (p < x)
    p <<= 1;
  return p;
}

static int key_eq(const ht_entry_t *e, const void *key, size_t klen) {
  return e->klen == klen && memcmp(e->key, key, klen) == 0;
}

static int ht_resize(ht_t *ht, size_t new_cap) {
  ht_entry_t *old = ht->entries;
  size_t oldcap = ht->cap;

  ht_entry_t *ne = (ht_entry_t *)xmalloc(new_cap * sizeof(ht_entry_t));
  for (size_t i = 0; i < new_cap; i++) {
    ne[i].state = 0;
    ne[i].key = ne[i].val = NULL;
    ne[i].klen = ne[i].vlen = 0;
    ne[i].hash = 0;
  }

  ht->entries = ne;
  ht->cap = new_cap;
  ht->len = 0;

  for (size_t i = 0; i < oldcap; i++) {
    if (old[i].state == 1) {
      // reinsert (sin copiar contenido, movemos punteros)
      uint32_t h = old[i].hash;
      size_t mask = new_cap - 1;
      size_t pos = (size_t)h & mask;
      while (ne[pos].state == 1)
        pos = (pos + 1) & mask;

      ne[pos] = old[i];
      ne[pos].state = 1;
      ht->len++;
    }
  }

  // liberar solo el array viejo (no las claves/vals, ya se movieron)
  xfree(old);
  return 0;
}

int ht_iterate(const ht_t *ht, ht_on_kv_fn cb, void *cb_ctx) {
  for (size_t i = 0; i < ht->cap; i++) {
    const ht_entry_t *e = &ht->entries[i];
    if (e->state == 1) {
      if (cb(cb_ctx, e->key, e->klen, e->val, e->vlen) != 0)
        return -1;
    }
  }
  return 0;
}

int ht_init(ht_t *ht, size_t initial_cap_pow2) {
  size_t cap = next_pow2(initial_cap_pow2 < 8 ? 8 : initial_cap_pow2);
  ht->entries = (ht_entry_t *)xmalloc(cap * sizeof(ht_entry_t));
  ht->cap = cap;
  ht->len = 0;
  for (size_t i = 0; i < cap; i++) {
    ht->entries[i].state = 0;
    ht->entries[i].key = ht->entries[i].val = NULL;
    ht->entries[i].klen = ht->entries[i].vlen = 0;
    ht->entries[i].hash = 0;
  }
  return 0;
}

void ht_destroy(ht_t *ht) {
  if (!ht->entries)
    return;
  for (size_t i = 0; i < ht->cap; i++) {
    if (ht->entries[i].state == 1) {
      xfree(ht->entries[i].key);
      xfree(ht->entries[i].val);
    }
  }
  xfree(ht->entries);
  ht->entries = NULL;
  ht->cap = ht->len = 0;
}

static int ht_maybe_grow(ht_t *ht) {
  // load factor ~ 0.7
  if ((ht->len + 1) * 10 >= ht->cap * 7) {
    return ht_resize(ht, ht->cap * 2);
  }
  return 0;
}

int ht_set(ht_t *ht, const void *key, size_t klen, const void *val,
           size_t vlen) {
  if (ht_maybe_grow(ht) != 0)
    return -1;

  uint32_t h = fnv1a_32(key, klen);
  size_t mask = ht->cap - 1;
  size_t pos = (size_t)h & mask;

  size_t first_tomb = (size_t)-1;

  for (;;) {
    ht_entry_t *e = &ht->entries[pos];

    if (e->state == 0) {
      if (first_tomb != (size_t)-1)
        e = &ht->entries[first_tomb];
      e->hash = h;
      e->state = 1;
      e->klen = (uint32_t)klen;
      e->vlen = (uint32_t)vlen;
      e->key = xmalloc(klen ? klen : 1);
      e->val = xmalloc(vlen ? vlen : 1);
      if (klen)
        memcpy(e->key, key, klen);
      if (vlen)
        memcpy(e->val, val, vlen);
      ht->len++;
      return 0;
    }

    if (e->state == 2) {
      if (first_tomb == (size_t)-1)
        first_tomb = pos;
    } else if (e->hash == h && key_eq(e, key, klen)) {
      // update: reemplazar val
      xfree(e->val);
      e->val = xmalloc(vlen ? vlen : 1);
      e->vlen = (uint32_t)vlen;
      if (vlen)
        memcpy(e->val, val, vlen);
      return 0;
    }

    pos = (pos + 1) & mask;
  }
}

int ht_get(const ht_t *ht, const void *key, size_t klen, const void **out_val,
           size_t *out_vlen) {
  uint32_t h = fnv1a_32(key, klen);
  size_t mask = ht->cap - 1;
  size_t pos = (size_t)h & mask;

  for (;;) {
    const ht_entry_t *e = &ht->entries[pos];

    if (e->state == 0)
      return -1;
    if (e->state == 1 && e->hash == h && key_eq(e, key, klen)) {
      *out_val = e->val;
      *out_vlen = e->vlen;
      return 0;
    }

    pos = (pos + 1) & mask;
  }
}

int ht_del(ht_t *ht, const void *key, size_t klen) {
  uint32_t h = fnv1a_32(key, klen);
  size_t mask = ht->cap - 1;
  size_t pos = (size_t)h & mask;

  for (;;) {
    ht_entry_t *e = &ht->entries[pos];

    if (e->state == 0)
      return -1;
    if (e->state == 1 && e->hash == h && key_eq(e, key, klen)) {
      xfree(e->key);
      xfree(e->val);
      e->key = e->val = NULL;
      e->klen = e->vlen = 0;
      e->hash = 0;
      e->state = 2; // tombstone
      ht->len--;
      return 0;
    }

    pos = (pos + 1) & mask;
  }
}
