/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef COMPILER_H
#define COMPILER_H

#include "common.h"
#include "var.h"

typedef enum {
  #define OPCODE(name, _, __) OP_##name,
  #include "opcodes.h"
  #undef OPCODE
} Opcode;

// Pocketlang compiler is a single pass compiler, which means it doesn't go
// throught the basic compilation pipline such as lexing, parsing (AST),
// analyzing, intermediate codegeneration, and target codegeneration one by one
// instead it'll generate the target code as it reads the source (directly from
// lexing to codegen). Despite it's faster than multipass compilers, we're
// restricted syntax-wise and from compiletime optimizations, yet we support
// "forward names" to call functions before they defined (unlike C/Python).
typedef struct Compiler Compiler;

// This will take source code as a cstring, compiles it to pocketlang bytecodes
// and append them to the script's implicit main function ("$(SourceBody)").
bool compile(PKVM* vm, Script* script, const char* source,
             const PkCompileOptions* options);

// Mark the heap allocated ojbects of the compiler at the garbage collection
// called at the marking phase of vmCollectGarbage().
void compilerMarkObjects(PKVM* vm, Compiler* compiler);

#endif // COMPILER_H
