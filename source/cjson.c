#include "cjson.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <uchar.h>
#include <inttypes.h>

#include "Cruzer-S/list/list.h"

#define SKIP_WHITESPACE(STR) for (; isspace(*(STR)); (STR)++)
#define INDENT_TO(N) for (size_t i = 0; i < N; i++) fputc('\t', stdout);
#define DEFAULT_STRING_SIZE 64
#define DEFAULT_BUFFER_SIZE BUFSIZ

///////////////////////////////////////////////////////////////////////////////
/// Data Structure
///////////////////////////////////////////////////////////////////////////////
struct cjson_entry {
	struct cjson_value value;

	struct list list;
};

struct cjson_pair {
	char *key;
	struct cjson_value value;

	struct list list;
};

struct cjson_object {
	struct list head;
	struct list *pointer;
};

struct cjson_array {
	struct list head;
	struct list *pointer;
};
///////////////////////////////////////////////////////////////////////////////
/// Declaration
///////////////////////////////////////////////////////////////////////////////
/// Parser
static int parse_escaped_unicode(char *escape, char **endptr, char *ret);
static int parse_escape(char *escape, char **endptr, char ret[MB_CUR_MAX]);

static struct cjson_object *parse_object(char *, char **);
static struct cjson_pair *parse_pair(char *, char **);
static char *parse_string(char *json, char **endptr);
static bool parse_value(char *, char **, struct cjson_value *);
static struct cjson_array *parse_array(char *json, char **endptr);

// Check
static bool check_duplicated_key(struct cjson_object *object, char *key);

/// Destroy 
static void destroy_object(struct cjson_object *object);
static void destroy_pair(struct cjson_pair *pair);
static void destroy_value(struct cjson_value *value);
static void destroy_array(struct cjson_array *array);

/// Print
static void print_object(struct cjson_object *object, size_t level);
static void print_value(struct cjson_value *value, size_t level);
static void print_array(struct cjson_array *array, size_t level);

///////////////////////////////////////////////////////////////////////////////
/// Definition
///////////////////////////////////////////////////////////////////////////////
/// Parser
int parse_escaped_unicode(char *escape, char **endptr, char *ret)
{
	char16_t chr;
	mbstate_t state;
	size_t length;
	int nread;

	memset(&state, 0, sizeof(state));

	if (sscanf(escape, "\\u%4" SCNx16 "%n", &chr, &nread) == 0)
		return -1;

	escape += nread; // skip escaped unicode

	length = c16rtomb(ret, chr, &state);
	if (length == 0) {
		if (sscanf(escape, "\\u%4" SCNx16 "%n", &chr, &nread) == 0)
			return -1;

		escape += nread;

		length = c16rtomb(ret, chr, &state);
	}

	if (endptr != NULL)
		*endptr = escape;

	return length;
}

int parse_escape(char *escape, char **endptr, char ret[MB_CUR_MAX])
{
	int length = 1;

	if (*escape++ != '\\')
		return -1;

	switch (*escape++) {
	case '/':  ret[0] = '/';  break;
	case '\"': ret[0] = '\"'; break;
	case '\\': ret[0] = '\\'; break;
	case 'b':  ret[0] = '\b'; break;
	case 'f':  ret[0] = '\f'; break;
	case 'n':  ret[0] = '\n'; break;
	case 'r':  ret[0] = '\r'; break;
	case 't':  ret[0] = '\t'; break;

	case 'u': length = parse_escaped_unicode(escape - 2, &escape, ret);
		  break;

	default: return -1;
	}

	if (endptr != NULL)
		*endptr = escape;

	return length;
}

struct cjson_object *parse_object(char *json, char **endptr)
{
	struct cjson_object *object;
	
	if (*json++ != '{') // move to next character 
		goto RETURN_NULL;

	object = malloc(sizeof(struct cjson_object));
	if (object == NULL)
		goto RETURN_NULL;

	list_init_head(&object->head);

	SKIP_WHITESPACE(json);
	for (struct cjson_pair *cur, *pair; *json != '}' && *json != '\0'; )
	{
		pair = parse_pair(json, &json);
		if (pair == NULL)
			goto DESTROY_OBJECT;

		if (check_duplicated_key(object, pair->key)) {
			destroy_pair(pair);
			goto DESTROY_OBJECT;
		}

		list_add_tail(&object->head, &pair->list);
		
		SKIP_WHITESPACE(json);
		if (*json == ',')	json++;
		else if (*json != '}')	goto DESTROY_OBJECT;
		SKIP_WHITESPACE(json);
	}

	if (*json++ != '}') // move to next character
		goto DESTROY_OBJECT;

	if (endptr != NULL)
		*endptr = json;

	return object;

DESTROY_OBJECT:	destroy_object(object);
RETURN_NULL:	return NULL;
}

struct cjson_pair *parse_pair(char *json, char **endptr)
{
	struct cjson_pair *pair = malloc(sizeof(struct cjson_pair));
	if (pair == NULL)
		goto RETURN_NULL;

	pair->key = parse_string(json, &json);
	if (pair->key == NULL)
		goto FREE_PAIR;

	SKIP_WHITESPACE(json);

