/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "core.h"
#include "vm.h"

typedef struct {
  const char* name; //< Name of the function.
  int length;       //< Length of the name.
  Function fn;      //< Native function pointer.
} _BuiltinFn;

// Count of builtin function +1 for termination.
#define BUILTIN_COUNT 10

// Array of all builtin functions.
_BuiltinFn builtins[BUILTIN_COUNT];

static void initializeBuiltinFN(MSVM* vm, _BuiltinFn* bfn, const char* name,
                                int arity, MiniScriptNativeFn ptr) {
  bfn->name = name;
  bfn->length = (name != NULL) ? (int)strlen(name) : 0;

  varInitObject(&bfn->fn._super, vm, OBJ_FUNC);
  bfn->fn.name = name;
  bfn->fn.arity = arity;
  bfn->fn.owner = NULL;
  bfn->fn.is_native = true;
  bfn->fn.native = ptr;
}

int findBuiltinFunction(const char* name, int length) {
  for (int i = 0; i < BUILTIN_COUNT; i++) {
    if (builtins[i].name == NULL) return -1;

    if (length == builtins[i].length && strncmp(name, builtins[i].name, length) == 0) {
      return i;
    }
  }
  return -1;
}

Function* getBuiltinFunction(int index) {
  ASSERT(index < BUILTIN_COUNT, "Index out of bound.");
  return &builtins[index].fn;
}

void coreToString(MSVM* vm) {
  Var arg1 = vm->rbp[1];
  vm->rbp[0] = VAR_OBJ(&toString(vm, arg1)->_super);
}

void corePrint(MSVM* vm) {
  Var arg1 = vm->rbp[1];
  String* str; //< Will be cleaned by garbage collector;

  // If it's already a string don't allocate a new string instead use it.
  if (IS_OBJ(arg1) && AS_OBJ(arg1)->type == OBJ_STRING) {
    str = (String*)AS_OBJ(arg1);
  } else {
    str = toString(vm, arg1);
  }
  vm->config.write_fn(vm, str->data);
  vm->config.write_fn(vm, "\n");
}

void initializeCore(MSVM* vm) {

  // Initialize builtin functions.
  int i = 0;
  initializeBuiltinFN(vm, &builtins[i++], "print",     1, corePrint);
  initializeBuiltinFN(vm, &builtins[i++], "to_string", 1, coreToString);

  // Sentinal to mark the end of the array.
  initializeBuiltinFN(vm, &builtins[i], NULL, 0, NULL);

}

// Validators /////////////////////////////////////////////////////////////////

// Check if a numeric value bool/number and set [value].
bool isNumeric(Var var, double* value) {
  if (IS_BOOL(var)) {
    *value = AS_BOOL(var);
    return true;
  }
  if (IS_NUM(var)) {
    *value = AS_NUM(var);
    return true;
  }
  return false;
}

// Check if [var] is bool/number. if not set error and return false.
bool validateNumeric(MSVM* vm, Var var, double* value, const char* arg) {
  if (isNumeric(var, value)) return true;
  msSetRuntimeError(vm, "%s must be a numeric value.", arg);
  return false;
}

// Operators //////////////////////////////////////////////////////////////////

Var varAdd(MSVM* vm, Var v1, Var v2) {

  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(d1 + d2);
    }
    return VAR_NULL;
  }

  // TODO: string addition/ array addition etc.
  return VAR_NULL;
}

Var varSubtract(MSVM* vm, Var v1, Var v2) {
  return VAR_NULL;
}

Var varMultiply(MSVM* vm, Var v1, Var v2) {

  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(d1 * d2);
    }
    return VAR_NULL;
  }

  return VAR_NULL;
}

Var varDivide(MSVM* vm, Var v1, Var v2) {
  return VAR_NULL;
}