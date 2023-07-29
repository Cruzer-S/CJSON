#include "cjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "cjson_parser.h"

#define _CJSON_INTERNAL
#include "cjson_internal.h"

CJSON cjson_create_empty(void)
{
	CJSON cjson;

	cjson = malloc(sizeof(struct cjson));
	if (cjson == NULL)
		return NULL;

	cjson->entry = NULL;
	cjson->total = 0;

	return cjson;
}

CJSON cjson_create_from_file(const char *filename)
{
	CJSON cjson;
	FILE *fp;
	long length;
	char *string;

 	cjson = cjson_create_empty();
	if (cjson == NULL)
		goto RETURN_NULL;

	fp = fopen(filename, "r");
	if (fp == NULL)
		goto DESTROY_CJSON;

	if (fseek(fp, 0L, SEEK_END) != 0)
		goto CLOSE_FILE;
	
	length = ftell(fp); rewind(fp);

	string = malloc(length);
	if (string == NULL)
		goto CLOSE_FILE;

	for (long read, total = 0; total < length; total += read) {
		read = fread(string + total, 1, BUFSIZ, fp);

		if (read < 0)
			goto FREE_STRING;
	}
	
	fclose(fp); fp = NULL;

	if ( !cjson_parse(cjson, string) )
		goto FREE_STRING;

	free(string);

	return cjson;

FREE_STRING:	free(string);
CLOSE_FILE:	if (fp != NULL) fclose(fp);
DESTROY_CJSON:	cjson_destroy(cjson);
RETURN_NULL: 	return NULL;
}

void cjson_destroy(CJSON cjson)
{
	free(cjson);
}
