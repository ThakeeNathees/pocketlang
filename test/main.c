/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdio.h>

#include "miniscript.h"

#include <stdlib.h>

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

  const char* notice =
    "MiniScript " MS_VERSION_STRING " (https://github.com/ThakeeNathees/miniscript/)\n"
    "Copyright(c) 2020 - 2021 ThakeeNathees.\n"
    "Free and open source software under the terms of the MIT license.\n";
  const char* help = "Usage: miniscript <source_path>\n";

  if (argc < 2) {
    printf("%s\n%s", notice, help);
    return 0;
  }

  const char* source_path = argv[1];

  MSConfiguration config;
  config.error_fn = errorPrint;
  config.write_fn = writeFunction;
  config.load_script_fn = loadScript;
  config.load_script_done_fn = loadScriptDone;

  MSVM* vm = msNewVM(&config);
  MSInterpretResult result = msInterpret(vm, source_path);

  return result;
}
