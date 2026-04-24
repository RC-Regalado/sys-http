#include "client.h"
#include "database.h"
#include "hashmap.h"
#include "io.h"
#include "json.h"
#include "str.h"
#include "syscalls.h"
#include <time.h>

#include "requests.h"

static int starts_with(const char *text, const char *prefix) {
  int i = 0;
  while (prefix[i] != '\0') {
    if (text[i] != prefix[i])
      return 0;
    i++;
  }
  return 1;
}

static long parse_decimal(const char *s) {
  if (!s)
    return -1;

  long value = 0;
  int i = 0;
  while (s[i] == ' ')
    i++;
  while (s[i] >= '0' && s[i] <= '9') {
    value = (value * 10) + (s[i] - '0');
    i++;
  }
  return value;
}

static int read_body(client *cl, line_reader *reader) {
  const char *content_len = hashmap_get(&cl->headers, "Content-Length");
  long body_len = parse_decimal(content_len);

  if (body_len <= 0)
    return 0;

  if (cl->pool.offset + body_len + 1 >= cl->pool.capacity) {
    long need = cl->pool.offset + body_len + 1;
    if (string_pool_relloc(&cl->pool, need * 2) < 0)
      return -1;
  }

  char *body = &cl->pool.base[cl->pool.offset];
  long buffered = reader->write_pos - reader->read_pos;
  if (buffered < 0)
    buffered = 0;
  if (buffered > body_len)
    buffered = body_len;

  for (long i = 0; i < buffered; ++i)
    body[i] = reader->buffer[reader->read_pos + i];

  long copied = buffered;
  while (copied < body_len) {
    long n = read(cl->fd, body + copied, body_len - copied);
    if (n <= 0)
      return -1;
    copied += n;
  }

  body[body_len] = '\0';
  cl->pool.offset += body_len + 1;

  char *key = string_pool_alloc(&cl->pool, "BODY");
  if (!key)
    return -1;
  hashmap_put(&cl->headers, key, body);
  return 0;
}

static const char *skip_ws(const char *p) {
  while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
    p++;
  return p;
}

static int extract_json_key(const char *body, char *out, int out_size) {
  if (!body || !out || out_size < 2)
    return -1;

  const char *p = body;
  while (*p) {
    if (p[0] == '"' && p[1] == 'k' && p[2] == 'e' && p[3] == 'y' &&
        p[4] == '"') {
      p += 5;
      p = skip_ws(p);
      if (*p != ':')
        return -1;
      p++;
      p = skip_ws(p);
      if (*p != '"')
        return -1;
      p++;

      int n = 0;
      while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) {
          p++;
        }
        if (n >= out_size - 1)
          return -1;
        out[n++] = *p++;
      }
      if (*p != '"')
        return -1;
      out[n] = '\0';
      return 0;
    }
    p++;
  }
  return -1;
}

static int write_json_reply(int client, enum request_status status,
                            const char *key, int ok) {
  string_pool storage;
  string_pool body;
  json_object obj;

  if (string_pool_init(&storage, 512) < 0 || string_pool_init(&body, 512) < 0)
    return -1;
  if (json_object_init(&obj, &storage) < 0) {
    string_pool_destroy(&storage);
    string_pool_destroy(&body);
    return -1;
  }

  json_add_bool(&obj, "ok", ok);
  if (key)
    json_add_string(&obj, "key", key);
  else
    json_add_null(&obj, "key");

  if (json_serialize(&obj, &body) < 0) {
    string_pool_destroy(&storage);
    string_pool_destroy(&body);
    return -1;
  }

  write_headers(client, status);
  writef(client, "Content-Type: application/json\r\n");
  writef(client, "Content-Length: %ld\r\n", len(body.base));
  writef(client, "Connection: close\r\n\r\n");
  write(client, body.base, len(body.base));

  string_pool_destroy(&storage);
  string_pool_destroy(&body);
  return 0;
}

