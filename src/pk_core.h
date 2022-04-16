/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_CORE_H
#define PK_CORE_H

#include "pk_internal.h"
#include "pk_value.h"

// Initialize core language, builtin function and core libs.
void initializeCore(PKVM* vm);

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

#endif // PK_CORE_H
