#pragma once
#include "hash.h"
#include <stdint.h>

int mmap_load_into_ht(const char *path, ht_t *ht);
