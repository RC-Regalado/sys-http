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
    writelog("Error en memoria");

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

void write_response(int client, hash_map *map) {
  const char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html;\r\n";
  string_pool handler;
  string_pool_init(&handler, 1024);

  // Usamos el pool para datos de inicialización
  char *msg = string_pool_alloc(&handler, "templates/");
  const char *request = hashmap_get(map, "REQUEST");

  int space_index1 = index(request, ' ');
  int space_index2 = index(request + space_index1 + 1, ' ');

  char type[space_index1];
  char path[space_index2 + 1];

  int bytes_reader = 0;
  char buffer[256];
  int content_lenght;

  for (long i = 0; i < space_index1; i++) {
    type[i] = request[i];
  }

  writelog(type);

  for (long i = 0; i < space_index2; i++) {
    path[i] = request[space_index1 + i + 1];
  }

  path[space_index2] = '\0';

  if (strcmp(path, "/") == 0)
    string_pool_append(&handler, "index.html", 0);
  else
    string_pool_append(&handler, path, 0);

  int fd = open(msg, O_RDONLY);

  string_pool_reset(&handler);

  while ((bytes_reader = read(fd, buffer, 256)) > 0) {
    msg = string_pool_alloc(&handler, buffer);

    if (bytes_reader < 256 || buffer[bytes_reader] == 0)
      break;
  }

  writelog("El servicio envía: ");
  writelog(msg);

  content_lenght = nlen(handler.offset - 1);
  tostr(buffer, handler.offset - 1, content_lenght);

  // Cabecera por cortes
  writeout(client, header);
  writeout(client, "Content-Length:");
  writeout(client, buffer);
  writeout(client, "\r\n\r\n");

  writeout(client, msg);

  syscall3(SYS_CLOSE, fd, 0, 0);
}

void server() {

  struct sockaddr_in addr = {0}; // inicializa todo a 0
  string_pool builder;
  hash_map map = {.pool_index = 0};
  string_pool_init(&builder, 1024);

  int port = 5050;
  int enable = 1;

  int size = nlen(port);
  char buff[size + 1];

  tostr(buff, port, size);

  writelog("Iniciando el servicio en el puerto ");
  writelog(buff);
  writelog("\n");

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = 0; // INADDR_ANY

  int sockfd = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    writelog("Ha ocurrido un error al iniciar el socket.\n");
    return;
  }

  setsockopt(sockfd, (long)&enable, sizeof(int));

  if (syscall3(SYS_BIND, sockfd, (long)&addr, sizeof(addr)) < 0) {
    writelog("El puerto ya está en uso!\n");
    return;
  }
  syscall3(SYS_LISTEN, sockfd, 5, 0);

  while (1) {
    int client = syscall3(SYS_ACCEPT, sockfd, 0, 0);

    writelog("Cliente conectado\n");

    read_incoming(client, &builder, &map);
    write_response(client, &map);

    for (int i = 0; i < map.pool_index; i++) {
      const char *key = map.pool[i].key;
      writelog(hashmap_get(&map, key));
      writelog("\n");
    }

    syscall3(SYS_CLOSE, client, 0, 0);

    string_pool_reset(&builder);
    writelog("\n");
  }
}
