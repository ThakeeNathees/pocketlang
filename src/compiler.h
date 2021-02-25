/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef COMPILER_H
#define COMPILER_H

#include "common.h"
#include "var.h"

typedef struct Compiler Compiler;

Script* compileSource(MSVM* vm, const char* path);

#endif // COMPILER_H
