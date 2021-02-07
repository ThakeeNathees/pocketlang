/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

// Symbol table maps the names to it's member indecies in the VarBuffer.
#include "gen/string_buffer.h"

// TODO: Change this to use Map.
typedef StringBuffer NameTable;

// Initialize the symbol table.
void nameTableInit(NameTable* self);

// Remove the elements of the symbol table.
void nameTableClear(NameTable* self, VM* vm);

// Add a name to the name table and return the index of the name in the table.
int nameTableAdd(NameTable* self, VM* vm, const char* name, size_t length);

// Return name at index.
const char* nameTableGet(NameTable* self, int index);

#endif // SYMBOL_TABLE_H
