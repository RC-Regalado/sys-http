#ifndef __STR_H
#define __STR_H

typedef struct {
  char *value;
  unsigned int size;
} string;

unsigned int len(const char *string);
int strcmp(const char *s1, const char *s2);

#endif // __STR_H
