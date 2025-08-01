#include "inc/io.h"
#include "inc/str.h"

extern long syscall3(long syscall, long rdi, long rsi, long rdx);

void writeout(long where, const char *str) {
  syscall3(SYS_WRITE, where, (long)str, len(str));
}

void writelog(const char *str) { writeout(STDOUT, str); }

unsigned int read(long instream, char *buffer, unsigned short lenght) {
  syscall3(SYS_READ, instream, (long)buffer, lenght);

  return len(buffer);
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
