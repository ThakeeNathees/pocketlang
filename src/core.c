/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "core.h"

#include <math.h>
#include <time.h>

#include "vm.h"

typedef struct {
  const char* name; //< Name of the function.
  int length;       //< Length of the name.
  Function fn;      //< Native function pointer.
} _BuiltinFn;

// Count of builtin function +1 for termination.
#define BUILTIN_COUNT 50

// Convert number var as int32_t. Check if it's number before using it.
#define _AS_INTEGER(var) (int32_t)trunc(AS_NUM(var))

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

// Validators /////////////////////////////////////////////////////////////////

// Check if a numeric value bool/number and set [value].
static inline bool isNumeric(Var var, double* value) {
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

// Check if [var] is bool/number. If not set error and return false.
static bool validateNumeric(MSVM* vm, Var var, double* value,
  const char* name) {
  if (isNumeric(var, value)) return true;
  msSetRuntimeError(vm, "%s must be a numeric value.", name);
  return false;
}

// Check if [var] is integer. If not set error and return false.
static bool validateIngeger(MSVM* vm, Var var, int32_t* value,
  const char* name) {
  double number;
  if (isNumeric(var, &number)) {
    double truncated = trunc(number);
    if (truncated == number) {
      *value = (int32_t)(truncated);
      return true;
    }
  }

  msSetRuntimeError(vm, "%s must be an integer.", name);
  return false;
}

static bool validateIndex(MSVM* vm, int32_t index, int32_t size,
  const char* container) {
  if (index < 0 || size <= index) {
    msSetRuntimeError(vm, "%s index out of range.", container);
    return false;
  }
  return true;
}

// Builtin Functions //////////////////////////////////////////////////////////

// Argument getter (1 based).
#define ARG(n) vm->rbp[n]

// Argument count used in variadic functions.
#define ARGC ((int)(vm->sp - vm->rbp) - 1)

// Set return value.
#define RET(value) vm->rbp[0] = value

Function* getBuiltinFunction(int index) {
  ASSERT(index < BUILTIN_COUNT, "Index out of bound.");
  return &builtins[index].fn;
}

#define FN_IS_PRIMITE_TYPE(name, check)       \
  void coreIs##name(MSVM* vm) {               \
    RET(VAR_BOOL(check(ARG(1))));             \
  }

#define FN_IS_OBJ_TYPE(name, _enum)                     \
  void coreIs##name(MSVM* vm) {                         \
    Var arg1 = ARG(1);                                  \
    if (IS_OBJ(arg1) && AS_OBJ(arg1)->type == _enum) {  \
      RET(VAR_TRUE);                                    \
    } else {                                            \
      RET(VAR_FALSE);                                   \
    }                                                   \
  }

FN_IS_PRIMITE_TYPE(Null, IS_NULL)
FN_IS_PRIMITE_TYPE(Bool, IS_BOOL)
FN_IS_PRIMITE_TYPE(Num,  IS_NUM)

FN_IS_OBJ_TYPE(String, OBJ_STRING)
FN_IS_OBJ_TYPE(List,  OBJ_LIST)
FN_IS_OBJ_TYPE(Map,    OBJ_MAP)
FN_IS_OBJ_TYPE(Range,  OBJ_RANGE)
FN_IS_OBJ_TYPE(Function,  OBJ_FUNC)
FN_IS_OBJ_TYPE(Script,  OBJ_SCRIPT)
FN_IS_OBJ_TYPE(UserObj,  OBJ_USER)

void coreToString(MSVM* vm) {
  RET(VAR_OBJ(&toString(vm, ARG(1), false)->_super));
}

