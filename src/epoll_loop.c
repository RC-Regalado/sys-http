/**
 * @file epoll_loop.c
 * @brief Manejador principal de eventos para clientes HTTP usando epoll
 */

#include "epoll_loop.h"
#include "client.h"
#include "io.h"
#include "requests.h"
#include "syscall.h"

#include <sys/epoll.h>
#include <sys/syscall.h>
// #include <unistd.h>

#define MAX_EVENTS 128
#define CLIENT_CAPACITY 1024

extern long syscall3(long syscall, long rdi, long rsi, long rdx);

extern int sys_epoll_create1();
extern void sys_epoll_ctl(long epfd, long epoll_ctl_add, long sockfd, long ev);
extern int sys_epoll_wait(long epfd, long events, long max_events);

/** Array estático de clientes */
static client *clients[CLIENT_CAPACITY];

/**
 * @brief Busca un cliente dado su file descriptor
 */
client *get_client(int fd) {
  for (int i = 0; i < CLIENT_CAPACITY; ++i) {
    client *c = clients[i];
    if (c != 0x0 && c->fd == fd)
      return clients[i];
  }
  return NULL;
}

/**
 * @brief Obtiene un cliente libre para asociar a un nuevo socket
 */
client *new_client(int fd) {
  for (int i = 0; i < CLIENT_CAPACITY; ++i) {
    client *c = clients[i];
    if (c == 0x0 || c->fd < 0) {
      if (c == 0x0)
        c = client_create(fd);

      c->fd = fd;
      return c;
    }
  }
  return NULL;
}
/**
 * @brief Loop principal del servidor basado en epoll
 * @param sockfd Socket de escucha
 */
void event_loop(int sockfd) {
  struct epoll_event ev, events[MAX_EVENTS];
  int epfd = sys_epoll_create1();

  ev.events = EPOLLIN;
  ev.data.fd = sockfd;
  sys_epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, (long)&ev);

  while (1) {
    int nfds = sys_epoll_wait(epfd, (long)events, MAX_EVENTS);

    for (int i = 0; i < nfds; ++i) {
      int fd = events[i].data.fd;
      client *c;

      if (fd != sockfd) {
        int client_fd = syscall3(SYS_ACCEPT, sockfd, 0, 0);
        c = new_client(client_fd);

        if (!c) {
          logf("No hay espacio para nuevos clientes\n");
          close(client_fd);
          continue;
        }

        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        sys_epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, (long)&ev);
        logf("Cliente %d aceptado\n", client_fd);

      } else {
        c = get_client(fd);
        if (!c) {
          logf("Cliente no encontrado: %d\n", fd);
          continue;
        }
      }

      if (c->want_read) {
        int status = read_incoming(c);
        if (status < 0) {
          logf("Error leyendo de cliente %d\n", c->fd);
          client_destroy(c);
          syscall3(SYS_EPOLL_CTL, epfd, EPOLL_CTL_DEL, c->fd);
          close(c->fd);
          continue;
        }
      }

      if (c->want_write) {
        write_response(c);
      }

      if (c->want_close) {
        //        write_response(c->fd, &c->headers);
        syscall3(SYS_EPOLL_CTL, epfd, EPOLL_CTL_DEL, c->fd);
        client_destroy(c);
      } else {
        c->want_read = 1;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = c->fd;
        sys_epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, (long)&ev);
        logf("Cliente %d reciclado\n", c->fd);
      }
    }
  }
}
