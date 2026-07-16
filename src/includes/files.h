#ifndef FILES_H_
#define FILES_H_

#include "client.h"

// File descriptor flags
#define F_GETFL 3
#define F_SETFL 4

struct pollfd {
  int fd;
  short int events;
  short int revents;
};

void fd_set_nonblock(int fd);
void serve_static_file(client *cl);

#endif // !FILES_H_
