/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdlib.h>
#include <stdio.h>

#include "pocketlang.h"

void errorPrint(PKVM* vm, PKErrorType type, const char* file, int line,
                const char* message) {
  if (type == PK_ERROR_COMPILE) {
    fprintf(stderr, "Error: %s\n       at %s:%i\n", message, file, line);
  } else if (type == PK_ERROR_RUNTIME) {
    fprintf(stderr, "Error: %s\n", message);
  } else if (type == PK_ERROR_STACKTRACE) {
    fprintf(stderr, "  %s() [%s:%i]\n", message, file, line);
  }
}

void writeFunction(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

void onResultDone(PKVM* vm, pkStringPtr result) {

  if ((bool)result.user_data) {
    free((void*)result.string);
  }
}

// FIXME:
pkStringPtr resolvePath(PKVM* vm, const char* from, const char* path) {
  if (from == NULL) {
    // TODO: name is the complete path.
  }

  pkStringPtr result;
  result.on_done = onResultDone;
  result.string = path;
  result.user_data = (void*)false;
  return result;
}

pkStringPtr loadScript(PKVM* vm, const char* path) {

  pkStringPtr result;
  result.on_done = onResultDone;

  // Open the file.
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    // Setting .string to NULL to indicate the failure of loading the script.
    result.string = NULL;
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

  result.string = buff;
  result.user_data = (void*)true;

  return result;
}

int main(int argc, char** argv) {

  const char* notice =
    "PocketLang " PK_VERSION_STRING " (https://github.com/ThakeeNathees/pocketlang/)\n"
    "Copyright(c) 2020 - 2021 ThakeeNathees.\n"
    "Free and open source software under the terms of the MIT license.\n";
  const char* help = "Usage: pocketlang <source_path>\n";

  if (argc < 2) {
    printf("%s\n%s", notice, help);
    return 0;
  }
  
  const char* source_path = argv[1];

  pkConfiguration config;
  pkInitConfiguration(&config);
  config.error_fn = errorPrint;
  config.write_fn = writeFunction;
  config.load_script_fn = loadScript;
  config.resolve_path_fn = resolvePath;

  PKVM* vm = pkNewVM(&config);
  PKInterpretResult result = pkInterpret(vm, source_path);
  pkFreeVM(vm);

  return result;
}
