#ifndef CLIENT_H_
#define CLIENT_H_

#include "hashmap.h"
#include "io.h"
#include "str.h"
#include <time.h>

// 1024 alcanzaba para curl pero no para las cabeceras reales de un
// navegador (cookies, User-Agent, Accept-*, Sec-Fetch-*...); agotaba el
// pool y el fallo de alloc terminaba en NULL deref (ver parse_header_line).
#define CLIENT_BUF_SIZE 8192
#define CLIENT_MAX_PATH 256

#define STATE_KEEP_ALIVE 1

#define EINTR 4 /* Interrupted system call */
typedef enum {
  RESPONSE_IDLE,
  RESPONSE_HEADERS,
  RESPONSE_FILE,
  RESPONSE_DONE
} response_state;

typedef struct {
  int fd;
  int want_read;
  int want_write;
  int want_close;

  string_pool pool;
  hash_map headers;
  hash_map query;
  line_reader reader;
  int request_line_seen;

  time_t last_active;
  void *usr_data;

  char method[8];
  char path[CLIENT_MAX_PATH];
  int state;

  response_state response_state;

  char *response_headers;
  long response_headers_length;
  long response_headers_offset;

  int file_fd;
  long file_offset;
  long file_remaining;
} client;

client *client_create(int fd);
void client_destroy(client *c);
void client_reset(client *c);
#endif // !CLIENT_H_
