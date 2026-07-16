#include "client.h"
#include "handlers.h"
#include "hashmap.h"
#include "io.h"
#include "json.h"
#include "query.h"
#include "str.h"

#include "requests.h"

typedef struct {
  int method_len;
  int path_len;
  int version_len;
} request_line;

#define HTTP_MAX_BODY 8192

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

  long buffered = reader->write_pos - reader->read_pos;
  if (buffered < 0)
    buffered = 0;
  if (buffered > body_len)
    buffered = body_len;

  if (body_len > HTTP_MAX_BODY) {
    char discard[256];
    long remaining = body_len - buffered;
    while (remaining > 0) {
      long chunk =
          remaining > (long)sizeof(discard) ? (long)sizeof(discard) : remaining;
      long n = read(cl->fd, discard, chunk);
      if (n <= 0)
        break;
      remaining -= n;
    }
    write_headers(cl->fd, PAYLOAD_TOO_LARGE);
    cl->want_read = 0;
    cl->want_write = 0;
    cl->want_close = 1;
    return 1;
  }

  if (cl->pool.offset + body_len + 1 >= cl->pool.capacity) {
    long need = cl->pool.offset + body_len + 1;
    if (string_pool_relloc(&cl->pool, need * 2) < 0)
      return -1;
  }

  char *body = &cl->pool.base[cl->pool.offset];

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

static void write_status_line(int client, enum request_status status) {
  switch (status) {
  case OK:
    writef(client, "HTTP/1.1 200 OK\r\n");
    break;
  case INTERNAL_ERROR:
    writef(client, "HTTP/1.1 500 Internal Server Error\r\n");
    break;
  case FORBIDDEN:
    writef(client, "HTTP/1.1 403 Forbidden\r\n");
    break;
  case NOT_FOUND:
    writef(client, "HTTP/1.1 404 Not Found\r\n");
    break;
  case METHOD_NOT_ALLOWED:
    writef(client, "HTTP/1.1 405 Method Not Allowed\r\n");
    break;
  case LENGTH_REQUIRED:
    writef(client, "HTTP/1.1 411 Length Required\r\n");
    break;
  case PAYLOAD_TOO_LARGE:
    writef(client, "HTTP/1.1 413 Payload Too Large\r\n");
    break;
  case UNSUPPORTED_MEDIA_TYPE:
    writef(client, "HTTP/1.1 415 Unsupported Media Type\r\n");
    break;
  default:
    writef(client, "HTTP/1.1 400 Bad Request\r\n");
  }
}

static void write_json_body(int client, enum request_status status,
                            const char *body) {
  int body_len = len(body);

  write_status_line(client, status);
  writef(client, "Content-Type: application/json\r\n");
  writef(client, "Content-Length: %ld\r\n", body_len);
  writef(client, "Connection: close\r\n\r\n");
  write(client, body, body_len);
}

int write_json_reply(int client, enum request_status status, const char *key,
                     int ok) {
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

  write_json_body(client, status, body.base);

  string_pool_destroy(&storage);
  string_pool_destroy(&body);
  return 0;
}

static void parse_header_line(hash_map *map, string_pool *pool, char *data) {
  int colon = index(data, ':');
  if (colon < 0)
    return; // linea sin ':' -- no es un header valido, se ignora

  int data_len = len(data);
  int value_start = colon + 1;
  if (value_start < data_len && data[value_start] == ' ')
    value_start++; // "Key: value" -> salta el espacio tras ':'
  int value_len = data_len - value_start;

  // string_pool_nalloc ya copia acotado por size; no hace falta un
  // segundo copiado manual (evita el bug de substr escribiendo sobre
  // key/value == NULL cuando el pool se agota).
  char *key = string_pool_nalloc(pool, data, colon);
  char *value = string_pool_nalloc(pool, data + value_start, value_len);

  if (!key || !value) {
    logf("Error en memoria: pool de headers agotado\n");
    return;
  }

  hashmap_put(map, key, value);
}

static void apply_connection_header(client *cl) {
  const char *conn = hashmap_get(&cl->headers, "Connection");

  if (conn == NULL)
    return;

  if (strcmp(conn, "keep-alive") == 0) {
    // cl->state = STATE_KEEP_ALIVE;
    cl->want_close = 1;
  }

  if (strcmp(conn, "close") == 0) {
    cl->want_close = 1;
    cl->state = 0;
  }

  cl->want_close = 1;
}

int read_incoming(client *cl) {
  line_reader *reader = &cl->reader;
  reader->fd = cl->fd;
  int pos = reader->read_pos;
  char *line;

  int n = 0;
  while ((n = readline_stream(reader, 1024)) > 0) {
    line = &reader->buffer[pos];
    if (!cl->request_line_seen) {
      char *key = string_pool_alloc(&cl->pool, "REQUEST\0");
      char *value = string_pool_alloc(&cl->pool, line);
      hashmap_put(&cl->headers, key, value);
      cl->request_line_seen = 1;
    } else {
      parse_header_line(&cl->headers, &cl->pool, line);
    }
    pos = reader->read_pos;
  }

  if (n == READ_AGAIN || (n == 0 && !cl->request_line_seen)) {
    cl->want_read = 1;
    cl->want_write = 0;
    return 0;
  }

  int body_status = read_body(cl, reader);
  if (body_status < 0)
    return -1;
  if (body_status > 0)
    return 0;

  apply_connection_header(cl);

  cl->want_read = 0;
  cl->want_write = 1;

  return 0;
}

static int parse_request_line(const char *request, request_line *line) {
  int space_index1 = index(request, ' ');
  if (space_index1 < 0)
    return -1;

  int space_index2 = index(request + space_index1 + 1, ' ');
  if (space_index2 < 0)
    return -1;

  line->method_len = space_index1;
  line->path_len = space_index2;
  line->version_len = len(request + space_index1 + space_index2 + 2);
  return 0;
}

static int valid_http_version(const char *version) {
  return strcmp(version, "HTTP/1.1") == 0 || strcmp(version, "HTTP/1.0") == 0;
}

void write_response(client *cl) {
  int client_fd = cl->fd;
  const char *request = hashmap_get(&cl->headers, "REQUEST");
  request_line line;

  if (request == 0 || parse_request_line(request, &line) < 0) {
    write_headers(client_fd, request ? UNKNOWN : INTERNAL_ERROR);
    cl->want_close = 1;
    return;
  }

  char method[line.method_len + 1];
  char file[line.path_len + 1];
  char version[line.version_len + 1];

  substr(request, method, 0, line.method_len);
  substr(request, file, line.method_len + 1, line.path_len);
  substr(request, version, line.method_len + line.path_len + 2,
         line.version_len);

  method[line.method_len] = '\0';
  file[line.path_len] = '\0';
  version[line.version_len] = '\0';

  if (!valid_http_version(version)) {
    write_headers(client_fd, UNKNOWN);
    cl->want_close = 1;
    return;
  }

  char *query = query_split(file);
  if (query)
    query_parse(&cl->query, &cl->pool, query);

  dispatch_request(cl, method, file);

  if (cl->response_state == RESPONSE_HEADERS ||
      cl->response_state == RESPONSE_FILE)
    return;

  apply_connection_header(cl);
  cl->want_write = 0;
}

void write_headers(int client, enum request_status status) {
  write_status_line(client, status);
  if (status != OK)
    write(client, "\r\n", 2);
}
