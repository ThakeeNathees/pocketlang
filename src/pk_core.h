/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#ifndef CORE_H
#define CORE_H

#include "pk_internal.h"
#include "pk_var.h"

// Initialize core language, builtin function and core libs.
void initializeCore(PKVM* vm);

// Find the builtin function name and returns it's index in the builtins array
// if not found returns -1.
int findBuiltinFunction(const PKVM* vm, const char* name, uint32_t length);

// Returns the builtin function at index [index].
Function* getBuiltinFunction(const PKVM* vm, int index);

// Returns the builtin function's name at index [index].
const char* getBuiltinFunctionName(const PKVM* vm, int index);

// Return the core library with the [name] if exists in the core libs,
// otherwise returns NULL.
Script* getCoreLib(const PKVM* vm, String* name);

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

Var varAdd(PKVM* vm, Var v1, Var v2);         // Returns v1 + v2.
Var varSubtract(PKVM* vm, Var v1, Var v2);    // Returns v1 - v2.
Var varMultiply(PKVM* vm, Var v1, Var v2);    // Returns v1 * v2.
Var varDivide(PKVM* vm, Var v1, Var v2);      // Returns v1 / v2.
Var varModulo(PKVM* vm, Var v1, Var v2);      // Returns v1 % v2.

Var varBitAnd(PKVM* vm, Var v1, Var v2);      // Returns v1 & v2.
Var varBitOr(PKVM* vm, Var v1, Var v2);       // Returns v1 | v2.
Var varBitXor(PKVM* vm, Var v1, Var v2);      // Returns v1 ^ v2.
Var varBitLshift(PKVM* vm, Var v1, Var v2);   // Returns v1 << v2.
Var varBitRshift(PKVM* vm, Var v1, Var v2);   // Returns v1 >> v2.
Var varBitNot(PKVM* vm, Var v);               // Returns ~v.

bool varGreater(Var v1, Var v2); // Returns v1 > v2.
bool varLesser(Var v1, Var v2);  // Returns v1 < v2.

// Returns [elem] in [container].
bool varContains(PKVM* vm, Var elem, Var container);

// Returns the attribute named [attrib] on the variable [on].
Var varGetAttrib(PKVM* vm, Var on, String* attrib);

// Set the attribute named [attrib] on the variable [on] with the given
// [value].
void varSetAttrib(PKVM* vm, Var on, String* name, Var value);

// Returns the subscript value (ie. on[key]).
Var varGetSubscript(PKVM* vm, Var on, Var key);

// Set subscript [value] with the [key] (ie. on[key] = value).
void varsetSubscript(PKVM* vm, Var on, Var key, Var value);

#endif // CORE_H
