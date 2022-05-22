/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_AMALGAMATED
#include "libs.h"
#endif

void registerModuleMath(PKVM* vm);
void registerModuleTypes(PKVM* vm);
void registerModuleTime(PKVM* vm);
void registerModuleIO(PKVM* vm);
void registerModulePath(PKVM* vm);
void registerModuleDummy(PKVM* vm);

void registerLibs(PKVM* vm) {
  registerModuleMath(vm);
  registerModuleTypes(vm);
  registerModuleTime(vm);
  registerModuleIO(vm);
  registerModulePath(vm);
  registerModuleDummy(vm);
}

void cleanupLibs(PKVM* vm) {

}
