/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef MINISCRIPT_H
#define MINISCRIPT_H

#include <stdint.h>
#include <stdbool.h>

// The version number macros.
#define MS_VERSION_MAJOR 0
#define MS_VERSION_MINOR 1
#define MS_VERSION_PATCH 0

// String representation of the value.
#define MS_VERSION_STRING "0.1.0"

// MiniScript Virtual Machine.
// it'll contain the state of the execution, stack, heap, and manage memory
// allocations.
typedef struct VM VM;

// C function pointer which is callable from MiniScript.
typedef void (*MiniScriptNativeFn)(VM* vm);



#endif // MINISCRIPT_H
