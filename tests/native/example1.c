/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

// This is a minimal example on how to pass values between pocket VM and C.

#include <pocketlang.h>
#include <stdio.h>

// The pocket script we're using to test.
static const char* code =
  "  from YourModule import variableToC \n"
  "  a = 42                             \n"
  "  b = variableToC(a)                 \n"
  "  print('[pocket] b =', b)           \n"
  ;

/*****************************************************************************/
/* MODULE FUNCTION                                                           */
/*****************************************************************************/

static void variableToC(PKVM* vm) {
  
  // Get the parameter from pocket VM.
  double a;
  if (!pkGetArgNumber(vm, 1, &a)) return;
  
  printf("[C] a = %f\n", a);
  
  // Return value to the pocket VM.
  pkReturnNumber(vm, 3.14);
}

/*****************************************************************************/
/* POCKET VM CALLBACKS                                                       */
/*****************************************************************************/

// Error report callback.
static void reportError(PKVM* vm, PkErrorType type,
                        const char* file, int line,
                        const char* message) {
  fprintf(stderr, "Error: %s\n", message);
}

// print() callback to write stdout.
static void stdoutWrite(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

/*****************************************************************************/
/* MAIN                                                                      */
/*****************************************************************************/

int main(int argc, char** argv) {

  // Pocket VM configuration.
  PkConfiguration config = pkNewConfiguration();
  config.error_fn  = reportError;
  config.write_fn  = stdoutWrite;
  //config.read_fn = stdinRead;

  // Create a new pocket VM.
  PKVM* vm = pkNewVM(&config);
  
  // Register your module.
  PkHandle* your_module = pkNewModule(vm, "YourModule");
  pkModuleAddFunction(vm, your_module, "variableToC",  variableToC, 1);
  pkReleaseHandle(vm, your_module);
  
  // The path and the source code.
  PkStringPtr source = { code, NULL, NULL, 0, 0 };
  PkStringPtr path = { "./some/path/", NULL, NULL, 0, 0 };
  
  // Run the code.
  PkResult result = pkInterpretSource(vm, source, path, NULL/*options*/);
  
  // Free the VM.
  pkFreeVM(vm);
  
  return (int)result;
}
