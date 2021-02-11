/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef CORE_H
#define CORE_H

#include "var.h"
#include "miniscript.h"

void initializeCore(MSVM* vm);

// Find the builtin function name and returns it's index in the builtins array
// if not found returns -1.
int findBuiltinFunction(const char* name, int length);

Function* getBuiltinFunction(int index);

// Operators //////////////////////////////////////////////////////////////////

Var varAdd(MSVM* vm, Var v1, Var v2);
Var varSubtract(MSVM* vm, Var v1, Var v2);
Var varMultiply(MSVM* vm, Var v1, Var v2);
Var varDivide(MSVM* vm, Var v1, Var v2);


#endif // CORE_H
