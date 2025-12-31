#ifndef EPOLL_LOOP_H_
#define EPOLL_LOOP_H_

#define SYS_EPOLL_CREATE1 291
#define SYS_EPOLL_CTL 233
#define SYS_EPOLL_WAIT 232
#define SYS_ACCEPT 43

void event_loop(int sockfd);

#endif // !EPOLL_LOOP_H_
