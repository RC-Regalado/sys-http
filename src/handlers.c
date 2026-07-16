#include "handlers.h"

#include "database.h"
#include "files.h"
#include "hashmap.h"
#include "io.h"
#include "json.h"
#include "requests.h"
#include "str.h"

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

static int has_body(hash_map *headers) {
  return parse_decimal(hashmap_get(headers, "Content-Length")) > 0;
}

static int content_type_starts_with(hash_map *headers, const char *expected) {
  const char *content_type = hashmap_get(headers, "Content-Type");
  return content_type && starts_with(content_type, expected);
}

static int has_parent_segment(const char *path) {
  for (int i = 0; path[i] != '\0'; i++) {
    if (path[i] != '.')
      continue;
    if (path[i + 1] != '.')
      continue;

    char before = i == 0 ? '/' : path[i - 1];
    char after = path[i + 2];
    if ((before == '/' || before == '\\') &&
        (after == '\0' || after == '/' || after == '\\'))
      return 1;
  }
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
        if (*p == '\\' && *(p + 1))
          p++;
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

static void set_route_path(client *cl, const char *file) {
  if (strcmp(file, "/") == 0)
    string_n_copy("/index.html", cl->path, CLIENT_MAX_PATH);
  else
    string_n_copy(file[0] == '/' ? file + 1 : file, cl->path, CLIENT_MAX_PATH);
}

static void handle_get(client *cl) {
  if (starts_with(cl->path, "database/namespace/")) {
    const char *namespace_name = cl->path + 19;
    if (*namespace_name == '\0') {
      write_headers(cl->fd, NOT_FOUND);
      cl->want_close = 1;
      return;
    }
    database_list(cl, namespace_name);
    cl->want_close = 1;
    return;
  }

  if (strcmp(cl->path, "database/notes") == 0) {
    database_list(cl, "notes");
    cl->want_close = 1;
    return;
  }

  if (starts_with(cl->path, "database/")) {
    const char *id = cl->path + 9;
    if (*id == '\0') {
      write_headers(cl->fd, NOT_FOUND);
      cl->want_close = 1;
      return;
    }
    database_route(cl, id);
    cl->want_close = 1;
    return;
  }

  serve_static_file(cl);
}

static void handle_post(int client, hash_map *headers, const char *path) {
  if (strcmp(path, "database/notes") == 0) {
    const char *body = hashmap_get(headers, "BODY");
    string_pool key;

    if (!content_type_starts_with(headers, "text/markdown")) {
      write_json_reply(client, UNSUPPORTED_MEDIA_TYPE, 0, 0);
      return;
    }

    if (!body || *body == '\0' || string_pool_init(&key, 64) < 0) {
      write_json_reply(client, UNKNOWN, 0, 0);
      return;
    }

    int saved = database_add_note(body, len(body), &key);
    if (saved == 0)
      write_json_reply(client, OK, key.base, 1);
    else
      write_json_reply(client, INTERNAL_ERROR, 0, 0);

    string_pool_destroy(&key);
    return;
  }

  if (strcmp(path, "database") == 0) {
    const char *body = hashmap_get(headers, "BODY");
    char key[128];

    if (!content_type_starts_with(headers, "application/json")) {
      write_json_reply(client, UNSUPPORTED_MEDIA_TYPE, 0, 0);
      return;
    }

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
  if (accept)
    json_add_string(&obj, "accept", accept);
  else
    json_add_null(&obj, "accept");

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

void dispatch_request(client *cl, const char *method, const char *file) {
  int fd = cl->fd;

  logf("request type(%s) to %s\n", method, file);

  if (has_parent_segment(file)) {
    write_headers(fd, FORBIDDEN);
    cl->want_close = 1;
    return;
  }

  set_route_path(cl, file);

  if (strcmp(method, "GET") == 0) {
    handle_get(cl);
  } else if (strcmp(method, "POST") == 0) {
    if (!has_body(&cl->headers)) {
      write_headers(fd, LENGTH_REQUIRED);
      cl->want_close = 1;
      return;
    }
    handle_post(fd, &cl->headers, cl->path);
    cl->want_close = 1;
  } else {
    write_headers(fd, METHOD_NOT_ALLOWED);
    cl->want_close = 1;
  }
}
