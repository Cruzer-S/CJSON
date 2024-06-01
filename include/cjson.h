#ifndef CJSON_H__
#define CJSON_H__

#include <stdbool.h>
#include <stddef.h>

enum cjson_value_type {
	CJSON_VALUE_TYPE_BOOLEAN,
	CJSON_VALUE_TYPE_NULL,
	CJSON_VALUE_TYPE_NUMBER,
	CJSON_VALUE_TYPE_STRING,
	CJSON_VALUE_TYPE_ARRAY,
	CJSON_VALUE_TYPE_OBJECT
};

struct cjson_value {
	enum cjson_value_type type;

	union {
		double n;
		char *s;
		struct cjson_array *a;
		struct cjson_object *o;
		bool b;
	};
};

struct cjson_object *cjson_create_object(char *json);
struct cjson_array *cjson_create_array(char *json);

void cjson_print(struct cjson_object *cjson);

#define cjson_get(A, ...) _Generic((A), 				\
	struct cjson_object *: cjson_get_by_key,			\
	struct cjson_array *: cjson_get_by_index			\
)(A, __VA_ARGS__)

#define cjson_add(A, ...) _Generic((A), 				\
	struct cjson_object *: cjson_add_in_object,			\
	struct cjson_array *: cjson_add_in_list				\
)(A, __VA_ARGS__)

#define cjson_del(A, ...) _Generic((A), 				\
	struct cjson_object *: cjson_del_by_key,			\
	struct cjson_array *: cjson_del_by_index			\
)(A, __VA_ARGS__)

struct cjson_value *cjson_get_by_key(struct cjson_object *cjson, char *key);
struct cjson_value *cjson_get_by_index(struct cjson_array *array, int index);

bool cjson_del_by_key(struct cjson_object *cjson, char *key);
bool cjson_del_by_index(struct cjson_object *cjson, int index);

bool cjson_add_in_object(
	struct cjson_object *cjson,
	struct cjson_value value
);
bool cjson_add_in_array(
	struct cjson_array *cjson,
	struct cjson_value value 
);

struct cjson_value *cjson_foreach(struct cjson_object *cjson);
struct cjson_value *cjson_foreach_next(void);
void cjson_foreach_insert(struct cjson_value value);
void cjson_foreach_remove(void);

void cjson_destroy(struct cjson_object *cjson);

#endif
