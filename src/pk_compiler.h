/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#ifndef COMPILER_H
#define COMPILER_H

#include "pk_common.h"
#include "pk_var.h"

typedef enum {
  #define OPCODE(name, _, __) OP_##name,
  #include "pk_opcodes.h"
  #undef OPCODE
} Opcode;

// Pocketlang compiler is a one pass/single pass compiler, which means it
// doesn't go through the basic compilation pipeline such as lexing, parsing
// (AST), analyzing, intermediate code generation, and target codegeneration
// one by one. Instead it'll generate the target code as it reads the source
// (directly from lexing to codegen). Despite it's faster than multipass
// compilers, we're restricted syntax-wise and from compile-time optimizations.
// Yet we support "forward names" to call functions before they defined
// (unlike C/Python).
typedef struct Compiler Compiler;

// This will take source code as a cstring, compiles it to pocketlang bytecodes
// and append them to the script's implicit main function ("$(SourceBody)").
// On a successfull compilation it'll return PK_RESULT_SUCCESS, otherwise it'll
// return PK_RESULT_COMPILE_ERROR but if repl_mode set in the [options],  and
// we've reached and unexpected EOF it'll return PK_RESULT_UNEXPECTED_EOF.
PkResult compile(PKVM* vm, Script* script, const char* source,
                 const PkCompileOptions* options);

// Mark the heap allocated objects of the compiler at the garbage collection
// called at the marking phase of vmCollectGarbage().
void compilerMarkObjects(PKVM* vm, Compiler* compiler);

#endif // COMPILER_H
