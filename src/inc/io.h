#ifndef __IO_H
#define __IO_H

// SYSCALL
#define SYS_WRITE 1
#define SYS_READ 0
#define SYS_CLOSE 3

// OUTPUTS
#define STDOUT 1

// ERRORS
#define INERR -1

// SIZES
#define LINE_BUF_SIZE 1024

typedef struct {
  int fd;                     // File descriptor (socket o archivo)
  char buffer[LINE_BUF_SIZE]; // Buffer interno
  int read_pos;               // Inicio de lectura
  int write_pos;              // Fin de datos válidos
} line_reader;

void writeout(long where, const char *msg);
void writelog(const char *msg);

int readline_stream(line_reader *reader, unsigned short chunk_len);
unsigned int read(long instream, char *buffer, unsigned short len);
#endif // IO_H_
