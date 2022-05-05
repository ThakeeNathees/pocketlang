/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include <pocketlang.h>

int main(int argc, char** argv) {

  // Create a new pocket VM.
  PKVM* vm = pkNewVM(NULL);
  
  // Run a string.
  pkRunString(vm, "print('hello world')");
  
  // TODO: move path module to src/ or write a simple path resolving
  // function to support pkRunFile() without requesting someone
  // to provide path resolving function.
  //
  // Run a script from file.
  //pkRunFile(vm, "script.pk");
  
  // Free the VM.
  pkFreeVM(vm);
  
  return 0;
}
