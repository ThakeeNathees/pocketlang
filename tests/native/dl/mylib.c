
#include <pocketlang.h>

PK_EXPORT void hello(PKVM* vm) {
  pkSetSlotString(vm, 0, "hello from dynamic lib.");
}

PK_EXPORT PkHandle* pkExportModule(PKVM* vm) {
  PkHandle* mylib = pkNewModule(vm, "mylib");
  
  pkModuleAddFunction(vm, mylib, "hello", hello, 0);

  return mylib;
}
