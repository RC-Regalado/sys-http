// client.c
#include "client.h"
#include "files.h"
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
  fd_set_nonblock(fd);

  c->fd = fd;
  c->want_read = 1;
  c->want_write = 0;
  c->want_close = 0;
  c->state = 0;
  c->reader.fd = fd;
  c->reader.read_pos = 0;
  c->reader.write_pos = 0;
  c->request_line_seen = 0;
  c->response_state = RESPONSE_IDLE;
  c->response_headers = 0;
  c->response_headers_length = 0;
  c->response_headers_offset = 0;
  c->file_fd = -1;
  c->file_offset = 0;
  c->file_remaining = 0;

  string_pool_init(&c->pool, CLIENT_BUF_SIZE);
  hashmap_init(&c->headers);
  hashmap_init(&c->query);

  return c;
}

/**
 * @brief Libera los recursos usados por el cliente
 */
void client_destroy(client *c) {
  if (!c)
    return;
  if (c->file_fd >= 0)
    close(c->file_fd);
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
  c->reader.fd = c->fd;
  c->reader.read_pos = 0;
  c->reader.write_pos = 0;
  c->request_line_seen = 0;
  c->response_state = RESPONSE_IDLE;
  c->response_headers = 0;
  c->response_headers_length = 0;
  c->response_headers_offset = 0;
  if (c->file_fd >= 0)
    close(c->file_fd);
  c->file_fd = -1;
  c->file_offset = 0;
  c->file_remaining = 0;
  c->method[0] = 0;
  c->path[0] = 0;

  c->fd = -1;

  hashmap_reset(&c->headers);
  hashmap_reset(&c->query);
  string_pool_reset(&c->pool);
}
