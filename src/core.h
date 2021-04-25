/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef CORE_H
#define CORE_H

#include "var.h"
#include "common.h"

// Initialize core language, builtin function and "std" scripts.
// Note (TODO: refactore required):
//    Since the builtin function doesn't require any allocation they're
//    elements of a static `builtins` array but the "std" scripts are `Script`
//    objects they required memory management and they're bound with the VM.
//    It contradicts `initializeCore()` to be called for each VM or only once.
//    1. Make the scripts share between VMs.
//    2. Destroy scripts buffer only when the last VM die.
void initializeCore(MSVM* vm);

// Find the builtin function name and returns it's index in the builtins array
// if not found returns -1.
int findBuiltinFunction(const char* name, int length);

// Returns the builtin function at index [index].
Function* getBuiltinFunction(int index);

// Returns the builtin function's name at index [index].
const char* getBuiltinFunctionName(int index);

// Operators //////////////////////////////////////////////////////////////////

Var varAdd(MSVM* vm, Var v1, Var v2);
Var varSubtract(MSVM* vm, Var v1, Var v2);
Var varMultiply(MSVM* vm, Var v1, Var v2);
Var varDivide(MSVM* vm, Var v1, Var v2);

bool varGreater(MSVM* vm, Var v1, Var v2);
bool varLesser(MSVM* vm, Var v1, Var v2);

Var varGetAttrib(MSVM* vm, Var on, String* attrib);
void varSetAttrib(MSVM* vm, Var on, String* name, Var value);

Var varGetSubscript(MSVM* vm, Var on, Var key);
void varsetSubscript(MSVM* vm, Var on, Var key, Var value);

// Functions //////////////////////////////////////////////////////////////////

// Parameter [iterator] should be VAR_NULL before starting the iteration.
// If an element is obtained by iteration it'll return true otherwise returns
// false indicating that the iteration is over.
bool varIterate(MSVM* vm, Var seq, Var* iterator, Var* value);

#endif // CORE_H
