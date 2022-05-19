/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_DEBUG_H
#define PK_DEBUG_H

#ifndef PK_AMALGAMATED
#include "internal.h"
#include "value.h"
#endif

// Pretty print compile time error.
void reportCompileTimeError(PKVM* vm, const char* path, int line,
                            const char* source, const char* at, int length,
                            const char* fmt, va_list args);

// Pretty print runtime error.
void reportRuntimeError(PKVM* vm, Fiber* fiber);

// Dump opcodes of the given function to the stdout.
void dumpFunctionCode(PKVM* vm, Function* func);

// Dump the all the global values of the script to the stdout.
void dumpGlobalValues(PKVM* vm);

// Dump the current (top most) stack call frame to the stdout.
void dumpStackFrame(PKVM* vm);

#endif // PK_DEBUG_H
