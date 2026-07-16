#ifndef HANDLERS_H_
#define HANDLERS_H_

#include "client.h"

void dispatch_request(client *cl, const char *method, const char *file);

#endif // HANDLERS_H_
