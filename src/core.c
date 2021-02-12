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
#define BUILTIN_COUNT 50

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

#define FN_IS_PRIMITE_TYPE(name, check)       \
  void coreIs##name(MSVM* vm) {               \
    vm->rbp[0] = VAR_BOOL(check(vm->rbp[1])); \
  }

#define FN_IS_OBJ_TYPE(name, _enum)                     \
  void coreIs##name(MSVM* vm) {                         \
    Var arg1 = vm->rbp[1];                              \
    if (IS_OBJ(arg1) && AS_OBJ(arg1)->type == _enum) {  \
      vm->rbp[0] = VAR_TRUE;                            \
    } else {                                            \
      vm->rbp[0] = VAR_FALSE;                           \
    }                                                   \
  }

FN_IS_PRIMITE_TYPE(Null, IS_NULL)
FN_IS_PRIMITE_TYPE(Bool, IS_BOOL)
FN_IS_PRIMITE_TYPE(Num,  IS_NUM)

FN_IS_OBJ_TYPE(String, OBJ_STRING)
FN_IS_OBJ_TYPE(Array,  OBJ_ARRAY)
FN_IS_OBJ_TYPE(Map,    OBJ_MAP)
FN_IS_OBJ_TYPE(Range,  OBJ_RANGE)
FN_IS_OBJ_TYPE(Function,  OBJ_FUNC)
FN_IS_OBJ_TYPE(Script,  OBJ_SCRIPT)
FN_IS_OBJ_TYPE(UserObj,  OBJ_USER)

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

void coreImport(MSVM* vm) {
  Var arg1 = vm->rbp[1];
  if (IS_OBJ(arg1) && AS_OBJ(arg1)->type == OBJ_STRING) {
    TODO;
  } else {
    msSetRuntimeError(vm, "Expected a String argument.");
  }
}

void initializeCore(MSVM* vm) {

  int i = 0; //< Iterate through builtins.

  // Initialize builtin functions.
  initializeBuiltinFN(vm, &builtins[i++], "is_null", 1, coreIsNull);
  initializeBuiltinFN(vm, &builtins[i++], "is_bool", 1, coreIsBool);
  initializeBuiltinFN(vm, &builtins[i++], "is_num",  1, coreIsNum);

  initializeBuiltinFN(vm, &builtins[i++], "is_string",   1, coreIsString);
  initializeBuiltinFN(vm, &builtins[i++], "is_array",    1, coreIsArray);
  initializeBuiltinFN(vm, &builtins[i++], "is_map",      1, coreIsMap);
  initializeBuiltinFN(vm, &builtins[i++], "is_range",    1, coreIsRange);
  initializeBuiltinFN(vm, &builtins[i++], "is_function", 1, coreIsFunction);
  initializeBuiltinFN(vm, &builtins[i++], "is_script",   1, coreIsScript);
  initializeBuiltinFN(vm, &builtins[i++], "is_userobj",  1, coreIsUserObj);

  initializeBuiltinFN(vm, &builtins[i++], "to_string", 1, coreToString);
  initializeBuiltinFN(vm, &builtins[i++], "print",     1, corePrint);
  initializeBuiltinFN(vm, &builtins[i++], "import",    1, coreImport);

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

  TODO; //string addition/ array addition etc.
  return VAR_NULL;
}

Var varSubtract(MSVM* vm, Var v1, Var v2) {
  TODO;
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

  TODO;
  return VAR_NULL;
}

Var varDivide(MSVM* vm, Var v1, Var v2) {
  TODO;
  return VAR_NULL;
}

bool varIterate(MSVM* vm, Var seq, Var* iterator, Var* value) {
  TODO;
  return false;
}