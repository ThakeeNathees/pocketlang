/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef CORE_H
#define CORE_H

#include "var.h"

Var varAdd(MSVM* vm, Var v1, Var v2);
Var varSubtract(MSVM* vm, Var v1, Var v2);
Var varMultiply(MSVM* vm, Var v1, Var v2);
Var varDivide(MSVM* vm, Var v1, Var v2);

#endif // CORE_H
