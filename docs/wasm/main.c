/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <emscripten.h>
#include <pocketlang.h>
#include <string.h>

// IO api functions.
extern void js_errorPrint(const char* message);
extern void js_writeFunction(const char* message);

void errorPrint(PKVM* vm, const char* message) {
  // No need to pass the file (since there is only script that'll ever run on
  // the browser.
  js_errorPrint(message);
}

void writeFunction(PKVM* vm, const char* text) {
  js_writeFunction(text);
}

EMSCRIPTEN_KEEPALIVE
int runSource(const char* source) {

  PkConfiguration config = pkNewConfiguration();
  config.stderr_write = errorPrint;
  config.stdout_write = writeFunction;
  config.load_script_fn = NULL;
  config.resolve_path_fn = NULL;

  PKVM* vm = pkNewVM(&config);

  // FIXME:
  // The bellow blocks of code can be simplified with
  //
  //    PkResult result = pkInterpretSource(vm, src, path, NULL);
  //
  // But the path is printed on the error messages as "@(TRY)"
  // If we create a module and compile it, then it won't have a
  // path and the error message is simplified. This behavior needs
  // to be changed in the error report function.

  PkResult result;

  PkHandle* module = pkNewModule(vm, "@(TRY)");
  do {
    PkStringPtr src = { .string = source };
    result = pkCompileModule(vm, module, src, NULL);
    if (result != PK_RESULT_SUCCESS) break;

    PkHandle* _main = pkModuleGetMainFunction(vm, module);
    result = pkRunFunction(vm, _main, 0, -1, -1);
    pkReleaseHandle(vm, _main);

  } while (false);
  pkReleaseHandle(vm, module);

  pkFreeVM(vm);

  return (int)result;
}
