/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef COMPILER_H
#define COMPILER_H

#include "common.h"
#include "var.h"

typedef struct Compiler Compiler;

bool compile(PKVM* vm, Script* script, const char* source);

void compilerMarkObjects(Compiler* compiler, PKVM* vm);

#endif // COMPILER_H
