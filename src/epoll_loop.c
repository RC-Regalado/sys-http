/**
 * @file epoll_loop.c
 * @brief Manejador principal de eventos para clientes HTTP usando epoll
 */

#include "epoll_loop.h"
#include "client.h"
#include "io.h"
#include "requests.h"
#include "syscalls.h"

#include <sys/epoll.h>

#define MAX_EVENTS 128
#define CLIENT_CAPACITY 1024

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
      if (c == 0x0) {
        c = client_create(fd);
        clients[i] = c;
      }

      c->fd = fd;
      return c;
    }
  }
  return NULL;
}

static void forget_client(client *c) {
  for (int i = 0; i < CLIENT_CAPACITY; ++i) {
    if (clients[i] == c) {
      clients[i] = 0x0;
      return;
    }
  }
}
/**
 * @brief Loop principal del servidor basado en epoll
 * @param sockfd Socket de escucha
 */
void event_loop(int sockfd) {
  struct epoll_event ev, events[MAX_EVENTS];
  int epfd = sys_epoll_create1(0);

  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = sockfd;
  sys_epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);

  while (1) {
    int nfds = sys_epoll_wait(epfd, events, MAX_EVENTS, -1);

    for (int i = 0; i < nfds; ++i) {
      int fd = events[i].data.fd;
      client *c;

      if (fd == sockfd) {
        int client_fd = sys_accept(sockfd, 0, 0);
        c = new_client(client_fd);

        if (!c) {
          logf("No hay espacio para nuevos clientes\n");
          close(client_fd);
          continue;
        }

        ev.events = EPOLLIN;
        ev.data.fd = client_fd;
        sys_epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);
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
          int dead_fd = c->fd;
          logf("Error leyendo de cliente %d\n", dead_fd);
          sys_epoll_ctl(epfd, EPOLL_CTL_DEL, dead_fd, 0);
          forget_client(c);
          client_destroy(c);
          continue;
        }
      }

      if (c->want_write) {
        write_response(c);
      }

      if (c->want_close) {
        //        write_response(c->fd, &c->headers);
        sys_epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, 0);
        forget_client(c);
        client_destroy(c);
      } else {
        c->want_read = 1;
        ev.events = EPOLLIN;
        ev.data.fd = c->fd;
        sys_epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &ev);
        logf("Cliente %d reciclado\n", c->fd);
      }
    }
  }
}
