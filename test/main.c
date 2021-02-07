/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdio.h>

#define CLOGGER_IMPLEMENT
#include "clogger.h"

// FIXME:
#include "../src/common.h"
#include "../src/var.h"
#include "../src/vm.h"

#include "../src/types/gen/string_buffer.h"
#include "../src/types/gen/byte_buffer.h"

int main() {
	clogger_init();
	//clogger_logfError("[DummyError] dummy error\n");
	//clogger_logfWarning("[DummyWarning] dummy warning\n");

	FILE* fp = fopen("test.ms", "r");
	if (fp != NULL) {
		char buff[1024];
		size_t read = fread(buff, 1, sizeof(buff), fp);
		buff[read] = '\0';
		printf("%s\n", buff);
		fclose(fp);
	} else {
		clogger_logfError("[Error] cannot open file test.ms\n");
	}

	VM* vm = (VM*)malloc(sizeof(VM));
	memset(vm, 0, sizeof(VM));

	ByteBuffer buff;
	byteBufferInit(&buff);

	byteBufferWrite(&buff, vm, 'a');
	byteBufferWrite(&buff, vm, 'b');
	byteBufferWrite(&buff, vm, 'c');

	String* str = newString(vm, (const char*)buff.data, 3);
	Var vstr = VAR_OBJ(&str->_super);
	if (strcmp(AS_CSTRING(vstr), "abc") != 0) {
		clogger_logfError("[Error] something went wrong.\n");
	}

	compileSource(vm, "native someNativeFn(a, b, c);\n");

	return 0;
}
