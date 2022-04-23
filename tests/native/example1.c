/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

// This is a minimal example on how to pass values between pocket VM and C.

#include <pocketlang.h>
#include <stdio.h>

// The pocket script we're using to test.
static const char* code =
  "  from my_module import cFunction \n"
  "  a = 42                          \n"
  "  b = cFunction(a)                \n"
  "  print('[pocket] b = $b')        \n"
  ;

/*****************************************************************************/
/* MODULE FUNCTION                                                           */
/*****************************************************************************/

static void cFunction(PKVM* vm) {
  
  // Get the parameter from pocket VM.
  double a;
  if (!pkValidateSlotNumber(vm, 1, &a)) return;
  
  printf("[C] a = %f\n", a);
  
  // Return value to the pocket VM.
  pkSetSlotNumber(vm, 0, 3.14);
}

/*****************************************************************************/
/* POCKET VM CALLBACKS                                                       */
/*****************************************************************************/

static void stdoutCallback(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

/*****************************************************************************/
/* MAIN                                                                      */
/*****************************************************************************/

int main(int argc, char** argv) {

  // Pocket VM configuration.
  PkConfiguration config = pkNewConfiguration();
  config.stdout_write = stdoutCallback;

  // Create a new pocket VM.
  PKVM* vm = pkNewVM(&config);
  
  // Registering a native module.
  PkHandle* my_module = pkNewModule(vm, "my_module");
  pkModuleAddFunction(vm, my_module, "cFunction", cFunction, 1);
  pkRegisterModule(vm, my_module);
  pkReleaseHandle(vm, my_module);
  
  // The path and the source code.
  PkStringPtr source = { .string = code };
  PkStringPtr path = { .string = "./some/path/" };
  
  // Run the code.
  PkResult result = pkInterpretSource(vm, source, path, NULL/*options*/);
  
  // Free the VM.
  pkFreeVM(vm);
  
  return (int)result;
}