void parse_headers(hash_map *map, string_pool *pool, char *data) {
  int colon = index(data, ':');

  int data_len = len(data);
  int value_len = data_len - colon - 1;

  char *key = string_pool_nalloc(pool, data, colon);
  char *value = string_pool_nalloc(pool, data + colon + 1, value_len);

  if ((void *)key < NULL)
    logf("Error en memoria");

  substr(data, key, 0, colon);
  substr(data, value, colon + 2, value_len); //  +2 por el espacio

  hashmap_put(map, key, value);
}

static void want_close(client *cl) {
  const char *conn = hashmap_get(&cl->headers, "Connection");

  if (conn == NULL)
    return;

  if (strcmp(conn, "keep-alive") == 0) {
    cl->state = STATE_KEEP_ALIVE;
    cl->want_close = 0;
  }

  if (strcmp(conn, "close") == 0) {
    cl->want_close = 1;
    cl->state = 0;
  }

  cl->want_close = 1;
}

int read_incoming(client *cl) {
  line_reader reader = {.read_pos = 0, .write_pos = 0};
  reader.fd = cl->fd;
  int pos = 0;
  char *line;

  int n = 0;
  while ((n = readline_stream(&reader, 1024)) > 0) {
    line = &reader.buffer[pos];
    if (pos == 0) {
      char *key = string_pool_alloc(&cl->pool, "REQUEST\0");
      char *value = string_pool_alloc(&cl->pool, line);
      hashmap_put(&cl->headers, key, value);
    } else {
      parse_headers(&cl->headers, &cl->pool, line);
    }
    pos = reader.read_pos;
  }

  if (read_body(cl, &reader) < 0)
    return -1;

  want_close(cl);

  cl->want_read = 0;
  cl->want_write = 1;

  return 0;
}

