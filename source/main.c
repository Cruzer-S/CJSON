#include <stdio.h>

#include "cjson.h"

int main(void)
{
	CJson cjson = cjson_create("sample.json");

	cjson_destroy(cjson);

	return 0;
}
