/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_DEBUG_H
#define PK_DEBUG_H

#include "pk_internal.h"
#include "pk_value.h"

// Dump the value of the [value] without a new line at the end to the buffer
// [buff]. Note that this will not write a null byte at of the buffer
// (unlike dumpFunctionCode).
void dumpValue(PKVM* vm, Var value, pkByteBuffer* buff);

// Dump opcodes of the given function to the buffer [buff].
void dumpFunctionCode(PKVM* vm, Function* func, pkByteBuffer* buff);

// Dump the all the global values of the script.
void dumpGlobalValues(PKVM* vm);

// Dump the current (top most) stack call frame.
void dumpStackFrame(PKVM* vm);

#endif // PK_DEBUG_H
