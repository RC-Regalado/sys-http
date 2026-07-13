#include "paged_file.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static int write_sb(paged_file_t *pf) {
  page_t p;
  memset(&p, 0, sizeof p);
  memcpy(p.bytes, &pf->sb, sizeof pf->sb);
  return pf_write_page(pf, 0, &p);
}

static int read_sb(paged_file_t *pf) {
  page_t p;
  if (pf_read_page(pf, 0, &p) != 0)
    return -1;
  memcpy(&pf->sb, p.bytes, sizeof pf->sb);
  if (pf->sb.magic != SB_MAGIC || pf->sb.version != SB_VERSION)
    return -1;
  return 0;
}

int pf_open(paged_file_t *pf, const char *path, int create) {
  memset(pf, 0, sizeof *pf);
  pf->fd = open(path, create ? (O_CREAT | O_RDWR | O_TRUNC) : O_RDWR, 0644);
  if (pf->fd < 0)
    return -1;

  if (create) {
    pf->sb.magic = SB_MAGIC;
    pf->sb.version = SB_VERSION;
    pf->sb.page_count = 1; // page 0 = superblock
    pf->sb.free_head = 0;
    if (write_sb(pf) != 0)
      return -1;
    if (pf_sync(pf) != 0)
      return -1;
  } else {
    if (read_sb(pf) != 0)
      return -1;
  }
  return 0;
}

int pf_close(paged_file_t *pf) {
  if (!pf)
    return 0;
  pf_sync(pf);
  close(pf->fd);
  pf->fd = -1;
  return 0;
}

int pf_read_page(paged_file_t *pf, uint32_t pid, page_t *out) {
  off_t off = (off_t)pid * PAGE_SIZE;
  ssize_t r = pread(pf->fd, out->bytes, PAGE_SIZE, off);
  return (r == PAGE_SIZE) ? 0 : -1;
}

int pf_write_page(paged_file_t *pf, uint32_t pid, const page_t *p) {
  off_t off = (off_t)pid * PAGE_SIZE;
  ssize_t w = pwrite(pf->fd, p->bytes, PAGE_SIZE, off);
  return (w == PAGE_SIZE) ? 0 : -1;
}

int pf_alloc_page(paged_file_t *pf, uint32_t *out_pid, page_t *out_page) {
  uint32_t pid;
  if (pf->sb.free_head != 0) {
    pid = pf->sb.free_head;
    page_t tmp;
    pf_read_page(pf, pid, &tmp);
    pf->sb.free_head = ((page_hdr_t *)tmp.bytes)->next_free;
  } else {
    pid = pf->sb.page_count++;
  }

  page_init(out_page, PAGE_DATA);
  if (pf_write_page(pf, pid, out_page) != 0)
    return -1;
  write_sb(pf);

  if (out_pid)
    *out_pid = pid;
  return 0;
}

int pf_free_page(paged_file_t *pf, uint32_t pid, page_t *scratch) {
  page_init(scratch, PAGE_FREE);
  ((page_hdr_t *)scratch->bytes)->next_free = pf->sb.free_head;
  pf->sb.free_head = pid;
  pf_write_page(pf, pid, scratch);
  write_sb(pf);
  return 0;
}

int pf_sync(paged_file_t *pf) { return fdatasync(pf->fd); }
