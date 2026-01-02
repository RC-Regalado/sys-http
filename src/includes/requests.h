#ifndef __REQUESTS_H
#define __REQUESTS_H

#include "client.h"
#include "hashmap.h"

enum request_status { OK, INTERNAL_ERROR, FORBIDDEN, NOT_FOUND, UNKNOWN };

void write_headers(int client, enum request_status status);

void write_response(client *cl);
int read_incoming(client *cl);

void get(client *cl);
void post(int client, hash_map *headers, const char *path);

#endif // !__REQUESTS_H
