#ifndef __REQUESTS_H
#define __REQUESTS_H

#include "hashmap.h"

enum request_status { OK, INTERNAL_ERROR, FORBIDDEN, NOT_FOUND, UNKNOWN };

void write_headers(int client, enum request_status status);

void get(int client, hash_map *headers, const char *path);
void post(int client, hash_map *headers, const char *path);

#endif // !__REQUESTS_H
