#include "inc/requests.h"
#include "inc/hashmap.h"
#include "inc/io.h"
#include "inc/str.h"

extern long _getsockopt(int fd, void *optval, unsigned int *optlen);

void write_headers(int client, enum request_status status) {
  // El OK sin doble retorno ya que se espera haya mas datos
  // luego de recibir esta cabecera
  switch (status) {
  case OK:
    writef(client, "HTTP/1.1 200 OK\r\n");
    break;
  case INTERNAL_ERROR:
    writef(client, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
    break;
  case FORBIDDEN:
    writef(client, "HTTP/1.1 403 Forbidden\r\n\r\n");
    break;
  case NOT_FOUND:
    writef(client, "HTTP/1.1 404 Not Found\r\n\r\n");
    break;
  default:
    writef(client, "HTTP/1.1 400 Bad Request\r\n\r\n");
  }
}

void chunk(int *client, int *fd, hash_map *header, const char *path,
           const char *ext) {
  //  string_pool response;

  write_headers(*client, OK);
  char *headers = "Content-Type: video/mp4\r\n"
                  "Transfer-Encoding: chunked\r\n"
                  "Connection: close\r\n\r\n";
  write(*client, headers, len(headers));
  // Lectura de video por fragmentos de 256 bytes
  int bytes_read = 0;
  char buffer[256];
  int err = 0;
  unsigned int err_len = sizeof(err);

  long dead = 0;

  while (!dead && (bytes_read = read(*fd, buffer, 256)) > 0) {
    if (_getsockopt(*client, &err, &err_len) || err) {
      dead = 1;
      break;
    }
    // Envía chunk: <tamaño>\r\n<data>\r\n
    writef(*client, "%x\r\n", bytes_read); // Tamaño en hex
    write(*client, buffer, bytes_read);    // Datos
    write(*client, "\r\n", 2);             // Terminador de chunk
  }
  // Cierre de conexión (si existe)
  if (!dead)
    writef(*client, "0\r\n\r\n");
}

void get(int client, hash_map *header, const char *path) {
  string_pool handler;
  string_pool_init(&handler, 1024);

  const char templates_dir[] = "templates/";

  string_pool_mark(&handler);

  char *route = string_pool_alloc(&handler, templates_dir);
  string_pool_append(&handler, path, 1);

  int fd = open(route, O_RDONLY);

  struct stat sb;

  if (fd < 0) {
    write_headers(client, NOT_FOUND);
    string_pool_destroy(&handler);
    return;
  }

  if (stat_file(fd, &sb) == -1) {
    logf("Ha ocurrido un error al realizar stat en el archivo: %s\n", route);
    write_headers(client, INTERNAL_ERROR);
    string_pool_destroy(&handler);
    close(fd);
    return;
  }

  int dot = last_index_of(path, '.');
  char *filetype = "application/octet-stream";

  if (dot > -1) {
    unsigned int l = len(path);
    int top = l - dot - 1;

    string_pool_reset_to_mark(&handler);

    substr(path, route, dot + 1, top);
    route[top] = '\0';

    if (strcmp(route, "html") == 0) {
      filetype = "text/html";
    } else if (strcmp(route, "css") == 0) {
      filetype = "text/css";
    } else if (strcmp(route, "js") == 0) {
      filetype = "application/javascript";
    } else if (strcmp(route, "png") == 0) {
      filetype = "image/png";
    } else if (strcmp(route, "jpg") == 0 || strcmp(path, "jpeg") == 0) {
      filetype = "image/jpeg";
    } else if (strcmp(route, "mp4") == 0) {
      chunk(&client, &fd, header, path, route);
      string_pool_destroy(&handler);
      close(fd);
      return;
    }
  }

  write_headers(client, OK);
  writef(client, "Content-Type: %s\r\n", filetype);
  writef(client, "Content-Length: %ld\r\n", sb.st_size);
  writef(client, "Connection: close\r\n");
  write(client, "\r\n", 2);

  string_pool_reset(&handler);

  long off = 0;
  long remaining = sb.st_size;
  while (remaining > 0) {
    long n = sendfile(client, fd, &off, remaining);
    if (n > 0) {
      remaining -= n;
      continue;
    }
    if (n == 0)
      break; // nada más que enviar
    break;
  }

  string_pool_destroy(&handler);
  close(fd);
}

void post(int client, hash_map *headers, const char *path) {
  const char response[] = "{'hex': ";
  int size = len(response) + 5;

  write_headers(client, OK);
  writef(client, "Content-Type: %s\r\n", hashmap_get(headers, "Accept"));
  writef(client, "Content-Length: %ld\r\n", size);
  writef(client, "Connection: close\r\n");
  write(client, "\r\n", 2);

  writef(client, "%s '%x'}", response, 10);
}
