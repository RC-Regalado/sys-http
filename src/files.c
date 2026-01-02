#include "files.h"
#include "io.h"

// Para llamada a fcntl
extern long syscall3(long syscall, long rdi, long rsi, long rdx);

void fd_set_nonblock(int fd) {
  long flags = syscall3(SYS_FCNTL, (long)fd, F_GETFL, 0);

  logf("[%s] :: Flags result %ld\n", __FILE_NAME__, flags);
  if (flags < 0)
    return;

  flags |= O_NONBLOCK;

  flags = syscall3(SYS_FCNTL, fd, F_SETFL, flags);

  if (flags) {
    logf("Syscall fcntl error, result %l", flags);
  }
}
