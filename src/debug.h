/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "common.h"
#include "var.h"

// Dump the value of the [value] without a new line at the end.
void dumpValue(PKVM* vm, Var value);

// Dump opcodes of the given function.
void dumpFunctionCode(PKVM* vm, Function* func);

// Dump the all the global values of the script.
void dumpGlobalValues(PKVM* vm);

// Dump the current (top most) stack call frame.
void dumpStackFrame(PKVM* vm);

#endif // DEBUG_H
