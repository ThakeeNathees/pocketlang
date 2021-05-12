/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

// var rs = Module.cwrap('runSource', 'number', ['string'])

#include <string.h>

#include <emscripten.h>
#include "pocketlang.h"

extern void js_errorPrint(const char* message, int line);
extern void js_writeFunction(const char* message);
extern const char* js_loadScript();

void errorPrint(PKVM* vm, PKErrorType type, const char* file, int line,
                const char* message) {
  js_errorPrint(message, line);
}

void writeFunction(PKVM* vm, const char* text) {
  js_writeFunction(text);
}

pkStringResult resolvePath(PKVM* vm, const char* from, const char* name) {
  pkStringResult result;
  result.success = false;
  return result;
}

pkStringResult loadScript(PKVM* vm, const char* path) {
  pkStringResult result;
  result.success = false;
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
  PKInterpretResult result = pkInterpretSource(vm, source);
  
  // Fix Core multiple initialization and gc sweep()
  // to fix this.
  //pkFreeVM(vm);

  return (int)result;
}
