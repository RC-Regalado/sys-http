#ifndef DATABASE_H_
#define DATABASE_H_

#include "client.h"
#include "str.h"

int database_route(client *cl, const char *id);
int database_set(const char *key, const char *payload, unsigned int payload_len);
int database_list(client *cl, const char *namespace_name);
int database_add_note(const char *markdown, unsigned int markdown_len,
                      string_pool *out_key);

#endif // DATABASE_H_
