#include "inc/hashmap.h"
#include "inc/io.h"
#include "inc/str.h"

#define SYS_SOCKET 41
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_ACCEPT 43

#define AF_INET 2
#define SOCK_STREAM 1

extern void _start();
extern long syscall3(long syscall, long rdi, long rsi, long rdx);

struct sockaddr_in {
  unsigned short sin_family;
  unsigned short sin_port;
  unsigned int sin_addr;
  char zero[8];
};

// void log(const char *str) { syscall3(SYS_WRITE, STDOUT, (long)str, len(str));
// }

unsigned short htons(unsigned short x) {
  asm("xchg %h0, %b0" : "+Q"(x)); // intercambia los bytes del registro
  return x;
}

void server() {
  struct sockaddr_in addr = {0}; // inicializa todo a 0

  addr.sin_family = AF_INET;
  addr.sin_port = htons(5050); // htons(8080)
  addr.sin_addr = 0;           // INADDR_ANY

  int sockfd = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);
  syscall3(SYS_BIND, sockfd, (long)&addr, sizeof(addr));
  syscall3(SYS_LISTEN, sockfd, 5, 0);

  const char *data = "Iniciando el servicio\n";
  writelog(data);

  line_reader reader = {.read_pos = 0, .write_pos = 0};
  hash_map map;

  while (1) {
    int client = syscall3(SYS_ACCEPT, sockfd, 0, 0);

    writelog("Cliente conectado\n");
    const char *msg =
        "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!";

    writelog("El cliente dice: ");
    reader.fd = client;
    int size = 0;
    int pos = 0;
    while ((size = readline_stream(&reader, 1024)) > 0) {
      char *line = &reader.buffer[pos];
      writelog(line);
      writelog("\n");
      pos = reader.read_pos;
    }

    writelog("El servicio envía: ");
    writelog(msg);
    syscall3(SYS_WRITE, client, (long)msg, len(msg));
    syscall3(SYS_CLOSE, client, 0, 0);

    writelog("\n");
  }
}
