#include "files.h"

#include "client.h"
#include "hashmap.h"
#include "io.h"
#include "requests.h"
#include "str.h"
#include "syscalls.h"

void fd_set_nonblock(int fd) {
  long flags = sys_fcntl(fd, F_GETFL, 0);

  logf("[%s] :: Flags result %ld\n", __FILE_NAME__, flags);
  if (flags < 0)
    return;

  flags |= O_NONBLOCK;

  flags = sys_fcntl(fd, F_SETFL, flags);

  if (flags) {
    logf("Syscall fcntl error, result %l", flags);
  }
}

static char *content_type_for(const char *path, string_pool *scratch) {
  int dot = last_index_of(path, '.');
  if (dot < 0)
    return "application/octet-stream";

  unsigned int l = len(path);
  int top = l - dot - 1;
  char *ext = string_pool_nalloc(scratch, path + dot + 1, top);
  if (!ext)
    return "application/octet-stream";

  if (strcmp(ext, "html") == 0)
    return "text/html";
  if (strcmp(ext, "css") == 0)
    return "text/css";
  if (strcmp(ext, "js") == 0)
    return "application/javascript";
  if (strcmp(ext, "png") == 0)
    return "image/png";
  if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
    return "image/jpeg";
  if (strcmp(ext, "ico") == 0)
    return "image/vnd.microsoft.icon";
  if (strcmp(ext, "mp4") == 0)
    return "video/mp4";

  return "application/octet-stream";
}

static void stream_chunked_mp4(client *cl, int fd) {
  int client_fd = cl->fd;

  write_headers(client_fd, OK);
  char *headers = "Content-Type: video/mp4\r\n"
                  "Transfer-Encoding: chunked\r\n"
                  "Connection: close\r\n\r\n";
  write(client_fd, headers, len(headers));

  int bytes_read = 0;
  char buffer[256];
  int err = 0;
  unsigned int err_len = sizeof(err);
  long dead = 0;

  while (!dead && (bytes_read = read(fd, buffer, 256)) > 0) {
    if (sys_getsockopt(client_fd, SOL_SOCKET, SO_ERROR, &err, &err_len) ||
        err) {
      dead = 1;
      break;
    }
    writef(client_fd, "%x\r\n", bytes_read);
    write(client_fd, buffer, bytes_read);
    write(client_fd, "\r\n", 2);
  }

  if (!dead)
    writef(client_fd, "0\r\n\r\n");
}

void send_static_pending(client *cl) {
  while (cl->response_headers_offset < cl->response_headers_length) {
    long left = cl->response_headers_length - cl->response_headers_offset;
    long n = write(cl->fd, cl->response_headers + cl->response_headers_offset,
                   left);

    if (n > 0) {
      cl->response_headers_offset += n;
      continue;
    }

    if (n == 0 || n == -EAGAIN || n == -EWOULDBLOCK) {
      cl->response_state = RESPONSE_HEADERS;
      cl->want_read = 0;
      cl->want_write = 1;
      cl->want_close = 0;
      return;
    }

    if (n == -EINTR)
      continue;

    cl->want_write = 0;
    cl->want_close = 1;
    return;
  }

  while (cl->file_remaining > 0) {
    long n = sendfile(cl->fd, cl->file_fd, &cl->file_offset,
                      cl->file_remaining);

    if (n > 0) {
      cl->file_remaining -= n;
      continue;
    }

    if (n == 0 || n == -EAGAIN || n == -EWOULDBLOCK) {
      cl->response_state = RESPONSE_FILE;
      cl->want_read = 0;
      cl->want_write = 1;
      cl->want_close = 0;
      return;
    }

    if (n == -EINTR)
      continue;

    // Error real de sendfile (ni EAGAIN/EWOULDBLOCK ni EINTR): la respuesta
    // ya prometio Content-Length completo en los headers, asi que la unica
    // opcion es cortar la conexion -- pero se deja registrado como error,
    // no como transferencia completa (antes caia al mismo cierre "exitoso"
    // de abajo con file_remaining > 0, dejando al cliente con body truncado
    // sin ninguna traza de que algo fallo).
    logf("Error en sendfile (fd=%d, restante=%ld): %ld\n", cl->fd,
         cl->file_remaining, n);
    break;
  }

  if (cl->file_fd >= 0) {
    close(cl->file_fd);
    cl->file_fd = -1;
  }
  cl->file_remaining = 0;
  cl->response_state = RESPONSE_DONE;
  cl->want_write = 0;
  cl->want_close = 1;
}

void serve_static_file(client *cl) {
  string_pool handler;
  string_pool_init(&handler, 1024);

  const char templates_dir[] = "templates/";
  char *route = string_pool_alloc(&handler, templates_dir);
  string_pool_append(&handler, cl->path, 1);

  int client_fd = cl->fd;
  int fd = open(route, O_RDONLY);
  struct stat sb;

  if (fd < 0) {
    write_headers(client_fd, NOT_FOUND);
    string_pool_destroy(&handler);
    cl->want_close = 1;
    return;
  }

  if (stat_file(fd, &sb) == -1) {
    logf("Ha ocurrido un error al realizar stat en el archivo: %s\n", route);
    write_headers(client_fd, INTERNAL_ERROR);
    string_pool_destroy(&handler);
    cl->want_close = 1;
    close(fd);
    return;
  }

  char *filetype = content_type_for(cl->path, &handler);
  if (strcmp(filetype, "video/mp4") == 0) {
    stream_chunked_mp4(cl, fd);
    string_pool_destroy(&handler);
    close(fd);
    cl->want_close = 1;
    return;
  }

  string_pool_reset(&handler);
  string_pool_append(&handler, "HTTP/1.1 200 OK\r\n", 0);
  string_pool_format(&handler, "Content-Type: %s\r\n", filetype);
  string_pool_format(&handler, "Content-Length: %ld\r\n", sb.st_size);
  string_pool_append(&handler, "Connection: close\r\n\r\n", 0);

  cl->response_headers =
      string_pool_nalloc(&cl->pool, handler.base, handler.offset - 1);
  if (!cl->response_headers) {
    write_headers(client_fd, INTERNAL_ERROR);
    string_pool_destroy(&handler);
    close(fd);
    cl->want_close = 1;
    return;
  }

  cl->response_state = RESPONSE_HEADERS;
  cl->response_headers_length = handler.offset - 1;
  cl->response_headers_offset = 0;
  cl->file_fd = fd;
  cl->file_offset = 0;
  cl->file_remaining = sb.st_size;

  string_pool_destroy(&handler);
  send_static_pending(cl);
}
