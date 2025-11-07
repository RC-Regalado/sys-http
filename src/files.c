#include "inc/files.h"
#include "inc/io.h"
#include <errno.h>

// Para llamada a fcntl
extern long syscall3(long syscall, long rdi, long rsi, long rdx);

void fd_set_nonblock(int fd) {
  errno = 0;
  long flags = syscall3(SYS_FCNTL, (long)fd, F_GETFL, 0);

  if (errno) {
    return;
  }

  flags |= O_NONBLOCK;

  errno = 0;
  syscall3(SYS_FCNTL, fd, F_SETFL, flags);

  if (errno) {
    logf("Syscall fcntl error, result %l", errno);
  }
}
