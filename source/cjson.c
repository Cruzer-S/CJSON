#include "cjson.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <uchar.h>
#include <inttypes.h>

#define SKIP_WHITESPACE(STR) for (; isspace(*(STR)); (STR)++)
#define INDENT_TO(N) for (size_t i = 0; i < N; i++) fputc('\t', stdout);
#define DEFAULT_STRING_SIZE 64
#define DEFAULT_BUFFER_SIZE BUFSIZ

///////////////////////////////////////////////////////////////////////////////
/// Data Structure
///////////////////////////////////////////////////////////////////////////////
struct cjson_array {
	struct cjson_value *value;
	struct cjson_array *next;
};

struct cjson_pair {
	char *key;
	struct cjson_value *value;

	struct cjson_pair *next;
};

struct cjson_object {
	struct cjson_pair *head;
};
///////////////////////////////////////////////////////////////////////////////
/// Declaration
///////////////////////////////////////////////////////////////////////////////
/// Parser
int parse_escaped_unicode(char *escape, char **endptr, char *ret);
int parse_escape(char *escape, char **endptr, char ret[MB_CUR_MAX]);

struct cjson_object *parse_object(char *, char **);
struct cjson_pair *parse_pair(char *, char **);
char *parse_string(char *json, char **endptr);
struct cjson_value *parse_value(char *, char **);
struct cjson_array *parse_array(char *json, char **endptr);

/// Destroy 
void destroy_object(struct cjson_object *object);
void destroy_pair(struct cjson_pair * pair);
void destroy_value(struct cjson_value *value);
void destroy_array(struct cjson_array *array);

/// Print
void print_object(struct cjson_object *object, size_t level);
void print_value(struct cjson_value *value, size_t level);
void print_array(struct cjson_array *array, size_t level);

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
	struct cjson_object * object;
	
	if (*json++ != '{') // move to next character 
		goto RETURN_NULL;

	object = malloc(sizeof(struct cjson_object));
	if (object == NULL)
		goto RETURN_NULL;

	object->head = NULL;

	SKIP_WHITESPACE(json);

	for (struct cjson_pair *cur, *pair;
	     *json != '}' && *json != '\0'; )
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

DESTROY_OBJECT:	destroy_object(object);
RETURN_NULL:	return NULL;
}

struct cjson_pair *parse_pair(char *json, char **endptr)
{
	struct cjson_pair * pair = malloc(sizeof(struct cjson_pair));
	if (pair == NULL)
		goto RETURN_NULL;

	pair->key = parse_string(json, &json);
	if (pair->key == NULL)
		goto FREE_PAIR;

	SKIP_WHITESPACE(json);

	if (*json++ != ':') // move to next character
		goto FREE_KEY;

	SKIP_WHITESPACE(json);
	pair->value = parse_value(json, &json);
	if (pair->value == NULL)
		goto FREE_KEY;

	pair->next = NULL;

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

struct cjson_value *parse_value(char *json, char **endptr)
{
	struct cjson_value *value = malloc(sizeof(struct cjson_value));
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

struct cjson_array *parse_array(char *json, char **endptr)
{
	struct cjson_array *head = NULL;

	if (*json++ != '[')
		goto RETURN_NULL;

	SKIP_WHITESPACE(json);
	for (struct cjson_array *cur = NULL, *array;
	     *json != ']' && *json != '\0'; )
	{
		array = malloc(sizeof(struct cjson_array));
		if (array == NULL)
			goto DESTROY_ARRAY;

		array->value = parse_value(json, &json);
		if (array->value == NULL) {
			free(array);
			goto DESTROY_ARRAY;
		}

		array->next = NULL;

		SKIP_WHITESPACE(json);
		if (*json == ',') {
			json++;
			SKIP_WHITESPACE(json);
		}

		if (head == NULL) {
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

DESTROY_ARRAY:	destroy_array(head);
RETURN_NULL:	return NULL;
}

/// Print
void print_object(struct cjson_object *object, size_t level)
{
	fputs("{\n", stdout);

	for (struct cjson_pair *pair = object->head; pair; pair = pair->next)
	{
		INDENT_TO(level);

		printf("\"%s\": ", pair->key);
		print_value(pair->value, level);
		
		if (pair->next) fputc(',', stdout);
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

	for (; array; array = array->next) {
		INDENT_TO(level);
		print_value(array->value, level);

		if (array->next) fputc(',', stdout);
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
	for (struct cjson_pair *temp, *pair = object->head; pair; pair = temp)
	{
		temp = pair->next;
		destroy_pair(pair);
	}

	free(object);
}

void destroy_pair(struct cjson_pair * pair)
{
	free(pair->key);
	destroy_value(pair->value);

	free(pair);
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

	free(value);
}

void destroy_array(struct cjson_array *array)
{
	for (struct cjson_array *temp; array; array = temp)
	{
		temp = array->next;
		destroy_value(array->value);
		free(array);
	}
}

///////////////////////////////////////////////////////////////////////////////
/// Implementation 
///////////////////////////////////////////////////////////////////////////////
struct cjson_object *cjson_create_object(char *json)
{
	return parse_object(json, NULL);
}

void cjson_destroy(struct cjson_object *cjson)
{
	destroy_object(cjson);
}

struct cjson_value *cjson_get_by_key(struct cjson_object *cjson, char *key)
{
	for (struct cjson_pair *temp, *pair = cjson->head;
      	     pair; temp = pair->next, pair = temp)
		if ( !strcmp(pair->key, key) )
			return pair->value;

	return NULL;
}

struct cjson_value *cjson_start(struct cjson_array *array)
{
	return NULL;
}
