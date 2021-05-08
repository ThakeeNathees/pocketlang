/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef BUFFERS_TEMPLATE_H
#define BUFFERS_TEMPLATE_H

#include "../common.h"
#include "miniscript.h"

#define M__NAME Uint
#define M__NAME_L uint
#define M__TYPE uint32_t
#include "buffer.h.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#define M__NAME Byte
#define M__NAME_L byte
#define M__TYPE uint8_t
#include "buffer.h.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#define M__NAME Var
#define M__NAME_L var
#define M__TYPE Var
#include "buffer.h.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#define M__NAME String
#define M__NAME_L string
#define M__TYPE String*
#include "buffer.h.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#define M__NAME Function
#define M__NAME_L function
#define M__TYPE Function*
#include "buffer.h.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#endif // BUFFERS_TEMPLATE_H