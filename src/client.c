// client.c
#include "client.h"
#include "hashmap.h"
#include "io.h"
#include "memory.h"

/**
 * @brief Crea y configura un nuevo cliente
 *
 * @param fd File descriptor del socket aceptado
 * @return Client* puntero al cliente creado
 */
client *client_create(int fd) {

  client *c = sysmap_alloc(sizeof(client));
  if (!c)
    return 0;

  c->fd = fd;
  c->want_read = 1;
  c->want_write = 0;
  c->want_close = 0;
  c->state = 0;

  string_pool_init(&c->pool, CLIENT_BUF_SIZE);
  hashmap_init(&c->headers);

  return c;
}

/**
 * @brief Libera los recursos usados por el cliente
 */
void client_destroy(client *c) {
  if (!c)
    return;
  string_pool_destroy(&c->pool);
  close(c->fd);
  sysmap_free(c);
}

/**
 * @brief Limpia el cliente para reutilizar la misma instancia
 */
void client_reset(client *c) {
  if (!c)
    return;
  c->want_read = 1;
  c->want_write = 0;
  c->want_close = 0;
  c->state = 0;
  c->method[0] = 0;
  c->path[0] = 0;

  c->fd = -1;

  hashmap_reset(&c->headers);
  string_pool_reset(&c->pool);
}
