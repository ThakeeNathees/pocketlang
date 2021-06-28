/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "internal.h"
#include "modules.h"

#include "thirdparty/argparse/argparse.h"

// FIXME: Everything below here is temporary and for testing.

int repl(PKVM* vm, const PkCompileOptions* options);
const char* read_line(uint32_t* length);

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

// Create new pocket VM and set it's configuration.
static PKVM* intializePocketVM() {
  PkConfiguration config = pkNewConfiguration();
  config.error_fn = errorFunction;
  config.write_fn = writeFunction;
  config.read_fn = readFunction;

  config.inst_free_fn = freeObj;
  config.inst_name_fn = getObjName;
  config.inst_get_attrib_fn = objGetAttrib;

  config.load_script_fn = loadScript;
  config.resolve_path_fn = resolvePath;

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

  registerModules(vm);

  PkCompileOptions options = pkNewCompilerOptions();
  options.debug = debug;

  if (cmd != NULL) { // pocket -c "print('foo')"

    PkStringPtr source = { cmd, NULL, NULL, 0, 0 };
    PkStringPtr path = { "$(Source)", NULL, NULL, 0, 0 };
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
