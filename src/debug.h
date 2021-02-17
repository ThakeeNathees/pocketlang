/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef DEBUG_H
#define DEBUG_H

#include "common.h"
#include "miniscript.h"

// Dump the value of the [value] without a new line at the end.
void dumpValue(MSVM* vm, Var value);

// Dump opcodes of the given function.
void dumpInstructions(MSVM* vm, Function* func);

#endif // DEBUG_H
