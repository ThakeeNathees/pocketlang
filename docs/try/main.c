/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

// var rs = Module.cwrap('runSource', 'number', ['string'])

#include <string.h>

#include <emscripten.h>
#include "pocketlang.h"

extern void js_errorPrint(int type, int line, const char* message);
extern void js_writeFunction(const char* message);
extern const char* js_loadScript();

void errorPrint(PKVM* vm, PKErrorType type, const char* file, int line,
                const char* message) {
  // No need to pass file (since there is only script that'll ever run on the 
  // browser.
  js_errorPrint((int)type, line, message);
}

void writeFunction(PKVM* vm, const char* text) {
  js_writeFunction(text);
}

pkStringPtr resolvePath(PKVM* vm, const char* from, const char* name) {
  pkStringPtr result;
  result.string = NULL;
  return result;
}

pkStringPtr loadScript(PKVM* vm, const char* path) {
  pkStringPtr result;
  result.string = NULL;
  return result;
}

EMSCRIPTEN_KEEPALIVE
int runSource(const char* source) {
  
  pkConfiguration config;
  pkInitConfiguration(&config);
  config.error_fn = errorPrint;
  config.write_fn = writeFunction;
  config.load_script_fn = loadScript;
  config.resolve_path_fn = resolvePath;

  PKVM* vm = pkNewVM(&config);
  PKInterpretResult result = pkInterpretSource(vm, source, "@try");

  pkFreeVM(vm);

  return (int)result;
}
