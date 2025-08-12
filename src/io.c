#include <stdarg.h>
#include <sys/stat.h>

#include "inc/io.h"
#include "inc/str.h"

extern long syscall3(long syscall, long rdi, long rsi, long rdx);

void format(long where, const char *fmt, va_list ap) {
  char out[LOG_BUF];
  int w = 0;

  // va_list ap;
  // va_start(ap, fmt);

  for (const char *p = fmt; *p && w < LOG_BUF - 1; ++p) {
    if (*p != '%') {
      out[w++] = *p;
      continue;
    }

    // tenemos un %
    ++p;
    if (!*p)
      break;

    if (*p == 's') {
      const char *s = va_arg(ap, const char *);
      if (!s)
        s = "(null)";
      while (*s && w < LOG_BUF - 1)
        out[w++] = *s++;
    } else if (*p == 'l' && *(p + 1) != 0 && *(p + 1) == 'd') {
      ++p;
      long v = va_arg(ap, long);
      unsigned int L = nlen(v);
      if (w + (int)L >= LOG_BUF)
        L = LOG_BUF - 1 - w;
      char tmp[32];
      unsigned int need =
          (L + 1 <= sizeof(tmp)) ? (L + 1) : (unsigned int)sizeof(tmp);
      tostr(tmp, v, need);
      for (unsigned int i = 0; tmp[i] && w < LOG_BUF - 1; ++i)
        out[w++] = tmp[i];
    } else if (*p == 'd') {
      long v = va_arg(ap, int);
      unsigned int L = nlen(v);
      if (w + (int)L >= LOG_BUF)
        L = LOG_BUF - 1 - w;
      char tmp[32];
      unsigned int need =
          (L + 1 <= sizeof(tmp)) ? (L + 1) : (unsigned int)sizeof(tmp);
      tostr(tmp, v, need);
      for (unsigned int i = 0; tmp[i] && w < LOG_BUF - 1; ++i)
        out[w++] = tmp[i];
    } else if (*p == '%') {
      out[w++] = '%';
    } else {
      out[w++] = '%';
      if (w < LOG_BUF - 1)
        out[w++] = *p;
    }
  }
  out[w] = '\0';

  syscall3(SYS_WRITE, where, (long)out, w);
}
long write(long where, const char *data, int size) {
  return syscall3(SYS_WRITE, where, (long)data, size);
}

void writef(long where, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  format(where, fmt, ap);

  va_end(ap);
}

void logf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  format(STDOUT, fmt, ap);

  va_end(ap);
}

int readline_stream(line_reader *reader, unsigned short chunk_len) {
  while (1) {
    // Buscar delimitador dentro del buffer actual
    for (int i = reader->read_pos; i < reader->write_pos; i++) {
      if (reader->buffer[i] == '\n') {
        int line_end = i;

        // Manejar \r\n
        if (i > 0 && reader->buffer[i - 1] == '\r') {
          reader->buffer[i - 1] = '\0'; // Termina línea sin \r
          line_end = i - 1;
        } else {
          reader->buffer[i] = '\0'; // Termina línea sin \n
        }

        int start = reader->read_pos;
        reader->read_pos = i + 1;
        return line_end - start;
      }
    }

    // Compactar buffer si hay espacio desperdiciado
    if (reader->read_pos > 0) {
      int remaining = reader->write_pos - reader->read_pos;
      for (int j = 0; j < remaining; j++) {
        reader->buffer[j] = reader->buffer[reader->read_pos + j];
      }
      reader->read_pos = 0;
      reader->write_pos = remaining;
    }

    // Verificar si hay espacio para leer más
    if (reader->write_pos >= LINE_BUF_SIZE) {
      return -2; // buffer lleno sin encontrar \n
    }

    // Leer más datos desde el fd
    long bytes_read =
        read(reader->fd, &reader->buffer[reader->write_pos], chunk_len);

    if (bytes_read == 0) {
      return 0; // EOF limpio
    } else if (bytes_read < 0) {
      return -1; // error de lectura
    }

    reader->write_pos += bytes_read;
  }
}

unsigned int read(long instream, char *buffer, unsigned short lenght) {
  return syscall3(SYS_READ, instream, (long)buffer, lenght);
}

int open(const char *filename, int flags) {
  return syscall3(SYS_OPEN, (long)filename, flags, 0);
}

void close(int fd) { syscall3(SYS_CLOSE, fd, 0, 0); }

int stat_file(int fd, struct stat *sb) {
  return syscall3(SYS_STAT, fd, (long)sb, 0);
}
