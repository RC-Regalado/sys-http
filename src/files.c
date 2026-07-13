#include "files.h"
#include "io.h"
#include "syscalls.h"

void fd_set_nonblock(int fd) {
  long flags = sys_fcntl(fd, F_GETFL, 0);

  logf("[%s] :: Flags result %ld\n", __FILE_NAME__, flags);
  if (flags < 0)
    return;

  flags |= O_NONBLOCK;

  flags = sys_fcntl(fd, F_SETFL, flags);

  if (flags) {
    logf("Syscall fcntl error, result %l", flags);
  }
}
