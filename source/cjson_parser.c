#include "cjson_parser.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#define _CJSON_INTERNAL
#include "cjson_internal.h"

#define SKIP_WHITESPACE(STRING) while (isspace(*(STRING))) (STRING)++

static char *cjson_parse_string(const char *value, const char *(*endptr))
{
	const char *start, *end;
	char *string;
	size_t length; 

	start = value;

	if (*start++ != '\"')
		return NULL;

	for (end = strchr(start, '\"');
      	     end != NULL && *(end - 1) == '\\';
	     end = strchr(end + 1, '\"')) /* empty loop body */ ;

	if (end == NULL)
		return NULL;	
	
	length = end - start;
	string = malloc(length + 1);
	if (string == NULL)
		return NULL;

	memcpy(string, start, length);
	string[length] = '\0';

	if (endptr != NULL)
		*endptr = end + 1;

	return string;
}

Value cjson_parse_value(const char *string, const char *(*endptr))
{
	Value value;
	int length;

	value = malloc(sizeof(struct cjson_value));
	if (value == NULL)
		goto RETURN_NULL;

	char test[100];

	SKIP_WHITESPACE(string);

	if (sscanf(string, "%ld%n", &value->i, &length) == 1 
	 && *(string + length) != '.') {
		value->type = CJSON_VALUE_TYPE_INTEGER; 
	} else if (sscanf(string, "%lf%n", &value->f, &length) == 1) {
		value->type = CJSON_VALUE_TYPE_FLOAT;
	} else if ( !strncmp(string, "true", 4) ) {
		value->type = CJSON_VALUE_TYPE_BOOLEAN;
		value->b = true;
		length = 4;
	} else if ( !strncmp(string, "false", 5) ) {
		value->type = CJSON_VALUE_TYPE_BOOLEAN;
		value->b = false;
		length = 5;
	} else if ( !strncmp(string, "null", 4) ) {
		value->type = CJSON_VALUE_TYPE_NULL;
		length = 4;
	} else if ( (value->s = cjson_parse_string(string, endptr)) ) {
		value->type = CJSON_VALUE_TYPE_STRING;

		if (endptr != NULL)
			length = *endptr - string;
	} else {
		goto FREE_VALUE;
	}

	if (endptr != NULL)
		*endptr = string + length;

	return value;

FREE_VALUE:	free(value);
RETURN_NULL:	return NULL;
}

static Entry cjson_parse_entry(const char *string, const char **endptr)
{
	Entry entry = malloc(sizeof(struct cjson_entry));
	if (entry == NULL)
		goto RETURN_NULL;

	entry->key = cjson_parse_string(string, &string);
	if (entry->key == NULL)
		goto FREE_ENTRY;

	SKIP_WHITESPACE(string);

	if (*string++ != ':')
		goto FREE_KEY;

	SKIP_WHITESPACE(string);

	entry->value = cjson_parse_value(string, &string);
	if (entry->value == NULL)
		goto FREE_KEY;

	if (endptr != NULL)
		*endptr = string;

	entry->next = NULL;

	return entry;

FREE_KEY:	free(entry->key);
FREE_ENTRY:	free(entry);
RETURN_NULL: 	return NULL;
}

bool cjson_parse(CJSON cjson, const char *string)
{
	Entry start, entry, prev;

	if (*string++ != '{')
		goto RETURN_FALSE;

	SKIP_WHITESPACE(string);
	if (*string == '}')
		return true;

	SKIP_WHITESPACE(string);
	for (prev = NULL, start = entry = cjson_parse_entry(string, &string);
	     entry != NULL;
	     entry = cjson_parse_entry(string, &string))
	{
		cjson->total++;

		SKIP_WHITESPACE(string);
		if (*string != ',')
			break;
		else
			string++;

		if (prev == NULL)
			prev = entry;
		else
			prev->next = entry, prev = entry;

		SKIP_WHITESPACE(string);
	}

	if (start == NULL)
		goto RETURN_FALSE;

	if (entry == NULL)
		goto FREE_ENTRIES;
	
	SKIP_WHITESPACE(string);
	if (*string != '}')
		goto FREE_ENTRIES;

	cjson->entry = start;

	return true;

FREE_ENTRIES:	for (Entry p = start; p != NULL; ) {
			Entry temp;

			free(p->key);

			if (p->value->type == CJSON_VALUE_TYPE_STRING)
				free(p->value->s);

			free(p->value);
			temp = p;

			p = p->next;
			free(temp);
		}
RETURN_FALSE:	return false;
}
