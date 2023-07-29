#ifndef _CJSON_INTERNAL_H__
#define _CJSON_INTERNAL_H__

#ifdef _CJSON_INTERNAL

#include <stdbool.h>
#include <stdio.h>

struct cjson
{
	struct cjson_entry *entry;

	size_t total;
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

struct cjson_entry
{
	char *key;
	
	struct cjson_value *value;

	struct cjson_entry *next;
};

typedef struct cjson_value *Value;
typedef struct cjson_entry *Entry;
typedef enum cjson_value_type *Type;

#endif

#endif
