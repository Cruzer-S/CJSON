#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "cjson.h"

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
		size_t readlen;

		if (index + bufsiz <= BUFSIZ) {
			char *new_buffer = realloc(buffer, bufsiz * 2);
			if (new_buffer == NULL)
				goto FREE_BUFFER;

			buffer = new_buffer;
			bufsiz *= 2;
		}

		readlen = fread(&buffer[index], 1, BUFSIZ, fp);
		
		index += readlen;
	}

	fclose(fp);

	return buffer;

FREE_BUFFER:	free(buffer);
CLOSE_FILE:	fclose(fp);
RETURN_NULL:	return NULL;
}

int main(int argc, char *argv[])
{
	CJSON cjson;
	char *contents;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <filename>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	contents = read_file(argv[1]);
	if (contents == NULL) {
		fprintf(stderr, "Failed to read file %s!\n", contents);
		exit(EXIT_FAILURE);
	}

	cjson = cjson_create(contents);

	cjson_print(cjson); fputc('\n', stdout);

	cjson_destroy(cjson);

	free(contents);

	return 0;
}
