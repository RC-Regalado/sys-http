#ifndef DATABASE_H_
#define DATABASE_H_

#include "client.h"

int database_route(client *cl, const char *id);
int database_set(const char *key, const char *payload, unsigned int payload_len);

#endif // DATABASE_H_
