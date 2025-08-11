#ifndef __IO_H
#define __IO_H
#include <sys/stat.h>

// SYSCALL
#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_STAT 5

// OUTPUTS
#define STDOUT 1

// ERRORS
#define INERR -1

// SIZES
#define LINE_BUF_SIZE 1024
#define LOG_BUF 1024

// FLAGS
#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR 02

typedef struct {
  int fd;                     // File descriptor (socket o archivo)
  char buffer[LINE_BUF_SIZE]; // Buffer interno
  int read_pos;               // Inicio de lectura
  int write_pos;              // Fin de datos válidos
} line_reader;

void write(long where, const char *data, int size);
void writef(long where, const char *fmt, ...);
void logf(const char *fmt, ...);

int readline_stream(line_reader *reader, unsigned short chunk_len);
unsigned int read(long instream, char *buffer, unsigned short len);

int open(const char *filename, int flags);
void close(int fd);

int stat_file(int fd, struct stat *sb);
#endif // IO_H_
