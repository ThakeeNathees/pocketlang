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

static const char* opnames[] = {
	#define OPCODE(name, params, stack) #name,
	#include "../src/opcodes.h"
	#undef OPCODE
	NULL,
};

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
		"1 + 2 * 3\n"
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

	//FILE* fp = fopen("test.ms", "r");
	//if (fp != NULL) {
	//	char buff[1024];
	//	size_t read = fread(buff, 1, sizeof(buff), fp);
	//	buff[read] = '\0';
	//	printf("%s\n", buff);
	//	fclose(fp);
	//} else {
	//	clogger_logfError("[Error] cannot open file test.ms\n");
	//}

	MSConfiguration config;
	config.error_fn = errorPrint;
	config.write_fn = writeFunction;
	config.load_script_fn = loadScript;
	config.load_script_done_fn = loadScriptDone;

	MSVM* vm = (MSVM*)malloc(sizeof(MSVM));
	vmInit(vm, &config);

	Script* script = compileSource(vm, "../some/path/file.ms");
	vmRunScript(vm, script);

	ByteBuffer* bytes = &script->body->fn->opcodes;
	for (int i = 0; i < bytes->count; i++) {
		const char* op = "(???)";
		if (bytes->data[i] <= (int)OP_END) {
			op = opnames[bytes->data[i]];
		}
		printf("%s    %i\n", op, bytes->data[i]);
	}

	return 0;
}
