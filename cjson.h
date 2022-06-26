#ifndef CJSON_H__
#define CJSON_H__


enum cjson_type;
struct cjson_value;
struct cjson;

enum cjson_type {
	CJSON_TYPE_INTEGER,
	CJSON_TYPE_STRING,
	CJSON_TYPE_BOOLEAN,
	CJSON_TYPE_OBJECT,
	CJSON_TYPE_ARRAY
};

struct cjson_value {
	enum cjson_type type;

	union {
		int number;
		char *string;
		bool boolean;
		void *object;
		struct cjson *array;
	} value;
};

struct cjson {
	char *key;
	struct cjson_value value;

	struct cjson *next;
};

#endif
