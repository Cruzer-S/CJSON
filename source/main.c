#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <locale.h>

#include "cjson.h"

#define LOCALE "en_US.UTF-8"

#define EXPAND_TAB(N) for (int i = 0; i < N; i++) fputc('\t', stdout)

char *read_file(const char *filename)
{
	FILE *fp;
	char *buffer;

	int bufsiz = BUFSIZ + 1;
	int index = 0;

	fp = fopen(filename, "r");
	if (fp == NULL)
		goto RETURN_NULL;

	buffer = malloc(bufsiz);
	if (buffer == NULL)
		goto CLOSE_FILE;

	while ( !feof(fp) ) {
		if (index + bufsiz < BUFSIZ) {
			char *new_buffer = realloc(buffer, bufsiz * 2);
			if (new_buffer == NULL)
				goto FREE_BUFFER;

			buffer = new_buffer;
			bufsiz *= 2;
		}

		index += fread(&buffer[index], 1, BUFSIZ, fp);
	}

	buffer[index] = '\0';

	fclose(fp);

	return buffer;

FREE_BUFFER:	free(buffer);
CLOSE_FILE:	fclose(fp);
RETURN_NULL:	return NULL;
}

void print_value(struct cjson_value *value, int depth);
void print_object(struct cjson_object *json, int depth);
void print_array(struct cjson_array *array, int depth);

void print_value(struct cjson_value *value, int depth)
{
	switch(value->type)
	{
	case CJSON_VALUE_TYPE_NULL:
		fputs("null", stdout);
		break;

	case CJSON_VALUE_TYPE_ARRAY:
		print_array(value->a, depth + 1);
		break;

	case CJSON_VALUE_TYPE_NUMBER:
		printf("%lf", value->n);
		break;

	case CJSON_VALUE_TYPE_STRING:
		printf("\"%s\"", value->s, stdout);
		break;

	case CJSON_VALUE_TYPE_BOOLEAN:
		fputs(value->b ? "true" : "false", stdout);
		break;

	case CJSON_VALUE_TYPE_OBJECT:
		print_object(value->o, depth + 1);
		break;
	}

}

void print_array(struct cjson_array *array, int depth)
{
	char buffer = '\0';
	fputc('[', stdout);

	for (struct cjson_value *value = cjson_foreach(array);
	     value; value = cjson_foreach_next(array))
	{
		if (buffer != '\0') fputc(buffer, stdout);
		else 		    buffer = ',';

		fputc('\n', stdout);

		EXPAND_TAB(depth);
		print_value(value, depth);
	}

	fputc('\n', stdout);
	EXPAND_TAB(depth - 1);
	fputc(']', stdout);
}

void print_object(struct cjson_object *json, int depth)
{
	char buffer = '\0';

	fputc('{', stdout);

	for (char *key = cjson_foreach(json);
	     key; key = cjson_foreach_next(json))
	{
		if (buffer != '\0') fputc(buffer, stdout);
		else		    buffer = ',';

		fputc('\n', stdout);

		EXPAND_TAB(depth);
		printf("%s: ", key);

		struct cjson_value *value = cjson_get(json, key);
		print_value(value, depth);
	}

	fputc('\n', stdout);
	EXPAND_TAB(depth - 1);
	fputc('}', stdout);
}

int main(int argc, char *argv[])
{
	struct cjson_object *cjson;
	char *contents;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <filename>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (strcmp(setlocale(LC_ALL, LOCALE), LOCALE)) {
		fprintf(stderr, "Failed to set locale: %s\n", LOCALE);
		exit(EXIT_FAILURE);
	}

	contents = read_file(argv[1]);
	if (contents == NULL) {
		fprintf(stderr, "Failed to read file %s!\n", contents);
		exit(EXIT_FAILURE);
	}

	cjson = cjson_create_object(contents);
	if (cjson == NULL) {
		fprintf(stderr, "Failed to create json object!\n");
		free(contents);
		exit(EXIT_FAILURE);
	}

	cjson_print(cjson); fputc('\n', stdout);

	print_object(cjson, 1); fputc('\n', stdout);
	
	cjson_destroy_object(cjson);

	free(contents);

	return 0;
}
