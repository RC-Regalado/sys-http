#ifndef CLIENT_H_
#define CLIENT_H_

#include "hashmap.h"
#include "str.h"
#include <time.h>

#define CLIENT_BUF_SIZE 1024
#define CLIENT_MAX_PATH 256

typedef struct {
  int fd;
  int want_read;
  int want_write;
  int want_close;

  string_pool pool;
  hash_map headers;

  time_t last_active;
  void *usr_data;

  char method[8];
  char path[CLIENT_MAX_PATH];
  int state;
} client;

client *client_create(int fd);
void client_destroy(client *c);
void client_reset(client *c);
#endif // !CLIENT_H_
