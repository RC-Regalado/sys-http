#ifndef __REQUESTS_H
#define __REQUESTS_H

#include "client.h"

enum request_status {
  OK,
  INTERNAL_ERROR,
  FORBIDDEN,
  NOT_FOUND,
  UNKNOWN,
  METHOD_NOT_ALLOWED,
  LENGTH_REQUIRED,
  PAYLOAD_TOO_LARGE,
  UNSUPPORTED_MEDIA_TYPE
};

void write_headers(int client, enum request_status status);
int write_json_reply(int client, enum request_status status, const char *key,
                     int ok);

void write_response(client *cl);
int read_incoming(client *cl);

#endif // !__REQUESTS_H
