/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "internal.h"
#include "modules/modules.h"
#include "thirdparty/argparse/argparse.h"

// FIXME: Everything below here is temporary and for testing.

int repl(PKVM* vm, const PkCompileOptions* options);

// ---------------------------------------

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

char* resolvePath_2rf_(PKVM* vm, const char* from, const char* path) {

  size_t from_dir_len;
  pathGetDirName(from, &from_dir_len);

  // FIXME:
  // Should handle paths with size of more than FILENAME_MAX incase caller
  // gave us an invalid path.

  if (from_dir_len == 0 || pathIsAbsolute(path)) {
    size_t path_size = strlen(path) + 1; // +1 for \0.

    char* resolved = pkAllocString(vm, path_size);
    pathNormalize(path, resolved, path_size);
    return resolved;

  } else {
    char from_dir[FILENAME_MAX];
    strncpy(from_dir, from, from_dir_len);
    from_dir[from_dir_len] = '\0';

    char fullpath[FILENAME_MAX];
    size_t path_size = pathJoin(from_dir, path, fullpath, sizeof(fullpath));
    path_size++; // +1 for \0.

    char* resolved = pkAllocString(vm, path_size);
    pathNormalize(fullpath, resolved, path_size);
    return resolved;
  }

  UNREACHABLE();
  return NULL;
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
  if (buff != NULL) {
    size_t read = fread(buff, sizeof(char), file_size, file);
    buff[read] = '\0';
  }

  fclose(file);

  result.string = buff;
  result.user_data = (void*)true;

  return result;
}

// FIXME:
// Included for isatty(). This should be moved to somewhere. and I'm not sure
// about the portability of these headers. and isatty() function.
#ifdef _WIN32
  #include <Windows.h>
  #include <io.h>
#else
  #include <unistd.h>
#endif

// Create new pocket VM and set it's configuration.
static PKVM* intializePocketVM() {

  PkConfiguration config = pkNewConfiguration();
  config.resolve_path_fn = resolvePath;

// FIXME:
// Refactor and make it portable. Maybe custom is_tty() function?.
// Windows isatty depricated -- use _isatty.
  if (!!isatty(fileno(stderr))) {
#ifdef _WIN32
    DWORD outmode = 0;
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    GetConsoleMode(handle, &outmode);
    SetConsoleMode(handle, outmode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    config.use_ansi_color = true;
  }

  return pkNewVM(&config);
}

int main(int argc, const char** argv) {

  // Parse command line arguments.

  const char* usage[] = {
    "pocket ... [-c cmd | file] ...",
    NULL,
  };

  const char* cmd = NULL;
  int debug = false, help = false, quiet = false, version = false;
  struct argparse_option cli_opts[] = {
      OPT_STRING('c', "cmd", (void*)&cmd,
        "Evaluate and run the passed string.", NULL, 0, 0),

      OPT_BOOLEAN('d', "debug", (void*)&debug,
        "Compile and run the debug version.", NULL, 0, 0),

      OPT_BOOLEAN('h', "help",  (void*)&help,
        "Prints this help message and exit.", NULL, 0, 0),

      OPT_BOOLEAN('q', "quiet", (void*)&quiet,
        "Don't print version and copyright statement on REPL startup.",
        NULL, 0, 0),

      OPT_BOOLEAN('v', "version", &version,
        "Prints the pocketlang version and exit.", NULL, 0, 0),
      OPT_END(),
  };

  // Parse the options.
  struct argparse argparse;
  argparse_init(&argparse, cli_opts, usage, 0);
  argc = argparse_parse(&argparse, argc, argv);

  if (help) { // pocket --help.
    argparse_usage(&argparse);
    return 0;
  }

  if (version) { // pocket --version
    fprintf(stdout, "pocketlang %s\n", PK_VERSION_STRING);
    return 0;
  }

  int exitcode = 0;

  // Create and initialize pocket VM.
  PKVM* vm = intializePocketVM();
  VmUserData user_data;
  user_data.repl_mode = false;
  pkSetUserData(vm, &user_data);

  REGISTER_ALL_MODULES(vm);

  PkCompileOptions options = pkNewCompilerOptions();
  options.debug = debug;

  if (cmd != NULL) { // pocket -c "print('foo')"

    PkStringPtr source = { cmd, NULL, NULL, 0, 0 };
    PkStringPtr path = { "@(Source)", NULL, NULL, 0, 0 };
    PkResult result = pkInterpretSource(vm, source, path, NULL);
    exitcode = (int)result;

  } else if (argc == 0) { // Run on REPL mode.

   // Print the copyright and license notice, if --quiet not set.
    if (!quiet) {
      printf("%s", CLI_NOTICE);
    }

    options.repl_mode = true;
    exitcode = repl(vm, &options);

  } else { // pocket file.pk ...

    PkStringPtr resolved = resolvePath(vm, ".", argv[0]);
    PkStringPtr source = loadScript(vm, resolved.string);

    if (source.string != NULL) {
      PkResult result = pkInterpretSource(vm, source, resolved, &options);
      exitcode = (int)result;
    } else {
      fprintf(stderr, "Error: cannot open file at \"%s\"\n", resolved.string);
      if (resolved.on_done != NULL) resolved.on_done(vm, resolved);
      if (source.on_done != NULL) source.on_done(vm, source);
    }
  }

  // Cleanup the VM and exit.
  pkFreeVM(vm);
  return exitcode;
}
