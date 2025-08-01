#include "inc/str.h"

unsigned int len(const char *str) {
  int i = 0;
  while (str[i] != 0)
    i++;
  return i;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }

  return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}
