#include "query.h"

#define QUERY_TOKEN_CAP 256

char *query_split(char *raw) {
  for (int i = 0; raw[i] != '\0'; i++) {
    if (raw[i] == '?') {
      raw[i] = '\0';
      return &raw[i + 1];
    }
  }
  return 0;
}

static int hex_val(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static int url_decode(const char *src, int src_len, char *dst, int dst_cap) {
  int di = 0;

  for (int si = 0; si < src_len && di < dst_cap - 1; si++) {
    char c = src[si];

    if (c == '+') {
      dst[di++] = ' ';
      continue;
    }

    if (c == '%' && si + 2 < src_len) {
      int hi = hex_val(src[si + 1]);
      int lo = hex_val(src[si + 2]);
      if (hi >= 0 && lo >= 0) {
        dst[di++] = (char)((hi << 4) | lo);
        si += 2;
        continue;
      }
    }

    dst[di++] = c;
  }

  dst[di] = '\0';
  return di;
}

void query_parse(hash_map *map, string_pool *pool, const char *query) {
  if (!map || !pool || !query)
    return;

  int qlen = (int)len(query);
  int i = 0;

  while (i < qlen) {
    int seg_start = i;
    while (i < qlen && query[i] != '&')
      i++;
    int seg_len = i - seg_start;

    if (seg_len > 0) {
      int eq = -1;
      for (int j = 0; j < seg_len; j++) {
        if (query[seg_start + j] == '=') {
          eq = j;
          break;
        }
      }

      int key_raw_len = eq >= 0 ? eq : seg_len;
      int val_raw_len = eq >= 0 ? seg_len - eq - 1 : 0;
      const char *val_raw = eq >= 0 ? query + seg_start + eq + 1 : "";

      char key_buf[QUERY_TOKEN_CAP];
      char val_buf[QUERY_TOKEN_CAP];

      url_decode(query + seg_start, key_raw_len, key_buf, sizeof(key_buf));
      url_decode(val_raw, val_raw_len, val_buf, sizeof(val_buf));

      if (key_buf[0] != '\0') {
        char *k = string_pool_alloc(pool, key_buf);
        char *v = string_pool_alloc(pool, val_buf);
        if (k && v)
          hashmap_put(map, k, v);
      }
    }

    i++; // saltar '&'
  }
}
