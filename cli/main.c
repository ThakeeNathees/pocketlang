/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "common.h"

// FIXME: Everything below here is temporary and for testing.

void repl(PKVM* vm, const PkCompileOptions* options);
const char* read_line(uint32_t* length);

void registerModules(PKVM* vm);

// Path public functions (TODO: should I add a header for that?)
void pathInit(void);
bool pathIsAbsolute(const char* path);
void pathGetDirName(const char* path, size_t* length);
size_t pathNormalize(const char* path, char* buff, size_t buff_size);
size_t pathJoin(const char* from, const char* path, char* buffer,
                size_t buff_size);

// ---------------------------------------

void onResultDone(PKVM* vm, PkStringPtr result) {
  if ((bool)result.user_data) {
    free((void*)result.string);
  }
}

void errorFunction(PKVM* vm, PkErrorType type, const char* file, int line,
                   const char* message) {

  VmUserData* ud = (VmUserData*)pkGetUserData(vm);
  bool repl = (ud) ? ud->repl_mode : false;

  if (type == PK_ERROR_COMPILE) {

    if (repl) fprintf(stderr, "Error: %s\n", message);
    else fprintf(stderr, "Error: %s\n       at \"%s\":%i\n",
                 message, file, line);

  } else if (type == PK_ERROR_RUNTIME) {
    fprintf(stderr, "Error: %s\n", message);

  } else if (type == PK_ERROR_STACKTRACE) {
    if (repl) fprintf(stderr, "  %s() [line:%i]\n", message, line);
    else fprintf(stderr, "  %s() [\"%s\":%i]\n", message, file, line);
  }
}

void writeFunction(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

PkStringPtr readFunction(PKVM* vm) {
  PkStringPtr result;
  result.string = read_line(NULL);
  result.on_done = onResultDone;
  result.user_data = (void*)true;
  return result;
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

  const char* usage =
    "usage: pocket [-c cmd | file]\n\n"
    "The following options are available:\n\n"
    "  -c cmd | file    executes a command or a file\n"
    "  --version        prints the version of pocketlang\n"
    "  --help           prints this help message\n";

  if (argc > 1) {
    if (strcmp(argv[1], "--help") == 0) {
      printf("%s", usage);
      return 0;
    } else if (strcmp(argv[1], "--version") == 0) {
      printf("Version: %s\n", PK_VERSION_STRING);
      return 0;
    } else {
      printf("Invalid option flag. Use --help for more.\n");
      return 0;
    }
  }

  // Initialize cli.
  pathInit();

  PkConfiguration config = pkNewConfiguration();
  config.error_fn = errorFunction;
  config.write_fn = writeFunction;
  config.read_fn = readFunction;
  config.load_script_fn = loadScript;
  config.resolve_path_fn = resolvePath;

  PkCompileOptions options = pkNewCompilerOptions();
  options.debug = true; // TODO: update this with cli args. (tco disabled).

  PKVM* vm = pkNewVM(&config);

  VmUserData user_data;
  user_data.repl_mode = false;
  pkSetUserData(vm, &user_data);

  registerModules(vm);

  // FIXME: this is temp till arg parse implemented.
  PkResult result;

  if (argc == 1) {
    options.repl_mode = true;
    repl(vm, &options);

  } if (argc >= 3 && strcmp(argv[1], "-c") == 0) {

    PkStringPtr source = { argv[2], NULL, NULL };
    PkStringPtr path = { "$(Source)", NULL, NULL };

    result = pkInterpretSource(vm, source, path, NULL);
    pkFreeVM(vm);
    return result;
  }

  PkStringPtr resolved = resolvePath(vm, ".", argv[1]);
  PkStringPtr source = loadScript(vm, resolved.string);

  if (source.string != NULL) {
    result = pkInterpretSource(vm, source, resolved, &options);

  } else {
    result = PK_RESULT_COMPILE_ERROR;
    fprintf(stderr, "Error: cannot open file at \"%s\"\n", resolved.string);
    if (resolved.on_done != NULL) resolved.on_done(vm, resolved);
    if (source.on_done != NULL) source.on_done(vm, source);
  }

  pkFreeVM(vm);
  return result;
}
