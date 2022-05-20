/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_AMALGAMATED
#include "libs.h"
#endif

void registerModuleDummy(PKVM* vm);
void registerModuleIO(PKVM* vm);
void registerModulePath(PKVM* vm);
void registerModuleMath(PKVM* vm);

void pkRegisterLibs(PKVM* vm) {
  registerModuleDummy(vm);
  registerModuleIO(vm);
  registerModulePath(vm);
  registerModuleMath(vm);
}