void write_response(client *cl) {
  string_pool handler;
  string_pool_init(&handler, 1024);

  int client = cl->fd;

  const char *request = hashmap_get(&cl->headers, "REQUEST");

  if (request == 0) {
    // writef(client, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
    write_headers(client, INTERNAL_ERROR);
    string_pool_destroy(&handler);
    return;
  }

  int space_index1 = index(request, ' ');
  int space_index2 = index(request + space_index1 + 1, ' ');

  if (space_index1 < 0 || space_index2 < 0) {
    write_headers(client, UNKNOWN);
    string_pool_destroy(&handler);
    return;
  }
  char type[space_index1 + 1];
  char file[space_index2 + 1];

  substr(request, type, 0, space_index1);
  substr(request, file, space_index1 + 1, space_index2);

  type[space_index1] = '\0';
  file[space_index2] = '\0';

  logf("request type(%s) to %s\n", type, file);

  if (strcmp(file, "..") == 0) {
    write_headers(client, FORBIDDEN);
    string_pool_destroy(&handler);
    return;
  }

  if (strcmp(file, "/") == 0) {
    string_n_copy("/index.html", cl->path, CLIENT_MAX_PATH);
  } else {
    string_n_copy(file[0] == '/' ? file + 1 : file, cl->path, CLIENT_MAX_PATH);
  }

  if (strncmp(type, "GET", 3) == 0) {
    get(cl);
  } else if (strcmp(type, "POST") == 0) {
    post(client, &cl->headers, cl->path);
  }

  string_pool_destroy(&handler);

  want_close(cl);
  cl->want_write = 0;
}

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
    if (sys_getsockopt(*client, SOL_SOCKET, SO_ERROR, &err, &err_len) || err) {
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

void get(client *cl) {
  string_pool handler;
  string_pool_init(&handler, 1024);

  if (starts_with(cl->path, "database/")) {
    const char *id = cl->path + 9;
    if (*id == '\0') {
      write_headers(cl->fd, NOT_FOUND);
      string_pool_destroy(&handler);
      cl->want_close = 1;
      return;
    }
    database_route(cl, id);
    string_pool_destroy(&handler);
    cl->want_close = 1;
    return;
  }

  const char templates_dir[] = "templates/";

  string_pool_mark(&handler);

  char *route = string_pool_alloc(&handler, templates_dir);
  string_pool_append(&handler, cl->path, 1);

  int client = cl->fd;
  int fd = open(route, O_RDONLY);

  struct stat sb;

  if (fd < 0) {
    write_headers(client, NOT_FOUND);
    string_pool_destroy(&handler);
    cl->want_close = 1;
    return;
  }

  if (stat_file(fd, &sb) == -1) {
    logf("Ha ocurrido un error al realizar stat en el archivo: %s\n", route);
    write_headers(client, INTERNAL_ERROR);
    string_pool_destroy(&handler);
    cl->want_close = 1;
    close(fd);
    return;
  }

  int dot = last_index_of(cl->path, '.');
  char *filetype = "application/octet-stream";

  if (dot > -1) {
    unsigned int l = len(cl->path);
    int top = l - dot - 1;

    string_pool_reset_to_mark(&handler);

    substr(cl->path, route, dot + 1, top);
    route[top] = '\0';

    if (strcmp(route, "html") == 0) {
      filetype = "text/html";
    } else if (strcmp(route, "css") == 0) {
      filetype = "text/css";
    } else if (strcmp(route, "js") == 0) {
      filetype = "application/javascript";
    } else if (strcmp(route, "png") == 0) {
      filetype = "image/png";
    } else if (strcmp(route, "jpg") == 0 || strcmp(route, "jpeg") == 0) {
      filetype = "image/jpeg";
    } else if (strcmp(route, "ico") == 0) {
      filetype = "image/vnd.microsoft.icon";
    } else if (strcmp(route, "mp4") == 0) {
      chunk(&client, &fd, &cl->headers, cl->path, route);
      string_pool_destroy(&handler);
      close(fd);
      return;
    }
  }

  write_headers(client, OK);

  string_pool_reset(&handler);

  string_pool_format(&handler, "Content-Type: %s\r\n", filetype);
  string_pool_format(&handler, "Content-Length: %ld\r\n", sb.st_size);

  /*
  if (cl->state == STATE_KEEP_ALIVE)
    string_pool_append(&handler, "Connection: keep-alive\r\n\r\n", 0);
  else
                       */
  // Aun no entiendo como funciona XD
  string_pool_append(&handler, "Connection: close\r\n\r\n", 0);
  //  string_pool_append(&handler, "\r\n", 0);

  // Se restan 2 porque:
  // 1 -> offset es size+1
  // 2 -> size incluye \0 que no se envía en cabeceras
  write(client, handler.base, handler.offset - 2);

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
  if (strcmp(path, "database") == 0) {
    const char *body = hashmap_get(headers, "BODY");
    char key[128];

    if (!body || extract_json_key(body, key, sizeof(key)) < 0) {
      write_json_reply(client, UNKNOWN, 0, 0);
      return;
    }

    int saved = database_set(key, body, len(body));
    if (saved == 0)
      write_json_reply(client, OK, key, 1);
    else
      write_json_reply(client, INTERNAL_ERROR, key, 0);
    return;
  }

  string_pool json_storage;
  string_pool response;
  json_object obj;

  if (string_pool_init(&json_storage, 512) < 0 ||
      string_pool_init(&response, 1024) < 0) {
    write_headers(client, INTERNAL_ERROR);
    return;
  }

  if (json_object_init(&obj, &json_storage) < 0) {
    write_headers(client, INTERNAL_ERROR);
    string_pool_destroy(&json_storage);
    string_pool_destroy(&response);
    return;
  }

  const char *accept = hashmap_get(headers, "Accept");
  json_add_string(&obj, "status", "ok");
  json_add_number(&obj, "hex", 10);
  json_add_string(&obj, "path", path ? path : "/");
  if (accept) {
    json_add_string(&obj, "accept", accept);
  } else {
    json_add_null(&obj, "accept");
  }

  if (json_serialize(&obj, &response) < 0) {
    write_headers(client, INTERNAL_ERROR);
    string_pool_destroy(&json_storage);
    string_pool_destroy(&response);
    return;
  }

  int size = len(response.base);

  write_headers(client, OK);
  writef(client, "Content-Type: application/json\r\n");
  writef(client, "Content-Length: %ld\r\n", size);
  writef(client, "Connection: close\r\n");
  write(client, "\r\n", 2);
  write(client, response.base, size);

  string_pool_destroy(&json_storage);
  string_pool_destroy(&response);
}
