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
  
  // Run from file.
  pkRunFile(vm, "script.pk");
  
  // Free the VM.
  pkFreeVM(vm);
  
  return 0;
}
