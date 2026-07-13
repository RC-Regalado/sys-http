#include <sys/stat.h>

#include "epoll_loop.h"
#include "io.h"
#include "str.h"
#include "syscalls.h"

#define AF_INET 2
#define SOCK_STREAM 1

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

void server() {
  struct sockaddr_in addr;
  int port = 5050;
  int enable = 1;

  logf("Iniciando el servicio en el puerto %d \n", port);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = 0; // INADDR_ANY
  for (int i = 0; i < 8; ++i)
    addr.zero[i] = 0;

  int sockfd = sys_socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    logf("Ha ocurrido un error al iniciar el socket.\n");
    return;
  }

  sys_setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

  if (sys_bind(sockfd, &addr, sizeof(addr)) < 0) {
    logf("El puerto ya está en uso!\n");
    return;
  }
  sys_listen(sockfd, 5);

  event_loop(sockfd);
}
