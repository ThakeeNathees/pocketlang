/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdio.h>

//#define CLOGGER_IMPLEMENT
//#include "clogger.h"

#include "miniscript.h"

#include <stdlib.h>

static const char* opnames[] = {
  #define OPCODE(name, params, stack) #name,
  #include "../src/opcodes.h"
  #undef OPCODE
  NULL,
};

void errorPrint(MSVM* vm, MSErrorType type, const char* file, int line,
                   const char* message) {
  fprintf(stderr, "Error: %s\n\tat %s:%i\n", message, file, line);
}

void writeFunction(MSVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

void loadScriptDone(MSVM* vm, const char* path, void* user_data) {
  free(user_data);
}

MSLoadScriptResult loadScript(MSVM* vm, const char* path) {
  MSLoadScriptResult result;
  result.is_failed = false;

  // Open the file.
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    result.is_failed = true;
    return result;
  }

  // Get the source length.
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Read source to buffer.
  char* buff = (char*)malloc(file_size + 1);
  size_t read = fread(buff, sizeof(char), file_size, file);
  // Using read instead of file_size is because "\r\n" is read as '\n' in
  // windows the '\r'.
  buff[read] = '\0';
  fclose(file);

  result.source = buff;
  result.user_data = (void*)buff;
  return result;
}

int main(int argc, char** argv) {

  MSConfiguration config;
  config.error_fn = errorPrint;
  config.write_fn = writeFunction;
  config.load_script_fn = loadScript;
  config.load_script_done_fn = loadScriptDone;

  MSVM* vm = msNewVM(&config);
  MSInterpretResult result = msInterpret(vm, "build/test.ms");

  //Script* script = compileSource(vm, "../some/path/file.ms");
  //vmRunScript(vm, script);
  //
  //ByteBuffer* bytes = &script->body->fn->opcodes;
  //for (int i = 0; i < bytes->count; i++) {
  //  const char* op = "(???)";
  //  if (bytes->data[i] <= (int)OP_END) {
  //    op = opnames[bytes->data[i]];
  //  }
  //  printf("%s    %i\n", op, bytes->data[i]);
  //}

  return 0;
}
