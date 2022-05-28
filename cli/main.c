/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <pocketlang.h>
#include <stdio.h>

// FIXME: Everything below here is temporary and for testing.

#if defined(__GNUC__)
  #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
  #pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(__clang__)
  #pragma clang diagnostic ignored "-Wint-to-pointer-cast"
  #pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
  #pragma warning(disable:26812)
#endif

#define _ARGPARSE_IMPL
#include "argparse.h"
#undef _ARGPARSE_IMPL

// FIXME:
// Included for isatty(). This should be moved to somewhere. and I'm not sure
// about the portability of these headers. and isatty() function.
#ifdef _WIN32
  #include <Windows.h>
  #include <io.h>
  #define isatty _isatty
  #define fileno _fileno
#else
  #include <unistd.h>
#endif

#define CLI_NOTICE                                                            \
  "PocketLang " PK_VERSION_STRING " (https://github.com/ThakeeNathees/pocketlang/)\n" \
  "Copyright (c) 2020-2021 ThakeeNathees\n"                                   \
  "Copyright (c) 2021-2022 Pocketlang Contributors\n"                         \
  "Free and open source software under the terms of the MIT license.\n"

// Create new pocket VM and set it's configuration.
static PKVM* intializePocketVM() {

  PkConfiguration config = pkNewConfiguration();

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
    config.use_ansi_escape = true;
  }

  PKVM* vm = pkNewVM(&config);
  return vm;
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

  if (cmd != NULL) { // pocket -c "print('foo')"
    PkResult result = pkRunString(vm, cmd);
    exitcode = (int) result;

  } else if (argc == 0) { // Run on REPL mode.

   // Print the copyright and license notice, if --quiet not set.
    if (!quiet) {
      printf("%s", CLI_NOTICE);
    }

    exitcode = pkRunREPL(vm);

  } else { // pocket file.pk ...

    const char* file_path = argv[0];
    PkResult result = pkRunFile(vm, file_path);
    exitcode = (int) result;
  }

  // Cleanup the VM and exit.
  pkFreeVM(vm);
  return exitcode;
}
