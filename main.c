#define SYS_SOCKET 41
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_ACCEPT 43
#define SYS_WRITE 1

#define AF_INET 2
#define SOCK_STREAM 1

__attribute__((naked)) void _start() {
  asm volatile(
      // Llamamos a nuestra función real en C
      "call server\n"

      // Terminamos el proceso con syscall exit(0)
      "mov $60, %rax\n"  // syscall: exit
      "xor %rdi, %rdi\n" // status = 0
      "syscall\n");
}

struct sockaddr_in {
  unsigned short sin_family;
  unsigned short sin_port;
  unsigned int sin_addr;
  char zero[8];
};

long syscall3(long n, long a1, long a2, long a3) {
  long ret;
  asm volatile("mov %1, %%rax\n"
               "mov %2, %%rdi\n"
               "mov %3, %%rsi\n"
               "mov %4, %%rdx\n"
               "syscall\n"
               "mov %%rax, %0\n"
               : "=r"(ret)
               : "r"(n), "r"(a1), "r"(a2), "r"(a3)
               : "rax", "rdi", "rsi", "rdx");
  return ret;
}

void server() {
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = 0x901f; // htons(8080) → 0x1f90 → 0x901f en little-endian
  addr.sin_addr = 0;      // INADDR_ANY
  for (int i = 0; i < 8; i++)
    addr.zero[i] = 0;

  int sockfd = syscall3(SYS_SOCKET, AF_INET, SOCK_STREAM, 0);
  syscall3(SYS_BIND, sockfd, (long)&addr, sizeof(addr));
  syscall3(SYS_LISTEN, sockfd, 5, 0);

  while (1) {
    int client = syscall3(SYS_ACCEPT, sockfd, 0, 0);

    const char *msg =
        "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!";
    syscall3(SYS_WRITE, client, (long)msg, 49);
  }
}
