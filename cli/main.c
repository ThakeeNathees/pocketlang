/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <pocketlang.h>

 // FIXME: everything below here is temproary and for testing.

void registerModules(PKVM* vm);

// Path public functions (TODO: should I add a header for that?)
void pathInit();
bool pathIsAbsolute(const char* path);
void pathGetDirName(const char* path, size_t* length);
size_t pathNormalize(const char* path, char* buff, size_t buff_size);
size_t pathJoin(const char* from, const char* path, char* buffer,
                size_t buff_size);

// ---------------------------------------

void errorPrint(PKVM* vm, PkErrorType type, const char* file, int line,
  const char* message) {
  if (type == PK_ERROR_COMPILE) {
    fprintf(stderr, "Error: %s\n       at \"%s\":%i\n", message, file, line);

  } else if (type == PK_ERROR_RUNTIME) {
    fprintf(stderr, "Error: %s\n", message);

  } else if (type == PK_ERROR_STACKTRACE) {
    fprintf(stderr, "  %s() [\"%s\":%i]\n", message, file, line);
  }
}

void writeFunction(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

void onResultDone(PKVM* vm, PkStringPtr result) {

  if ((bool)result.user_data) {
    free((void*)result.string);
  }
}

PkStringPtr resolvePath(PKVM* vm, const char* from, const char* path) {
  PkStringPtr result;
  result.on_done = onResultDone;

  size_t from_dir_len;
  pathGetDirName(from, &from_dir_len);

  // FIXME: Should handle paths with size of more than FILENAME_MAX.

  if (from_dir_len == 0 || pathIsAbsolute(path)) {
    size_t length = strlen(path);

    char* resolved = (char*)malloc(length + 1);
    pathNormalize(path, resolved, length + 1);

    result.string = resolved;
    result.user_data = (void*)true;

  } else {
    char from_dir[FILENAME_MAX];
    strncpy(from_dir, from, from_dir_len);
    from_dir[from_dir_len] = '\0';

    char fullpath[FILENAME_MAX];
    size_t length = pathJoin(from_dir, path, fullpath, sizeof(fullpath));

    char* resolved = (char*)malloc(length + 1);
    pathNormalize(fullpath, resolved, length + 1);

    result.string = resolved;
    result.user_data = (void*)true;
  }

  return result;
}

PkStringPtr loadScript(PKVM* vm, const char* path) {

  PkStringPtr result;
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
  char* buff = (char*)malloc((size_t)(file_size) + 1);
  // Using read instead of file_size is because "\r\n" is read as '\n' in
  // windows.
  size_t read = fread(buff, sizeof(char), file_size, file);
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
  const char* help = "usage: pocket [-c cmd | file]\n";


  // TODO: implement arg parse, REPL.

  if (argc < 2) {
    printf("%s\n%s", notice, help);
    return 0;
  }

  // Initialize cli.
  pathInit();

  PkConfiguration config = pkNewConfiguration();
  config.error_fn = errorPrint;
  config.write_fn = writeFunction;
  config.load_script_fn = loadScript;
  config.resolve_path_fn = resolvePath;

  PKVM* vm = pkNewVM(&config);
  registerModules(vm);
  PkInterpretResult result;

  // FIXME: this is temp till arg parse implemented.
  if (argc >= 3 && strcmp(argv[1], "-c") == 0) {

    PkStringPtr source = { argv[2], NULL, NULL };
    PkStringPtr path = { "$(Source)", NULL, NULL };

    result = pkInterpretSource(vm, source, path);
    pkFreeVM(vm);
    return result;
  }

  PkStringPtr resolved = resolvePath(vm, ".", argv[1]);
  PkStringPtr source = loadScript(vm, resolved.string);

  if (source.string != NULL) {
    result = pkInterpretSource(vm, source, resolved);

  } else {
    result = PK_RESULT_COMPILE_ERROR;
    fprintf(stderr, "Error: cannot open file at \"%s\"\n", resolved.string);
    if (resolved.on_done != NULL) resolved.on_done(vm, resolved);
    if (source.on_done != NULL) source.on_done(vm, source);
  }

  pkFreeVM(vm);
  return result;
}