	if (*json++ != ':') // move to next character
		goto FREE_KEY;

	SKIP_WHITESPACE(json);
	if ( !parse_value(json, &json, &pair->value) )
		goto FREE_KEY;

	if (endptr != NULL)
		*endptr = json;

	return pair;

FREE_KEY:	free(pair->key);
FREE_PAIR:	free(pair);
RETURN_NULL:	return NULL;
}

char *parse_string(char *json, char **endptr)
{
	size_t index = 0;
	size_t reqsiz = 0;
	size_t alloc_size = DEFAULT_STRING_SIZE;
	char *token;

	if (*json != '\"')
		goto RETURN_NULL;

	json++; // skip '"'
	
	token = malloc(alloc_size);
	if (token == NULL)
		goto RETURN_NULL;

	while (*json != '\"')
	{
		reqsiz = index + ((*json == '\\') ? MB_CUR_MAX + 1 : 1);
		if (alloc_size < reqsiz) {
			char *new_token;
			for (; alloc_size < reqsiz; alloc_size *= 2);

			new_token = realloc(token, alloc_size);
			if (new_token == NULL)
				goto FREE_TOKEN;

			token = new_token;
		}

		if (*json == '\\') {
			int len = parse_escape(json, &json, &token[index]);
			if (len == -1)
				goto FREE_TOKEN;

			index += len;
		} else {
			token[index++] = *json++;
		}
	}

	json++; // skip '"'

	token[index] = '\0';
	if (endptr != NULL)
		*endptr = json;

	return token;

FREE_TOKEN:	free(token);
RETURN_NULL:	return NULL;
}

bool parse_value(char *json, char **endptr, struct cjson_value *value)
{
	switch (*json) {
	case '{':  value->type = CJSON_VALUE_TYPE_OBJECT;  break;
	case '[':  value->type = CJSON_VALUE_TYPE_ARRAY;   break;
	case '\"': value->type = CJSON_VALUE_TYPE_STRING;  break;
	case 't': case 'f':
		   value->type = CJSON_VALUE_TYPE_BOOLEAN; break;
	case 'n':  value->type = CJSON_VALUE_TYPE_NULL;	   break;
	default:   value->type = CJSON_VALUE_TYPE_NUMBER;  break;
	}

	switch (value->type) {
	case CJSON_VALUE_TYPE_NUMBER: {
		int nread;
		if (sscanf(json, "%lf%n", &value->n, &nread) != 1)
			goto RETURN_NULL;

		json += nread;
	}	break;

	case CJSON_VALUE_TYPE_NULL:
		json += 4;
		break;

	case CJSON_VALUE_TYPE_STRING:
		value->s = parse_string(json, &json);
		if (value->s == NULL)
			goto RETURN_NULL; 
		break;

	case CJSON_VALUE_TYPE_BOOLEAN:
		if ( !strncmp(json, "true", 4) ) {
			value->b = true;
			json += 4;
		} else if ( !strncmp(json, "false", 5) ) {
			value->b = false;
			json += 5;
		} else {
			goto RETURN_NULL;
		}
		break;

	case CJSON_VALUE_TYPE_ARRAY:
		value->a = parse_array(json, &json);
		if (value->a == NULL)
			goto RETURN_NULL;
		break;

	case CJSON_VALUE_TYPE_OBJECT:
		value->o = parse_object(json, &json);
		if (value->o == NULL)
			goto RETURN_NULL;
		break;
	}

	if (endptr != NULL)
		*endptr = json;

	return value;

RETURN_NULL:	return NULL;
}

struct cjson_array *parse_array(char *json, char **endptr)
{
	struct cjson_array *array;

	if (*json++ != '[')
		goto RETURN_NULL;

	array = malloc(sizeof(struct cjson_array));
	if (array == NULL)
		goto RETURN_NULL;

	list_init_head(&array->head);

	SKIP_WHITESPACE(json);
	for (struct cjson_entry *cur, *entry; *json != ']' && *json != '\0'; )
	{
		entry = malloc(sizeof(struct cjson_entry));
		if (entry == NULL)
			goto DESTROY_ARRAY;

		if ( !parse_value(json, &json, &entry->value) ) {
			free(entry);
			goto DESTROY_ARRAY;
		}

		list_add_tail(&array->head, &entry->list);

		SKIP_WHITESPACE(json);
		if (*json == ',')	json++;
		else if (*json != ']')	goto DESTROY_ARRAY;
		SKIP_WHITESPACE(json);

	}

	if (*json++ != ']')
		goto DESTROY_ARRAY;

	if (endptr != NULL)
		*endptr = json;
	
	return array;

DESTROY_ARRAY:	destroy_array(array);
RETURN_NULL:	return NULL;
}

// Check
bool check_duplicated_key(struct cjson_object *object, char *key)
{
	LIST_FOREACH_ENTRY(&object->head, pair, struct cjson_pair, list)
		if ( !strcmp(key, pair->key) )
			return true;

	return false;
}

