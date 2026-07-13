#ifndef __REQUESTS_H
#define __REQUESTS_H

#include "client.h"

enum request_status { OK, INTERNAL_ERROR, FORBIDDEN, NOT_FOUND, UNKNOWN };

void write_headers(int client, enum request_status status);

void write_response(client *cl);
int read_incoming(client *cl);

#endif // !__REQUESTS_H
