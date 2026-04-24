#include "memory.h"
#include "syscalls.h"

extern void *sysalloc(long size);

void *sysmap_alloc(long size) {
  long total = size + sizeof(sysmap_header);

  // Alinea a múltiplos de 4096
  long aligned = (total + 4095) & ~4095;

  void *addr = (void *)sysalloc(aligned);

  if ((long)addr < 0)
    return 0;

  sysmap_header *h = (sysmap_header *)addr;
  h->size = aligned;
  h->used = 1;

  return (void *)(h + 1);
}

int sysmap_free(void *ptr) {
  if (!ptr)
    return -1;

  sysmap_header *h = ((sysmap_header *)ptr) - 1;
  h->used = 0;

  return sys_munmap((void *)h, h->size);
}
