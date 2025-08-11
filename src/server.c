#include <sys/stat.h>

#include "inc/hashmap.h"
#include "inc/io.h"
#include "inc/memory.h"
#include "inc/str.h"

#define SYS_SOCKET 41
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_ACCEPT 43

#define AF_INET 2
#define SOCK_STREAM 1

extern void _start();
extern long syscall3(long syscall, long rdi, long rsi, long rdx);
extern long setsockopt(long fd, long optval, long optlen);

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

void parse_headers(hash_map *map, string_pool *pool, char *data) {
  int colon = index(data, ':');

  int data_len = len(data);
  int value_len = data_len - colon - 1;

  char *key = string_pool_nalloc(pool, data, colon);
  char *value = string_pool_nalloc(pool, data + colon + 1, value_len);

  if ((long)key < NULL)
    logf("Error en memoria");

  substr(data, key, 0, colon);
  substr(data, value, colon + 1, value_len);

  hashmap_put(map, key, value);
}

int read_incoming(int fd, string_pool *pool, hash_map *map) {
  line_reader reader = {.read_pos = 0, .write_pos = 0};
  reader.fd = fd;
  int pos = 0;
  char *line;

  while (readline_stream(&reader, 1024) > 0) {
    line = &reader.buffer[pos];
    if (pos == 0) {
      char *key = string_pool_alloc(pool, "REQUEST\0");
      char *value = string_pool_alloc(pool, line);
      hashmap_put(map, key, value);
    } else {
      parse_headers(map, pool, line);
    }
    pos = reader.read_pos;
  }

  return 0;
}

void get_response() {}

void write_response(int client, hash_map *map) {
  const char *header = "HTTP/1.1 200 OK\r\n";
  string_pool handler;
  string_pool_init(&handler, 1024);

  char *route = string_pool_alloc(&handler, "templates");
  const char *request = hashmap_get(map, "REQUEST");

  struct stat sb;

  if (request == 0) {
    writef(client, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
    return;
  }

  int space_index1 = index(request, ' ');
  int space_index2 = index(request + space_index1 + 1, ' ');

  if (space_index1 < 0 || space_index2 < 0) {
    writef(client, "HTTP/1.1 400 Bad Request\r\n\r\n");
    return;
  }
  char type[space_index1];
  char path[space_index2 + 1];

  int bytes_readed = 0;
  char buffer[256];

  substr(request, type, 0, space_index1);
  substr(request, path, space_index1 + 1, space_index2);

  type[space_index1] = '\0';
  path[space_index2] = '\0';

  logf("request type(%s) to %s\n", type, path);

  if (strcmp(path, "/") == 0)
    string_pool_append(&handler, "index.html", 0);
  else
    string_pool_append(&handler, path, 0);

  int fd = open(route, O_RDONLY);

  if (fd < 0) {
    writef(client, "HTTP/1.1 404 Not Found\r\n\r\n");
    return;
  }

  if (stat_file(fd, &sb) == -1) {
    logf("Ha ocurrido un error al realizar stat en el archivo: %s\n", route);
    writef(client, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
    return;
  }

  // Cabecera por cortes
  writef(client, header);

  int ext_index = last_index_of(route, '.');
  char *filetype;

  unsigned int l = len(route);
  int top = l - ext_index - 1;

  substr(route, path, ext_index + 1, top);
  path[top] = '\0';

  if (strcmp(path, "html") == 0) {
    filetype = string_pool_alloc(&handler, "text/html");
  } else if (strcmp(path, "css") == 0) {
    filetype = string_pool_alloc(&handler, "text/css");
  }

  writef(client, "Content-Type: %s\r\n", filetype);
  writef(client, "Content-Length: %d \r\n\r\n", sb.st_size);

  string_pool_reset(&handler);

  while ((bytes_readed = read(fd, buffer, 256)) > 0) {
    write(client, buffer, bytes_readed);
    if (bytes_readed < 256)
      break;
  }

  string_pool_destroy(&handler);
  close(fd);
}

void server() {

  struct sockaddr_in addr = {0}; // inicializa todo a 0
  string_pool builder;
  hash_map map = {.pool_index = 0};
  string_pool_init(&builder, 1024);

  int port = 5050;
  int enable = 1;

  logf("Iniciando el servicio en el puerto %d \n", port);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = 0; // INADDR_ANY

  int sockfd = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    logf("Ha ocurrido un error al iniciar el socket.\n");
    return;
  }

  setsockopt(sockfd, (long)&enable, sizeof(int));

  if (syscall3(SYS_BIND, sockfd, (long)&addr, sizeof(addr)) < 0) {
    logf("El puerto ya está en uso!\n");
    return;
  }
  syscall3(SYS_LISTEN, sockfd, 5, 0);

  while (1) {
    int client = syscall3(SYS_ACCEPT, sockfd, 0, 0);

    logf("Cliente conectado\n");

    read_incoming(client, &builder, &map);
    write_response(client, &map);

    for (int i = 0; i < map.pool_index; i++) {
      const char *key = map.pool[i].key;
      logf("%s = %s \n", key, hashmap_get(&map, key));
    }

    close(client);

    string_pool_reset(&builder);
    logf("\n");
  }
}
