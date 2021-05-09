/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "buffers.h"

#include "../utils.h"
#include "../vm.h"

#define M__HEADER "gen/uint_buffer.h"
#define M__NAME Uint
#define M__NAME_L uint
#define M__TYPE uint32_t
#include "buffer.c.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#define M__HEADER "gen/byte_buffer.h"
#define M__NAME Byte
#define M__NAME_L byte
#define M__TYPE uint8_t
#include "buffer.c.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#define M__HEADER "gen/var_buffer.h"
#define M__NAME Var
#define M__NAME_L var
#define M__TYPE Var
#include "buffer.c.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#define M__HEADER "gen/string_buffer.h"
#define M__NAME String
#define M__NAME_L string
#define M__TYPE String*
#include "buffer.c.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE

#define M__HEADER "gen/function_buffer.h"
#define M__NAME Function
#define M__NAME_L function
#define M__TYPE Function*
#include "buffer.c.template"
#undef M__HEADER
#undef M__NAME
#undef M__NAME_L
#undef M__TYPE