void corePrint(MSVM* vm) {
  String* str; //< Will be cleaned by garbage collector;

  for (int i = 1; i <= ARGC; i++) {
    Var arg = ARG(i);
    // If it's already a string don't allocate a new string instead use it.
    if (IS_OBJ(arg) && AS_OBJ(arg)->type == OBJ_STRING) {
      str = (String*)AS_OBJ(arg);
    } else {
      str = toString(vm, arg, false);
    }

    if (i != 1) vm->config.write_fn(vm, " ");
    vm->config.write_fn(vm, str->data);
  }

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

// TODO: Temp function to benchmark.
void coreClock(MSVM* vm) {
  RET(VAR_NUM((double)clock() / CLOCKS_PER_SEC));
}



void initializeCore(MSVM* vm) {

  int i = 0; //< Iterate through builtins.

  // Initialize builtin functions.
  initializeBuiltinFN(vm, &builtins[i++], "is_null",     1, coreIsNull);
  initializeBuiltinFN(vm, &builtins[i++], "is_bool",     1, coreIsBool);
  initializeBuiltinFN(vm, &builtins[i++], "is_num",      1, coreIsNum);

  initializeBuiltinFN(vm, &builtins[i++], "is_string",   1, coreIsString);
  initializeBuiltinFN(vm, &builtins[i++], "is_list",     1, coreIsList);
  initializeBuiltinFN(vm, &builtins[i++], "is_map",      1, coreIsMap);
  initializeBuiltinFN(vm, &builtins[i++], "is_range",    1, coreIsRange);
  initializeBuiltinFN(vm, &builtins[i++], "is_function", 1, coreIsFunction);
  initializeBuiltinFN(vm, &builtins[i++], "is_script",   1, coreIsScript);
  initializeBuiltinFN(vm, &builtins[i++], "is_userobj",  1, coreIsUserObj);

  initializeBuiltinFN(vm, &builtins[i++], "to_string",   1, coreToString);
  initializeBuiltinFN(vm, &builtins[i++], "print",      -1, corePrint);
  initializeBuiltinFN(vm, &builtins[i++], "import",      1, coreImport);

  // FIXME: move this. temp for benchmarking.
  initializeBuiltinFN(vm, &builtins[i++], "clock",       0, coreClock);

  // Sentinal to mark the end of the array.
  initializeBuiltinFN(vm, &builtins[i], NULL, 0, NULL);

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
  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(d1 - d2);
    }
    return VAR_NULL;
  }

  TODO; //string addition/ array addition etc.
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

bool varGreater(Var v1, Var v2) {
  
  double d1, d2;
  if (isNumeric(v1, &d1) && isNumeric(v2, &d2)) {
    return d1 > d2;
  }

  TODO;
  return false;
}

bool varLesser(Var v1, Var v2) {
  double d1, d2;
  if (isNumeric(v1, &d1) && isNumeric(v2, &d2)) {
    return d1 < d2;
  }

  TODO;
  return false;
}

bool varIterate(MSVM* vm, Var seq, Var* iterator, Var* value) {

#ifdef DEBUG
  int32_t _temp;
  ASSERT(IS_NUM(*iterator) || IS_NULL(*iterator), OOPS);
  if (IS_NUM(*iterator)) {
    ASSERT(validateIngeger(vm, *iterator, &_temp, "Assetion."), OOPS);
  }
#endif

  // Primitive types are not iterable.
  if (!IS_OBJ(seq)) {
    if (IS_NULL(seq)) {
      msSetRuntimeError(vm, "Null is not iterable.");
    } else if (IS_BOOL(seq)) {
      msSetRuntimeError(vm, "Boolenan is not iterable.");
    } else if (IS_NUM(seq)) {
      msSetRuntimeError(vm, "Number is not iterable.");
    } else {
      UNREACHABLE();
    }
    *value = VAR_NULL;
    return false;
  }

  Object* obj = AS_OBJ(seq);

  int32_t iter = 0; //< Nth iteration.
  if (IS_NUM(*iterator)) {
    iter = _AS_INTEGER(*iterator);
  }

  switch (obj->type) {
    case OBJ_STRING: {
      TODO; // Need to consider utf8.
      
      TODO; // Return string[index].
    }

    case OBJ_LIST: {
      VarBuffer* elems = &((List*)obj)->elements;
      if (iter < 0 || iter >= elems->count) {
        return false; //< Stop iteration.
      }
      *value = elems->data[iter];
      *iterator = VAR_NUM((double)iter + 1);
      return true;
    }

    case OBJ_MAP:
      TODO;

    case OBJ_RANGE: {
      double from = ((Range*)obj)->from;
      double to   = ((Range*)obj)->to;
      if (from == to) return false;

      double current;
      if (from <= to) { //< Straight range.
        current = from + (double)iter;
      } else {          //< Reversed range.
        current = from - (double)iter;
      }
      if (current == to) return false;
      *value = VAR_NUM(current);
      *iterator = VAR_NUM((double)iter + 1);
      return true;
    }

    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_USER:
      TODO;
      break;
    default:
      UNREACHABLE();
  }

  TODO;
  return false;
}