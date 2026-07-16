#include <sys/stat.h>

#include "epoll_loop.h"
#include "io.h"
#include "str.h"
#include "syscalls.h"

#define AF_INET 2
#define AF_INET6 10
#define SOCK_STREAM 1
#define IPPROTO_IPV6 41
#define IPV6_V6ONLY 26

extern void _start();

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

static void init_ipv4_addr(struct sockaddr_in *addr, int port) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  addr->sin_addr = 0; // INADDR_ANY
  for (int i = 0; i < 8; ++i)
    addr->zero[i] = 0;
}

static int listen_ipv4(int port) {
  struct sockaddr_in addr;
  int enable = 1;
  int sockfd = sys_socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    return -1;
  }

  sys_setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  init_ipv4_addr(&addr, port);

  if (sys_bind(sockfd, &addr, sizeof(addr)) < 0) {
    close(sockfd);
    return -1;
  }
  sys_listen(sockfd, 5);
  return sockfd;
}

void server() {
  int port = 5050;

  logf("Iniciando el servicio en el puerto %d \n", port);

  int sockfd = listen_ipv4(port);

  if (sockfd < 0) {
    logf("Ha ocurrido un error al iniciar el socket.\n");
    return;
  }

  event_loop(sockfd);
}
