#ifndef CJSON_H__
#define CJSON_H__

#include <stdbool.h>

#include "Cruzer-S/cmacro/cmacro.h"

#define cjson_create(...) EXCONCAT(					\
	cjson_create_,NARGS(__VA_ARGS__)(__VA_ARGS__)			\
)
#define cjson_create_0 cjson_create_empty
#define cjson_create_1 cjson_create_from_file

typedef struct cjson *CJson;
typedef void *cjson_array;
typedef struct cjson_value *CJsonValue;

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

struct cjson_value
{
	enum cjson_value_type type;

	union {
		long i;
		double f;
		char *s;
		bool b;
		struct cjson_value *o;
		struct cjson_value *a;
		struct cjson *j;
	};
};

CJson cjson_create_empty(void);
CJson cjson_create_from_file(const char *filename);

bool cjson_has_key(CJson cjson,const char *key);
CJsonValue cjson_get(CJson cjson, const char *key);

void cjson_destroy(CJson );

#endif
