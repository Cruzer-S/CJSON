#ifndef CJSON_H__
#define CJSON_H__

#include <stdbool.h>
#include <stddef.h>

typedef void *CJSON;

CJSON cjson_create(char *json);

void cjson_print(CJSON cjson);

void cjson_destroy(CJSON );

#endif
