#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_nl(char *s) {
  size_t n = strlen(s);
  if (n && s[n - 1] == '\n')
    s[n - 1] = 0;
}

static char *next_token(char **cursor) {
  char *start;
  char *end;

  while (**cursor == ' ')
    (*cursor)++;
  if (**cursor == '\0')
    return NULL;

  start = *cursor;
  end = start;
  while (*end && *end != ' ')
    end++;
  if (*end) {
    *end = '\0';
    *cursor = end + 1;
  } else {
    *cursor = end;
  }
  return start;
}

static char *rest_token(char **cursor) {
  while (**cursor == ' ')
    (*cursor)++;
  return **cursor ? *cursor : NULL;
}

static int count_record(void *ctx, const db_record_view_t *record) {
  (void)record;
  *(long *)ctx += 1;
  return 0;
}

static int print_record(void *ctx, const db_record_view_t *record) {
  FILE *out = (FILE *)ctx;
  fprintf(out, "key=%.*s ns=%.*s type=%.*s flags=%u len=%zu data=",
          (int)record->klen, (const char *)record->key,
          (int)record->namespace_len, record->namespace_name,
          (int)record->content_type_len, record->content_type, record->flags,
          record->dlen);
  fwrite(record->data, 1, record->dlen, out);
  fputc('\n', out);
  return 0;
}

int main(int argc, char **argv) {
  int sync_each = 0;
  db_t db;
  char line[8192];

  if (argc < 2) {
    fprintf(stderr, "uso: %s <wal_path> [--sync]\n", argv[0]);
    return 1;
  }
  if (argc >= 3 && strcmp(argv[2], "--sync") == 0)
    sync_each = 1;

  if (db_open(&db, argv[1], sync_each) != 0) {
    perror("db_open");
    return 1;
  }

  fprintf(stdout,
          "microdb listo. comandos: WRITE/WRITEBLOB/READ/DELETE/FINDNS/"
          "FINDTYPE/COUNT/SET/GET/DEL/COMPACT/EXIT\n");

  while (fgets(line, sizeof line, stdin)) {
    char *cursor;
    char *cmd;
    trim_nl(line);
    if (line[0] == '\0')
      continue;

    cursor = line;
    cmd = next_token(&cursor);
    if (!cmd)
      continue;

    if (strcmp(cmd, "EXIT") == 0 || strcmp(cmd, "QUIT") == 0) {
      break;
    } else if (strcmp(cmd, "SET") == 0) {
      char *key = next_token(&cursor);
      char *value = rest_token(&cursor);
      if (!key || !value) {
        fprintf(stdout, "uso: SET <key> <value>\n");
        continue;
      }
      fprintf(stdout, "%s\n",
              db_set(&db, key, strlen(key), value, strlen(value)) == 0 ? "OK"
                                                                        : "ERR");
    } else if (strcmp(cmd, "GET") == 0) {
      char *key = next_token(&cursor);
      void *out = NULL;
      size_t out_len = 0;
      if (!key) {
        fprintf(stdout, "uso: GET <key>\n");
        continue;
      }
      if (db_get(&db, key, strlen(key), &out, &out_len) != 0) {
        fprintf(stdout, "(nil)\n");
      } else {
        fwrite(out, 1, out_len, stdout);
        fputc('\n', stdout);
        free(out);
      }
    } else if (strcmp(cmd, "DEL") == 0) {
      char *key = next_token(&cursor);
      if (!key) {
        fprintf(stdout, "uso: DEL <key>\n");
        continue;
      }
      fprintf(stdout, "%s\n",
              db_del(&db, key, strlen(key)) == 0 ? "OK" : "ERR");
    } else if (strcmp(cmd, "WRITE") == 0 || strcmp(cmd, "WRITEBLOB") == 0) {
      char *key = next_token(&cursor);
      char *namespace_name = next_token(&cursor);
      char *content_type = next_token(&cursor);
      char *value = rest_token(&cursor);
      db_record_meta_t meta;
      if (!key || !namespace_name || !content_type || !value) {
        fprintf(stdout, "uso: %s <key> <namespace> <content_type> <value>\n",
                cmd);
        continue;
      }
      meta.namespace_name = namespace_name;
      meta.content_type = content_type;
      meta.flags = strcmp(cmd, "WRITEBLOB") == 0 ? DB_RECORD_BLOB : 0;
      if (strstr(content_type, "json"))
        meta.flags |= DB_RECORD_JSON;
      if (strstr(content_type, "protobuf"))
        meta.flags |= DB_RECORD_PROTOBUF;
      if (strstr(content_type, "octet-stream"))
        meta.flags |= DB_RECORD_FILE;
      fprintf(stdout, "%s\n",
              db_write(&db, key, strlen(key), &meta, value, strlen(value), NULL,
                       NULL) == 0
                  ? "OK"
                  : "ERR");
    } else if (strcmp(cmd, "READ") == 0) {
      char *key = next_token(&cursor);
      db_selector_t selector;
      if (!key) {
        fprintf(stdout, "uso: READ <key>\n");
        continue;
      }
      memset(&selector, 0, sizeof selector);
      selector.key = key;
      selector.klen = strlen(key);
      if (db_read(&db, &selector, print_record, stdout) != 0)
        fprintf(stdout, "ERR\n");
    } else if (strcmp(cmd, "DELETE") == 0) {
      char *key = next_token(&cursor);
      db_selector_t selector;
      if (!key) {
        fprintf(stdout, "uso: DELETE <key>\n");
        continue;
      }
      memset(&selector, 0, sizeof selector);
      selector.key = key;
      selector.klen = strlen(key);
      fprintf(stdout, "%s\n",
              db_delete_where(&db, &selector, print_record, stdout) == 0 ? "OK"
                                                                         : "ERR");
    } else if (strcmp(cmd, "FINDNS") == 0) {
      char *namespace_name = next_token(&cursor);
      db_selector_t selector;
      if (!namespace_name) {
        fprintf(stdout, "uso: FINDNS <namespace>\n");
        continue;
      }
      memset(&selector, 0, sizeof selector);
      selector.namespace_name = namespace_name;
      if (db_read(&db, &selector, print_record, stdout) != 0)
        fprintf(stdout, "ERR\n");
    } else if (strcmp(cmd, "FINDTYPE") == 0) {
      char *content_type = next_token(&cursor);
      db_selector_t selector;
      if (!content_type) {
        fprintf(stdout, "uso: FINDTYPE <content_type>\n");
        continue;
      }
      memset(&selector, 0, sizeof selector);
      selector.content_type = content_type;
      if (db_read(&db, &selector, print_record, stdout) != 0)
        fprintf(stdout, "ERR\n");
    } else if (strcmp(cmd, "COUNT") == 0) {
      char *namespace_name = next_token(&cursor);
      db_selector_t selector;
      long count = 0;
      if (!namespace_name) {
        fprintf(stdout, "uso: COUNT <namespace>\n");
        continue;
      }
      memset(&selector, 0, sizeof selector);
      selector.namespace_name = namespace_name;
      selector.skip_blob = 1;
      if (db_read(&db, &selector, count_record, &count) != 0)
        fprintf(stdout, "ERR\n");
      else
        fprintf(stdout, "count=%ld\n", count);
    } else if (strcmp(cmd, "COMPACT") == 0) {
      fprintf(stdout, "%s\n", db_compact(&db) == 0 ? "OK" : "ERR");
    } else {
      fprintf(stdout, "comando desconocido\n");
    }
  }

  db_close(&db);
  return 0;
}
