#ifndef __MEMORY_H
#define __MEMORY_H

#ifndef NULL
#define NULL 0x0
#endif // !NULL

typedef struct {
  long size;
  int used;
} sysmap_header;

void *sysmap_alloc(long size);
int sysmap_free(void *ptr);

#endif // !__MEMORY_H
