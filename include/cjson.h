#ifndef CJSON_H__
#define CJSON_H__

#include <stdbool.h>
#include <stddef.h>

typedef struct cjson *CJSON;

CJSON cjson_create(char *json);
char *cjson_to_json(CJSON );

void cjson_destroy(CJSON );

#endif
