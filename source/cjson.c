#include "cjson.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <uchar.h>
#include <inttypes.h>

#define SKIP_WHITESPACE(STR) for (; isspace(*(STR)); (STR)++)
#define DEFAULT_STRING_SIZE 64

typedef enum cjson_value_type {
	CJSON_VALUE_TYPE_BOOLEAN,
	CJSON_VALUE_TYPE_NULL,
	CJSON_VALUE_TYPE_NUMBER,
	CJSON_VALUE_TYPE_STRING,
	CJSON_VALUE_TYPE_ARRAY,
	CJSON_VALUE_TYPE_OBJECT
} Type;

struct cjson {
	char *json;
	char *compress;

	struct cjson_object *head;
};

typedef struct cjson_array *Array;
typedef struct cjson_object *Object;

typedef struct cjson_value {
	Type type;

	union {
		double n;
		char *s;
		Array a;
		Object o;
		bool b;
	};
} *Value;

typedef struct cjson_array {
	Value value;

	struct cjson_array *next;
} *Array;

typedef struct cjson_pair {
	char *key;
	Value value;

	struct cjson_pair *next;
} *Pair;

typedef struct cjson_object {
	int npairs;

	Pair head;
} *Object;

Value parse_value(char *, char **);
Object parse_object(char *, char **);

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

static int parse_escape(char *escape, char **endptr, char ret[MB_CUR_MAX])
{
	int length = 1;

	if (*escape++ != '\\')
		return -1;

	switch (*escape++) {
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

void destroy_value(Value value)
{
	switch(value->type) {
	case CJSON_VALUE_TYPE_NUMBER:
	case CJSON_VALUE_TYPE_BOOLEAN:
	case CJSON_VALUE_TYPE_NULL:
		/* do nothing */
		break;
	case CJSON_VALUE_TYPE_ARRAY:

		break;

	case CJSON_VALUE_TYPE_OBJECT:
		break;

	case CJSON_VALUE_TYPE_STRING:
		free(value->s);
		break;
	}

	free(value);
}

Array parse_array(char *json, char **endptr)
{
	Array head = NULL;

	if (*json++ != '[')
		goto RETURN_NULL;

	SKIP_WHITESPACE(json);
	for (Array cur = NULL, array; *json != ']' && *json != ' '; )
	{
		array = malloc(sizeof(struct cjson_array));
		if (array == NULL)
			goto DESTROY_ARRAY;

		array->value = parse_value(json, &json);
		array->next = NULL;

		SKIP_WHITESPACE(json);
		if (*json == ',') {
			json++;
			SKIP_WHITESPACE(json);
		}

		if (cur == NULL) {
			head = array;
		} else {
			cur->next = array;
		}

		cur = array;
	}

	if (*json++ != ']')
		goto DESTROY_ARRAY;

	if (endptr != NULL)
		*endptr = json;

	return head;

DESTROY_ARRAY:	for (Array temp, cur = head; cur; cur = temp)
		{
			temp = cur->next;
			destroy_value(cur->value);
		}
			
RETURN_NULL:	return NULL;
}

static void *realloc2(void *json, size_t require, size_t *resize)
{
	char *new;
	size_t size = 1;

	while (size < require)
		size *= 2;

	new = realloc(json, size);
	if (resize != NULL)
		*resize = size;

	return new;
}

static char *parse_string(char *json, char **endptr)
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
			char *new = realloc2(token, alloc_size, &alloc_size);
			if (new == NULL)
				goto FREE_TOKEN;

			token = new;
		}

		if (*json == '\\')
			index += parse_escape(json, &json, &token[index]);
		else
			token[index++] = *json++;
	}

	json++; // skip '"'

	token[index] = '\0';
	if (endptr != NULL)
		*endptr = json;

	return token;

FREE_TOKEN:	free(token);
RETURN_NULL:	return NULL;
}

Value parse_value(char *json, char **endptr)
{
	Value value = malloc(sizeof(struct cjson_value));
	if (value == NULL)
		goto RETURN_NULL;

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
			goto FREE_VALUE;

		json += nread;
	}	break;

	case CJSON_VALUE_TYPE_NULL:
		json += 4;
		break;

	case CJSON_VALUE_TYPE_STRING:
		value->s = parse_string(json, &json);
		if (value->s == NULL)
			goto FREE_VALUE;
		break;

	case CJSON_VALUE_TYPE_BOOLEAN:
		if ( !strncmp(json, "true", 4) ) {
			value->b = true;
			json += 4;
		} else if ( !strncmp(json, "false", 5) ) {
			value->b = false;
			json += 5;
		} else {
			goto FREE_VALUE;
		}
		break;

	case CJSON_VALUE_TYPE_ARRAY:
		value->a = parse_array(json, &json);
		if (value->a == NULL)
			goto FREE_VALUE;
		break;

	case CJSON_VALUE_TYPE_OBJECT:
		value->o = parse_object(json, &json);
		if (value->o == NULL)
			goto FREE_VALUE;
		break;
	}

	if (endptr != NULL)
		*endptr = json;

	return value;

FREE_VALUE:	free(value);
RETURN_NULL:	return NULL;
}

void destroy_pair(Pair pair)
{
	free(pair->key);
	destroy_value(pair->value);

	free(pair);
}

Pair parse_pair(char *json, char **endptr)
{
	Pair pair = malloc(sizeof(struct cjson_pair));
	if (pair == NULL)
		goto RETURN_NULL;

	pair->key = parse_string(json, &json);
	if (pair->key == NULL)
		goto FREE_PAIR;

	SKIP_WHITESPACE(json);

	if (*json++ != ':') // move to next character
		goto FREE_PAIR;

	SKIP_WHITESPACE(json);
	pair->value = parse_value(json, &json);
	if (pair->value == NULL)
		goto FREE_PAIR;

	if (endptr != NULL)
		*endptr = json;

	return pair;

FREE_PAIR:	free(pair);
RETURN_NULL:	return NULL;
}

void cjson_destroy(CJSON cjson)
{
	free(cjson);
}

Object parse_object(char *json, char **endptr)
{
	Object object;
	
	if (*json++ != '{') // move to next character 
		goto RETURN_NULL;

	object = malloc(sizeof(struct cjson_object));
	if (object == NULL)
		goto RETURN_NULL;

	object->head = NULL;
	object->npairs = 0;

	SKIP_WHITESPACE(json);

	for (Pair cur, pair; *json != '}' && *json != '\0'; object->npairs++)
	{
		pair = parse_pair(json, &json);

		if (pair == NULL)
			goto DESTROY_OBJECT;

		if (object->head == NULL)
			object->head = pair;
		else
			cur->next = pair;

		cur = pair;

		SKIP_WHITESPACE(json);
		if (*json == ',') json++;

		SKIP_WHITESPACE(json);
	}

	if (*json++ != '}') // move to next character
		goto DESTROY_OBJECT;

	if (endptr != NULL)
		*endptr = json;

	return object;

DESTROY_OBJECT:	for (Pair pair = object->head, keep; pair; pair = keep) {
			keep = pair->next;
			destroy_pair(pair);
		}
FREE_OBJECT:	free(object);
RETURN_NULL:	return NULL;
}

CJSON cjson_create(char *json)
{
	CJSON cjson = malloc(sizeof(struct cjson));
	if (cjson == NULL)
		return NULL;

	cjson->json = json;
	cjson->head = parse_object(json, NULL);

	return cjson;
}
