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
extern void *sysmap(long size);

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
  writelog(hashmap_get(map, "REQUEST"));

  const char *header = "HTTP/1.1 200 OK\r\nContent-Length: "
                       "146\r\nContent-Type: text/html; \r\n\r\n";

  char msg[146];

  int fd = open("templates/index.html", O_RDONLY);

  read(fd, msg, 146);

  writelog("El servicio envía: ");
  writelog(msg);

  writeout(client, header);
  writeout(client, msg);
}

void server() {
  struct sockaddr_in addr = {0}; // inicializa todo a 0
  string_pool builder;
  hash_map map = {.pool_index = 0};
  string_pool_init(&builder, 1024);

  int port = 5050;
  writelog("Iniciando el servicio\n");

  string_pool_alloc(&builder, "Iniciando servicio");
  string_pool_relloc(&builder, 2048);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = 0; // INADDR_ANY

  int sockfd = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);
  syscall3(SYS_BIND, sockfd, (long)&addr, sizeof(addr));
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
