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
  PkResult result = pkRunString(vm, source);
  pkFreeVM(vm);

  return (int)result;
}
