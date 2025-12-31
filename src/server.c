#include <sys/stat.h>

#include "epoll_loop.h"
#include "io.h"
#include "str.h"

#define SYS_SOCKET 41
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_ACCEPT 43

#define AF_INET 2
#define SOCK_STREAM 1

extern void _start();
extern long syscall3(long syscall, long rdi, long rsi, long rdx);
extern long setsockopt(long fd, long optval, long optlen);

struct sockaddr_in {
  unsigned short sin_family;
  unsigned short sin_port;
  unsigned int sin_addr;
  char zero[8];
};

unsigned short htons(unsigned short x) {
  asm("xchg %h0, %b0" : "+Q"(x)); // intercambia los bytes del registro
  return x;
}

void server() {
  struct sockaddr_in addr = {0}; // inicializa todo a 0
  string_pool builder;
  string_pool_init(&builder, 1024);

  int port = 5050;
  int enable = 1;

  char *fmt = builder.base;

  if (string_pool_format(&builder, "Hola mundo el puerto es %d \n", port) < 0) {
    return;
  }

  logf(fmt);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = 0; // INADDR_ANY

  int sockfd = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    logf("Ha ocurrido un error al iniciar el socket.\n");
    return;
  }

  setsockopt(sockfd, (long)&enable, sizeof(int));

  if (syscall3(SYS_BIND, sockfd, (long)&addr, sizeof(addr)) < 0) {
    logf("El puerto ya está en uso!\n");
    return;
  }
  syscall3(SYS_LISTEN, sockfd, 5, 0);

  event_loop(sockfd);
}