/// Print
void print_object(struct cjson_object *object, size_t level)
{
	fputs("{\n", stdout);

	LIST_FOREACH_ENTRY(&object->head, pair, struct cjson_pair, list)
	{
		INDENT_TO(level);

		printf("\"%s\": ", pair->key);
		print_value(&pair->value, level);
		
		if ( !LIST_ENTRY_IS_LAST(&object->head, pair, list) )
			fputc(',', stdout);
		fputc('\n', stdout);
	}

	INDENT_TO(level - 1);

	fputc('}', stdout);
}

void print_value(struct cjson_value *value, size_t level)
{
	switch(value->type) {
	case CJSON_VALUE_TYPE_NULL:
		fputs("null", stdout);
		break;
	case CJSON_VALUE_TYPE_ARRAY:
		print_array(value->a, level + 1);
		break;
	case CJSON_VALUE_TYPE_NUMBER:
		printf("%lf", value->n);
		break;
	case CJSON_VALUE_TYPE_OBJECT:
		print_object(value->o, level + 1);
		break;
	case CJSON_VALUE_TYPE_STRING:
		printf("\"%s\"", value->s);
		break;
	case CJSON_VALUE_TYPE_BOOLEAN:
		fputs(value->b ? "true" : "false", stdout);
		break;
	}
}

void print_array(struct cjson_array *array, size_t level)
{
	fputs("[\n", stdout);

	LIST_FOREACH(&array->head, a) {
		struct cjson_entry *entry = LIST_ENTRY(
			a, struct cjson_entry, list
		);

		INDENT_TO(level);

		print_value(&entry->value, level);

		if ( !LIST_IS_LAST(&array->head, a) )
			fputc(',', stdout);

		fputc('\n', stdout);
	}

	INDENT_TO(level - 1);
	fputc(']', stdout);
}

void cjson_print(struct cjson_object *cjson)
{
	print_object(cjson, 1);
}

/// Destroy 
void destroy_object(struct cjson_object *object)
{
	LIST_FOREACH_SAFE(&object->head, p)
		destroy_pair(LIST_ENTRY(p, struct cjson_pair, list));

	free(object);
}

void destroy_pair(struct cjson_pair *pair)
{
	free(pair->key);
	destroy_value(&pair->value);

	free(pair);
}

void destroy_entry(struct cjson_entry *entry)
{
	destroy_value(&entry->value);
	free(entry);
}

void destroy_value(struct cjson_value *value)
{
	switch(value->type) {
	case CJSON_VALUE_TYPE_NUMBER:
	case CJSON_VALUE_TYPE_BOOLEAN:
	case CJSON_VALUE_TYPE_NULL:
		/* do nothing */
		break;
	case CJSON_VALUE_TYPE_ARRAY:
		destroy_array(value->a);
		break;

	case CJSON_VALUE_TYPE_OBJECT:
		destroy_object(value->o);
		break;

	case CJSON_VALUE_TYPE_STRING:
		free(value->s);
		break;
	}
}

void destroy_array(struct cjson_array *array)
{
	LIST_FOREACH_ENTRY_SAFE(&array->head, entry, struct cjson_entry, list)
		destroy_entry(entry);

	free(array);
}

///////////////////////////////////////////////////////////////////////////////
/// Implementation 
///////////////////////////////////////////////////////////////////////////////
struct cjson_object *cjson_create_object(char *json)
{
	return parse_object(json, NULL);
}

void cjson_destroy_object(struct cjson_object *cjson)
{
	destroy_object(cjson);
}

struct cjson_value *cjson_get_by_key(struct cjson_object *object, char *key)
{
	LIST_FOREACH_ENTRY(&object->head, pair, struct cjson_pair, list)
		if ( !strcmp(pair->key, key) )
			return &pair->value;

	return NULL;
}

struct cjson_value *cjson_get_by_index(struct cjson_array *array, int index)
{
	struct list *list = &array->head;

	for (; list && index > 0; list = list->next, index--);

	if (array != 0 || list == NULL)
		return NULL;

	return &LIST_ENTRY(list, struct cjson_entry, list)->value;
}


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

char *cjson_foreach_object(struct cjson_object *object)
{
	if (LIST_IS_EMPTY(&object->head))
		return NULL;

	object->pointer = object->head.next;

	return LIST_ENTRY(object->pointer, struct cjson_pair, list)->key;
}

char *cjson_foreach_object_next(struct cjson_object *object)
{
	if (LIST_IS_LAST(&object->head, object->pointer))
		return NULL;

	object->pointer = object->pointer->next;

	return LIST_ENTRY(object->pointer, struct cjson_pair, list)->key;
}

struct cjson_value *cjson_foreach_array(struct cjson_array *array)
{
	if (LIST_IS_EMPTY(&array->head))
		return NULL;

	array->pointer = array->head.next;

	return &LIST_ENTRY(array->pointer, struct cjson_entry, list)->value;
}

struct cjson_value *cjson_foreach_array_next(struct cjson_array *array)
{
	if (LIST_IS_LAST(&array->head, array->pointer))
		return NULL;

	array->pointer = array->pointer->next;

	return &LIST_ENTRY(array->pointer, struct cjson_entry, list)->value;
}

void cjson_foreach_insert(char *key, struct cjson_value value);
void cjson_foreach_remove(void);
