#ifndef EPOLL_LOOP_H_
#define EPOLL_LOOP_H_

#include "hashmap.h"
#include "str.h"
#define SYS_EPOLL_CREATE1 291
#define SYS_EPOLL_CTL 233
#define SYS_EPOLL_WAIT 232
#define SYS_ACCEPT 43

void write_response(int fd, hash_map *headers);
int read_incoming(int fb, string_pool *pool, hash_map *headers);

#endif // !EPOLL_LOOP_H_
