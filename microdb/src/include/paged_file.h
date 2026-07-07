#pragma once
#include "page.h"
#include <stdint.h>

#define SB_MAGIC 0x53424C4Bu /* 'SBLK' */
#define SB_VERSION 1u

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t page_count;
  uint32_t free_head;
} superblock_t;

typedef struct {
  int fd;
  superblock_t sb;
} paged_file_t;

int pf_open(paged_file_t *pf, const char *path, int create);
int pf_close(paged_file_t *pf);

int pf_read_page(paged_file_t *pf, uint32_t pid, page_t *out);
int pf_write_page(paged_file_t *pf, uint32_t pid, const page_t *p);

int pf_alloc_page(paged_file_t *pf, uint32_t *out_pid, page_t *out_page);
int pf_free_page(paged_file_t *pf, uint32_t pid, page_t *scratch);

int pf_sync(paged_file_t *pf);
