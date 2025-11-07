#ifndef FILES_H_
#define FILES_H_

// FCNTL callno
#define SYS_FCNTL 72

// File descriptor flags
#define F_GETFL 3
#define F_SETFL 4

struct pollfd {
  int fd;
  short int events;
  short int revents;
};

void fd_set_nonblock(int fd);

#endif // !FILES_H_
