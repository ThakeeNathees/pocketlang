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

// FIXME:
void onResultDone(PKVM* vm, pkStringResult result) {
  //   // The result.string is the allocated buffer and it has to be freed
  //   // manually since it wasn't allocated by the VM.
  //   free((void*)result.string);
}

// FIXME:
pkStringResult resolvePath(PKVM* vm, const char* from, const char* name) {
  if (from == NULL) {
    // TODO: name is the complete path.
  }

  pkStringResult result;
  result.success = true;
  result.on_done = onResultDone;
  result.string = name;
  return result;
}

pkStringResult loadScript(PKVM* vm, const char* path) {
  pkStringResult result;
  result.success = true;
  result.on_done = onResultDone;

  // Open the file.
  FILE* file = fopen(path, "r");
  if (file == NULL) {
    result.success = false;
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
