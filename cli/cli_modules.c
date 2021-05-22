/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "pocketlang.h"

// FIXME: everything below here is temproary and for testing.

#include <stdio.h>

void stdPathAbspath(PKVM* vm) {
  PkVar path;
  if (!pkGetArgValue(vm, 1, PK_STRING, &path)) return;

  const char* data = pkStringGetData(path);

  pkReturnNull(vm);
}

void stdPathCurdir(PKVM* vm) {
  pkReturnNull(vm);
}

void testAdd(PKVM* vm) {
  double v1, v2;
  if (!pkGetArgNumber(vm, 1, &v1)) return;
  if (!pkGetArgNumber(vm, 2, &v2)) return;

  double total = v1 + v2;

  pkReturnNumber(vm, total);
}

void register_cli_modules(PKVM* vm) {
  PkHandle* path = pkNewModule(vm, "path");
  pkModuleAddFunction(vm, path, "abspath", stdPathAbspath, 1);
  pkModuleAddFunction(vm, path, "curdir", stdPathCurdir, 0);

  PkHandle* test = pkNewModule(vm, "test");
  pkModuleAddFunction(vm, test, "add", testAdd, 2);

}