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
#define SIGPIPE 13
#define SIG_IGN 1
#define SYS_RT_SIGACTION 13

extern void _start();

struct sockaddr_in {
  unsigned short sin_family;
  unsigned short sin_port;
  unsigned int sin_addr;
  char zero[8];
};

struct sockaddr_in6 {
  unsigned short sin6_family;
  unsigned short sin6_port;
  unsigned int sin6_flowinfo;
  unsigned char sin6_addr[16];
  unsigned int sin6_scope_id;
};

struct kernel_sigaction {
  void (*handler)(int);
  unsigned long flags;
  void (*restorer)(void);
  unsigned long mask;
};

unsigned short htons(unsigned short x) {
  asm("xchg %h0, %b0" : "+Q"(x)); // intercambia los bytes del registro
  return x;
}

static void ignore_sigpipe(void) {
  struct kernel_sigaction act;
  act.handler = (void (*)(int))SIG_IGN;
  act.flags = 0;
  act.restorer = 0;
  act.mask = 0;
  sys_call4(SYS_RT_SIGACTION, SIGPIPE, (long)&act, 0, 8);
}

static void init_ipv4_addr(struct sockaddr_in *addr, int port) {
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  addr->sin_addr = 0; // INADDR_ANY
  for (int i = 0; i < 8; ++i)
    addr->zero[i] = 0;
}

static void init_ipv6_addr(struct sockaddr_in6 *addr, int port) {
  addr->sin6_family = AF_INET6;
  addr->sin6_port = htons(port);
  addr->sin6_flowinfo = 0;
  for (int i = 0; i < 16; ++i)
    addr->sin6_addr[i] = 0;
  addr->sin6_scope_id = 0;
}

static int listen_ipv6(int port) {
  struct sockaddr_in6 addr;
  int enable = 1;
  int v6only = 0;
  int sockfd = sys_socket(AF_INET6, SOCK_STREAM, 0);

  if (sockfd < 0) {
    return -1;
  }

  sys_setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  sys_setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(int));
  init_ipv6_addr(&addr, port);

  if (sys_bind(sockfd, &addr, sizeof(addr)) < 0) {
    close(sockfd);
    return -1;
  }
  sys_listen(sockfd, 5);
  return sockfd;
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

  ignore_sigpipe();
  logf("Iniciando el servicio en el puerto %d \n", port);

  int sockfd = listen_ipv6(port);
  if (sockfd < 0)
    sockfd = listen_ipv4(port);

  if (sockfd < 0) {
    logf("Ha ocurrido un error al iniciar el socket.\n");
    return;
  }

  event_loop(sockfd);
}
