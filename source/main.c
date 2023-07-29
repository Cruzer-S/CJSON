#include <stdio.h>

#include "cjson.h"

int main(void)
{
	CJSON cjson = cjson_create("sample.json");

	cjson_destroy(cjson);

	return 0;
}
