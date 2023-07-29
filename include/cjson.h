#ifndef CJSON_H__
#define CJSON_H__

#include <stdbool.h>

#include "Cruzer-S/cmacro/cmacro.h"

#define cjson_create(...) EXCONCAT(					\
	cjson_create_,NARGS(__VA_ARGS__)(__VA_ARGS__)			\
)
#define cjson_create_0 cjson_create_empty
#define cjson_create_1 cjson_create_from_file

typedef struct cjson *CJSON;
typedef void *cjson_array;

enum cjson_value_type
{
	CJSON_VALUE_TYPE_INTEGER,
	CJSON_VALUE_TYPE_FLOAT,
	CJSON_VALUE_TYPE_EXPONENT,
	CJSON_VALUE_TYPE_STRING,
	CJSON_VALUE_TYPE_BOOLEAN,
	CJSON_VALUE_TYPE_OBJECT,
	CJSON_VALUE_TYPE_ARRAY,
	CJSON_VALUE_TYPE_NULL
};

CJSON cjson_create_empty(void);
CJSON cjson_create_from_file(const char *filename);

bool cjson_has_key(CJSON cjson,const char *key);
enum cjson_value_type cjson_get_type(CJSON cjson, const char *key);

long cjson_get_int(CJSON cjson, const char *key);
double cjson_get_float(CJSON cjson, const char *key);
char *cjson_get_string(CJSON cjson, const char *key);
bool cjson_get_boolean(CJSON cjson, const char *key);
CJSON cjson_get_object(CJSON cjson, const char *key);
cjson_array cjson_get_array(CJSON cjson, const char *key);

void cjson_destroy(CJSON );

#endif
