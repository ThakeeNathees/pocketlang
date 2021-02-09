/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdio.h>

#define CLOGGER_IMPLEMENT
#include "clogger.h"

#include "miniscript.h"

// FIXME:
#include "../src/common.h"
#include "../src/var.h"
#include "../src/vm.h"

#include "../src/types/gen/string_buffer.h"
#include "../src/types/gen/byte_buffer.h"

void errorPrint(MSVM* vm, MSErrorType type, const char* file, int line,
                   const char* message) {
	printf("Error:%s\n\tat %s:%i\n", message, file, line);
}

void writeFunction(MSVM* vm, const char* text) {
	printf("%s", text);
}

void loadScriptDone(MSVM* vm, const char* path, void* user_data) {
	return;
}

MSLoadScriptResult loadScript(MSVM* vm, const char* path) {
	MSLoadScriptResult result;
	result.is_failed = false;

	result.source = ""
		"if -1+2 * 3\n"
		"end\n"
		;
	return result;

	// FIXME:
	FILE* f = fopen(path, "r");
	if (f == NULL) {
		result.is_failed = true;
		return result;
	}
	char* buff = (char*)malloc(1024);
	int i = 0;
	char c;
	while (c = fgetc(f)) {
		buff[i++] = c;
	}
	result.source = buff;
	fclose(f);
	
}

int main() {
	clogger_init();
	//clogger_logfError("[DummyError] dummy error\n");
	//clogger_logfWarning("[DummyWarning] dummy warning\n");

	//parseError(parser, "A function named %.*s already exists at %s:%s", length, start, file, line);

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

	MSVM* vm = (MSVM*)malloc(sizeof(MSVM));
	memset(vm, 0, sizeof(MSVM));

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

	MSConfiguration config;
	config.error_fn = errorPrint;
	config.write_fn = writeFunction;
	config.load_script_fn = loadScript;
	config.load_script_done_fn = loadScriptDone;
	vm->config = config;

	compileSource(vm, "../some/path/file.ms");

	return 0;
}
