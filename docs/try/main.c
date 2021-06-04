/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <emscripten.h>
#include <pocketlang.h>
#include <string.h>

// IO api functions.
extern void js_errorPrint(int type, int line, const char* message);
extern void js_writeFunction(const char* message);

void errorPrint(PKVM* vm, PkErrorType type, const char* file, int line,
                const char* message) {
  // No need to pass the file (since there is only script that'll ever run on
  // the browser.
  js_errorPrint((int)type, line, message);
}

void writeFunction(PKVM* vm, const char* text) {
  js_writeFunction(text);
}

PkStringPtr resolvePath(PKVM* vm, const char* from, const char* name) {
  PkStringPtr result;
  result.string = NULL;
  return result;
}

PkStringPtr loadScript(PKVM* vm, const char* path) {
  PkStringPtr result;
  result.string = NULL;
  return result;
}

EMSCRIPTEN_KEEPALIVE
int runSource(const char* source) {
  
  PkConfiguration config = pkNewConfiguration();
  config.error_fn = errorPrint;
  config.write_fn = writeFunction;
  config.load_script_fn = loadScript;
  config.resolve_path_fn = resolvePath;

  PKVM* vm = pkNewVM(&config);
  PkInterpretResult result = pkInterpretSource(vm, source, "@try");

  pkFreeVM(vm);

  return (int)result;
}
