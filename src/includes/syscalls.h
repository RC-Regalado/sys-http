#ifndef SYSCALLS_H_
#define SYSCALLS_H_

#include <sys/stat.h>

// Linux x86_64 syscall numbers
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_STAT 5
#define SYS_LSEEK 8
#define SYS_MUNMAP 11
#define SYS_PREAD64 17
#define SYS_PWRITE64 18
#define SYS_SOCKET 41
#define SYS_ACCEPT 43
#define SYS_SEND_FILE 40
#define SYS_BIND 49
#define SYS_LISTEN 50
#define SYS_SETSOCKOPT 54
#define SYS_GETSOCKOPT 55
#define SYS_FCNTL 72
#define SYS_EPOLL_WAIT 232
#define SYS_EPOLL_CTL 233
#define SYS_EPOLL_CREATE1 291

// Socket constants
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_ERROR 4

long sys_call0(long number);
long sys_call1(long number, long arg1);
long sys_call2(long number, long arg1, long arg2);
long sys_call3(long number, long arg1, long arg2, long arg3);
long sys_call4(long number, long arg1, long arg2, long arg3, long arg4);
long sys_call5(long number, long arg1, long arg2, long arg3, long arg4,
               long arg5);

long sys_read(int fd, void *buffer, unsigned long len);
long sys_write(int fd, const void *buffer, unsigned long len);
int sys_open(const char *filename, int flags, int mode);
int sys_close(int fd);
int sys_fstat(int fd, struct stat *sb);
long sys_lseek(int fd, long offset, int whence);
long sys_pread64(int fd, void *buffer, unsigned long len, long offset);
long sys_pwrite64(int fd, const void *buffer, unsigned long len, long offset);
long sys_sendfile(int out_fd, int in_fd, void *off, long count);
int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const void *addr, unsigned int len);
int sys_listen(int sockfd, int backlog);
int sys_accept(int sockfd, void *addr, void *addr_len);
int sys_setsockopt(int sockfd, int level, int optname, const void *optval,
                   unsigned int optlen);
int sys_getsockopt(int sockfd, int level, int optname, void *optval,
                   unsigned int *optlen);
int sys_fcntl(int fd, int cmd, long arg);
int sys_epoll_create1(int flags);
int sys_epoll_ctl(int epfd, int op, int fd, void *event_ptr);
int sys_epoll_wait(int epfd, void *events, int max_events, int timeout_ms);
int sys_munmap(void *addr, unsigned long len);

#endif // SYSCALLS_H_
