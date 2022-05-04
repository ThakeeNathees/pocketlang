/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

// This is a minimal example on how to pass values between pocket VM and C.

#include <pocketlang.h>

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
/* MAIN                                                                      */
/*****************************************************************************/

int main(int argc, char** argv) {

  // Create a new pocket VM.
  PKVM* vm = pkNewVM(NULL);
  
  // Registering a native module.
  PkHandle* my_module = pkNewModule(vm, "my_module");
  pkModuleAddFunction(vm, my_module, "cFunction", cFunction, 1);
  pkRegisterModule(vm, my_module);
  pkReleaseHandle(vm, my_module);
  
  // Run the code.
  PkResult result = pkRunString(vm, code);
  
  // Free the VM.
  pkFreeVM(vm);
  
  return (int) result;
}
