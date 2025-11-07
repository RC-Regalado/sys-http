#ifndef CLIENT_H_
#define CLIENT_H_

#include "io.h"

typedef struct client_ctx {
  int fd;
  int state;

  int want_read;
  int want_write;
  int want_close;

  char read_buf[READ_BUF_SIZE];
  int read_pos;

  char write_buff[WRITE_BUF_SIZE];
  int write_len;
  int write_pos;

  time_t last_active;
  void *usr_data;
} client_ctx;

#endif // !CLIENT_H_
