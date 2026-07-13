#include "syscalls.h"

extern long syscall3(long syscall, long rdi, long rsi, long rdx);

long sys_call0(long number) { return syscall3(number, 0, 0, 0); }

long sys_call1(long number, long arg1) { return syscall3(number, arg1, 0, 0); }

long sys_call2(long number, long arg1, long arg2) {
  return syscall3(number, arg1, arg2, 0);
}

long sys_call3(long number, long arg1, long arg2, long arg3) {
  return syscall3(number, arg1, arg2, arg3);
}

long sys_call4(long number, long arg1, long arg2, long arg3, long arg4) {
  long ret;
  asm volatile("mov %1, %%rax\n"
               "mov %2, %%rdi\n"
               "mov %3, %%rsi\n"
               "mov %4, %%rdx\n"
               "mov %5, %%r10\n"
               "syscall\n"
               "mov %%rax, %0\n"
               : "=r"(ret)
               : "r"(number), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)
               : "rax", "rdi", "rsi", "rdx", "r10", "rcx", "r11", "memory");
  return ret;
}

long sys_call5(long number, long arg1, long arg2, long arg3, long arg4,
               long arg5) {
  long ret;
  asm volatile("mov %1, %%rax\n"
               "mov %2, %%rdi\n"
               "mov %3, %%rsi\n"
               "mov %4, %%rdx\n"
               "mov %5, %%r10\n"
               "mov %6, %%r8\n"
               "syscall\n"
               "mov %%rax, %0\n"
               : "=r"(ret)
               : "r"(number), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4),
                 "r"(arg5)
               : "rax", "rdi", "rsi", "rdx", "r10", "r8", "rcx", "r11",
                 "memory");
  return ret;
}

long sys_read(int fd, void *buffer, unsigned long len) {
  return sys_call3(SYS_READ, fd, (long)buffer, len);
}

long sys_write(int fd, const void *buffer, unsigned long len) {
  return sys_call3(SYS_WRITE, fd, (long)buffer, len);
}

int sys_open(const char *filename, int flags, int mode) {
  return (int)sys_call3(SYS_OPEN, (long)filename, flags, mode);
}

int sys_close(int fd) { return (int)sys_call1(SYS_CLOSE, fd); }

int sys_fstat(int fd, struct stat *sb) {
  return (int)sys_call3(SYS_STAT, fd, (long)sb, 0);
}

long sys_lseek(int fd, long offset, int whence) {
  return sys_call3(SYS_LSEEK, fd, offset, whence);
}

long sys_pread64(int fd, void *buffer, unsigned long len, long offset) {
  return sys_call4(SYS_PREAD64, fd, (long)buffer, len, offset);
}

long sys_pwrite64(int fd, const void *buffer, unsigned long len, long offset) {
  return sys_call4(SYS_PWRITE64, fd, (long)buffer, len, offset);
}

long sys_sendfile(int out_fd, int in_fd, void *off, long count) {
  return sys_call4(SYS_SEND_FILE, out_fd, in_fd, (long)off, count);
}

int sys_socket(int domain, int type, int protocol) {
  return (int)sys_call3(SYS_SOCKET, domain, type, protocol);
}

int sys_bind(int sockfd, const void *addr, unsigned int len) {
  return (int)sys_call3(SYS_BIND, sockfd, (long)addr, len);
}

int sys_listen(int sockfd, int backlog) {
  return (int)sys_call2(SYS_LISTEN, sockfd, backlog);
}

int sys_accept(int sockfd, void *addr, void *addr_len) {
  return (int)sys_call3(SYS_ACCEPT, sockfd, (long)addr, (long)addr_len);
}

int sys_setsockopt(int sockfd, int level, int optname, const void *optval,
                   unsigned int optlen) {
  return (int)sys_call5(SYS_SETSOCKOPT, sockfd, level, optname, (long)optval,
                        optlen);
}

int sys_getsockopt(int sockfd, int level, int optname, void *optval,
                   unsigned int *optlen) {
  return (int)sys_call5(SYS_GETSOCKOPT, sockfd, level, optname, (long)optval,
                        (long)optlen);
}

int sys_fcntl(int fd, int cmd, long arg) {
  return (int)sys_call3(SYS_FCNTL, fd, cmd, arg);
}

int sys_epoll_create1(int flags) {
  return (int)sys_call1(SYS_EPOLL_CREATE1, flags);
}

int sys_epoll_ctl(int epfd, int op, int fd, void *event_ptr) {
  return (int)sys_call4(SYS_EPOLL_CTL, epfd, op, fd, (long)event_ptr);
}

int sys_epoll_wait(int epfd, void *events, int max_events, int timeout_ms) {
  return (int)sys_call4(SYS_EPOLL_WAIT, epfd, (long)events, max_events,
                        timeout_ms);
}

int sys_munmap(void *addr, unsigned long len) {
  return (int)sys_call2(SYS_MUNMAP, (long)addr, len);
}
