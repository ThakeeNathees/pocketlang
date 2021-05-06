/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "core.h"

#include <math.h>
#include <time.h>

#include "var.h"
#include "vm.h"

typedef struct {
  const char* name; //< Name of the function.
  int length;       //< Length of the name.
  Function* fn;     //< Native function pointer.
} _BuiltinFn;

// Count of builtin function +1 for termination.
#define BUILTIN_COUNT 50

// Convert number var as int32_t. Check if it's number before using it.
#define _AS_INTEGER(var) (int32_t)trunc(AS_NUM(var))

// Array of all builtin functions.
static _BuiltinFn builtins[BUILTIN_COUNT];
static int builtins_count = 0;

static void initializeBuiltinFN(MSVM* vm, _BuiltinFn* bfn, const char* name,
                               int length, int arity, MiniScriptNativeFn ptr) {
  bfn->name = name;
  bfn->length = length;

  bfn->fn = newFunction(vm, name, length, NULL, true);
  bfn->fn->arity = arity;
  bfn->fn->native = ptr;
}

int findBuiltinFunction(const char* name, uint32_t length) {
  for (int i = 0; i < builtins_count; i++) {
    if (length == builtins[i].length &&
        strncmp(name, builtins[i].name, length) == 0) {
      return i;
    }
  }
  return -1;
}

/*****************************************************************************/
/* VALIDATORS                                                                */
/*****************************************************************************/

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
static inline bool validateNumeric(MSVM* vm, Var var, double* value,
  const char* name) {
  if (isNumeric(var, value)) return true;
  msSetRuntimeError(vm, "%s must be a numeric value.", name);
  return false;
}

// Check if [var] is integer. If not set error and return false.
static inline bool validateIngeger(MSVM* vm, Var var, int32_t* value,
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

static inline bool validateIndex(MSVM* vm, int32_t index, int32_t size,
  const char* container) {
  if (index < 0 || size <= index) {
    msSetRuntimeError(vm, "%s index out of range.", container);
    return false;
  }
  return true;
}

/*****************************************************************************/
/* BUILTIN FUNCTIONS                                                         */
/*****************************************************************************/

// Argument getter (1 based).
#define ARG(n) vm->fiber->ret[n]

// Argument count used in variadic functions.
#define ARGC ((int)(vm->fiber->sp - vm->fiber->ret) - 1)

// Set return value.
#define RET(value)             \
  do {                         \
    *(vm->fiber->ret) = value; \
    return;                    \
  } while (false)

Function* getBuiltinFunction(int index) {
  ASSERT_INDEX(index, builtins_count);
  return builtins[index].fn;
}

const char* getBuiltinFunctionName(int index) {
  ASSERT_INDEX(index, builtins_count);
  return builtins[index].name;
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
  Var arg1 = vm->fiber->ret[1];
  if (!IS_OBJ(arg1) || AS_OBJ(arg1)->type != OBJ_STRING) {
    msSetRuntimeError(vm, "Expected a String argument.");
  }

  String* path = (String*)AS_OBJ(arg1);
  if (path->length > 4 && strncmp(path->data, "std:", 4) == 0) {
    Script* scr = vmGetStdScript(vm, path->data + 4);
    ASSERT(scr != NULL, OOPS);
    RET(VAR_OBJ(scr));
  }

  TODO;
}

/*****************************************************************************/
/* STD METHODS                                                               */
/*****************************************************************************/

// std:list Methods.
void stdListSort(MSVM* vm) {
  Var list = ARG(1);
  if (!IS_OBJ(list) || AS_OBJ(list)->type != OBJ_LIST) {
    msSetRuntimeError(vm, "Expected a list at argument 1.");
  }

  // TODO: sort.
  
  RET(list);
}

// std:os Methods.
void stdOsClock(MSVM* vm) {
  RET(VAR_NUM((double)clock() / CLOCKS_PER_SEC));
}

/*****************************************************************************/
/* CORE INITIALIZATION                                                       */
/*****************************************************************************/
void initializeCore(MSVM* vm) {

  ASSERT(builtins_count == 0, "Initialize core only once.");

#define INITALIZE_BUILTIN_FN(name, fn, argc)                 \
  initializeBuiltinFN(vm, &builtins[builtins_count++], name, \
                      (int)strlen(name), argc, fn);

  // Initialize builtin functions.
  INITALIZE_BUILTIN_FN("is_null",     coreIsNull,     1);
  INITALIZE_BUILTIN_FN("is_bool",     coreIsBool,     1);
  INITALIZE_BUILTIN_FN("is_num",      coreIsNum,      1);
  
  INITALIZE_BUILTIN_FN("is_string",   coreIsString,   1);
  INITALIZE_BUILTIN_FN("is_list",     coreIsList,     1);
  INITALIZE_BUILTIN_FN("is_map",      coreIsMap,      1);
  INITALIZE_BUILTIN_FN("is_range",    coreIsRange,    1);
  INITALIZE_BUILTIN_FN("is_function", coreIsFunction, 1);
  INITALIZE_BUILTIN_FN("is_script",   coreIsScript,   1);
  INITALIZE_BUILTIN_FN("is_userobj",  coreIsUserObj,  1);
  
  INITALIZE_BUILTIN_FN("to_string",   coreToString,   1);
  INITALIZE_BUILTIN_FN("print",       corePrint,     -1);
  INITALIZE_BUILTIN_FN("import",      coreImport,     1);

  // Sentinal to mark the end of the array.
  //initializeBuiltinFN(vm, &builtins[i], NULL, 0, 0, NULL);

  // Make STD scripts.
  Script* std;  // A temporary pointer to the current std script.
  Function* fn; // A temporary pointer to the allocated function function.  

#define STD_NEW_SCRIPT(_name)              \
  do {                                     \
    std = newScript(vm);                   \
    std->name = _name;                     \
    vmPushTempRef(vm, &std->_super);       \
    vmAddStdScript(vm, std);               \
    vmPopTempRef(vm);                      \
  } while (false)

#define STD_ADD_FUNCTION(_name, fptr, _arity)                   \
  do {                                                          \
    fn = newFunction(vm, _name, (int)strlen(_name), std, true); \
    fn->native = fptr;                                          \
    fn->arity = _arity;                                         \
  } while (false)

  // std:list script.
  STD_NEW_SCRIPT("std:list");
  STD_ADD_FUNCTION("sort", stdListSort, 1);

  // std:os script.
  STD_NEW_SCRIPT("std:os");
  STD_ADD_FUNCTION("clock", stdOsClock, 0);
}

void markCoreObjects(MSVM* vm) {
  for (int i = 0; i < builtins_count; i++) {
    grayObject(&builtins[i].fn->_super, vm);
  }
}

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

Var varAdd(MSVM* vm, Var v1, Var v2) {

  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(d1 + d2);
    }
    return VAR_NULL;
  }

  if (IS_OBJ(v1) && IS_OBJ(v2)) {
    Object *o1 = AS_OBJ(v1), *o2 = AS_OBJ(v2);
    switch (o1->type) {

      case OBJ_STRING:
      {
        if (o2->type == OBJ_STRING) {
          return VAR_OBJ(stringFormat(vm, "@@", v1, v2));
        }
      } break;

      case OBJ_LIST:
      case OBJ_MAP:
      case OBJ_RANGE:
      case OBJ_SCRIPT:
      case OBJ_FUNC:
      case OBJ_FIBER:
      case OBJ_USER:
        TODO;
    }
  }


  msSetRuntimeError(vm, "Unsupported operand types for operator '-' "
    "%s and %s", varTypeName(v1), varTypeName(v2));

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

  TODO; // for user objects call vm.config.sub_userobj_sub(handles).

  msSetRuntimeError(vm, "Unsupported operand types for operator '-' "
    "%s and %s", varTypeName(v1), varTypeName(v2));

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

  msSetRuntimeError(vm, "Unsupported operand types for operator '*' "
    "%s and %s", varTypeName(v1), varTypeName(v2));
  return VAR_NULL;
}

Var varDivide(MSVM* vm, Var v1, Var v2) {
  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(d1 / d2);
    }
    return VAR_NULL;
  }

  msSetRuntimeError(vm, "Unsupported operand types for operator '/' "
    "%s and %s", varTypeName(v1), varTypeName(v2));
  return VAR_NULL;
}

bool varGreater(MSVM* vm, Var v1, Var v2) {
  double d1, d2;
  if (isNumeric(v1, &d1) && isNumeric(v2, &d2)) {
    return d1 > d2;
  }

  TODO;
  return false;
}

bool varLesser(MSVM* vm, Var v1, Var v2) {
  double d1, d2;
  if (isNumeric(v1, &d1) && isNumeric(v2, &d2)) {
    return d1 < d2;
  }

  TODO;
  return false;
}

// A convinent convenient macro used in varGetAttrib and varSetAttrib.
#define IS_ATTRIB(name) \
  (attrib->length == strlen(name) && strcmp(name, attrib->data) == 0)

#define ERR_NO_ATTRIB()                                             \
  msSetRuntimeError(vm, "'%s' objects has no attribute named '%s'", \
    varTypeName(on), attrib->data);

Var varGetAttrib(MSVM* vm, Var on, String* attrib) {

  if (!IS_OBJ(on)) {
    msSetRuntimeError(vm, "%s type is not subscriptable.", varTypeName(on));
    return VAR_NULL;
  }

  Object* obj = (Object*)AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
    {
      if (IS_ATTRIB("length")) {
        size_t length = ((String*)obj)->length;
        return VAR_NUM((double)length);
      }

      ERR_NO_ATTRIB();
      return VAR_NULL;
    }

    case OBJ_LIST:
    {
      if (IS_ATTRIB("length")) {
        size_t length = ((List*)obj)->elements.count;
        return VAR_NUM((double)length);
      }

      ERR_NO_ATTRIB();
      return VAR_NULL;
    }

    case OBJ_MAP:
    {
      Var value = mapGet((Map*)obj, VAR_OBJ(&attrib->_super));
      if (IS_UNDEF(value)) {
        msSetRuntimeError(vm, "Key (\"%s\") not exists.", attrib->data);
        return VAR_NULL;
      }
      return value;
    }

    case OBJ_RANGE:
      TODO;

    case OBJ_SCRIPT: {
      Script* scr = (Script*)obj;

      // Search in functions.
      uint32_t index = scriptSearchFunc(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX(index, scr->functions.count);
        return VAR_OBJ(scr->functions.data[index]);
      }

      // Search in globals.
      index = scriptSearchGlobals(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX(index, scr->globals.count);
        return scr->globals.data[index];
      }

      ERR_NO_ATTRIB();
      return VAR_NULL;
    }

    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_USER:
      TODO;

    default:
      UNREACHABLE();
  }
  CHECK_MISSING_OBJ_TYPE(7);

  UNREACHABLE();
  return VAR_NULL;
}

void varSetAttrib(MSVM* vm, Var on, String* attrib, Var value) {

#define ATTRIB_IMMUTABLE(prop)                                   \
do {                                                             \
  if (IS_ATTRIB(prop)) {                                         \
    msSetRuntimeError(vm, "'%s' attribute is immutable.", prop); \
    return;                                                      \
  }                                                              \
} while (false)

  if (!IS_OBJ(on)) {
    msSetRuntimeError(vm, "%s type is not subscriptable.", varTypeName(on));
    return;
  }

  Object* obj = (Object*)AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      ATTRIB_IMMUTABLE("length");
      ERR_NO_ATTRIB();
      return;

    case OBJ_LIST:
      ATTRIB_IMMUTABLE("length");
      ERR_NO_ATTRIB();
      return;

    case OBJ_MAP:
      TODO;
      ERR_NO_ATTRIB();
      return;

    case OBJ_RANGE:
      ERR_NO_ATTRIB();
      return;

    case OBJ_SCRIPT: {
      Script* scr = (Script*)obj;

      // Check globals.
      uint32_t index = scriptSearchGlobals(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX(index, scr->globals.count);
        scr->globals.data[index] = value;
        return;
      }

      // Check function (Functions are immutable).
      index = scriptSearchFunc(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX(index, scr->functions.count);
        ATTRIB_IMMUTABLE(scr->functions.data[index]->name);
        return;
      }

      ERR_NO_ATTRIB();
      return;
    }

    case OBJ_FUNC:
      ERR_NO_ATTRIB();
      return;

    case OBJ_FIBER:
      ERR_NO_ATTRIB();
      return;

    case OBJ_USER:
      TODO; //ERR_NO_ATTRIB();
      return;

    default:
      UNREACHABLE();
  }
  CHECK_MISSING_OBJ_TYPE(7);
  UNREACHABLE();
}

Var varGetSubscript(MSVM* vm, Var on, Var key) {
  if (!IS_OBJ(on)) {
    msSetRuntimeError(vm, "%s type is not subscriptable.", varTypeName(on));
    return VAR_NULL;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
    {
      int32_t index;
      String* str = ((String*)obj);
      if (!validateIngeger(vm, key, &index, "List index")) {
        return VAR_NULL;
      }
      if (!validateIndex(vm, index, str->length, "String")) {
        return VAR_NULL;
      }
      String* c = newString(vm, str->data + index, 1);
      return VAR_OBJ(c);
    }

    case OBJ_LIST:
    {
      int32_t index;
      VarBuffer* elems = &((List*)obj)->elements;
      if (!validateIngeger(vm, key, &index, "List index")) {
        return VAR_NULL;
      }
      if (!validateIndex(vm, index, (int)elems->count, "List")) {
        return VAR_NULL;
      }
      return elems->data[index];
    }

    case OBJ_MAP:
    {
      Var value = mapGet((Map*)obj, key);
      if (IS_UNDEF(value)) {
        String* key_str = toString(vm, key, true);
        vmPushTempRef(vm, &key_str->_super);
        msSetRuntimeError(vm, "Key (%s) not exists.", key_str->data);
        vmPopTempRef(vm);
        return VAR_NULL;
      }
      return value;
    }

    case OBJ_RANGE:
    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_USER:
      TODO;
    default:
      UNREACHABLE();
  }

  CHECK_MISSING_OBJ_TYPE(7);
  UNREACHABLE();
  return VAR_NULL;
}

void varsetSubscript(MSVM* vm, Var on, Var key, Var value) {
  if (!IS_OBJ(on)) {
    msSetRuntimeError(vm, "%s type is not subscriptable.", varTypeName(on));
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      msSetRuntimeError(vm, "String objects are immutable.");
      return;

    case OBJ_LIST:
    {
      int32_t index;
      VarBuffer* elems = &((List*)obj)->elements;
      if (!validateIngeger(vm, key, &index, "List index")) return;
      if (!validateIndex(vm, index, (int)elems->count, "List")) return;
      elems->data[index] = value;
      return;
    }

    case OBJ_MAP:
    {
      if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
        msSetRuntimeError(vm, "%s type is not hashable.", varTypeName(key));
      } else {
        mapSet((Map*)obj, vm, key, value);
      }
      return;
    }

    case OBJ_RANGE:
    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_USER:
      TODO;
    default:
      UNREACHABLE();
  }
  CHECK_MISSING_OBJ_TYPE(7);
  UNREACHABLE();
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

  uint32_t iter = 0; //< Nth iteration.
  if (IS_NUM(*iterator)) {
    iter = _AS_INTEGER(*iterator);
  }

  switch (obj->type) {
    case OBJ_STRING: {
      // TODO: // Need to consider utf8.
      String* str = ((String*)obj);
      if (iter >= str->length) {
        return false; //< Stop iteration.
      }
      // TODO: Or I could add char as a type for efficiency.
      *value = VAR_OBJ(newString(vm, str->data + iter, 1));
      *iterator = VAR_NUM((double)iter + 1);
      return true;
    }

    case OBJ_LIST: {
      VarBuffer* elems = &((List*)obj)->elements;
      if (iter >= elems->count) {
        return false; //< Stop iteration.
      }
      *value = elems->data[iter];
      *iterator = VAR_NUM((double)iter + 1);
      return true;
    }

    case OBJ_MAP: {
      Map* map = (Map*)obj;

      // Find the next valid key.
      for (; iter < map->capacity; iter++) {
        if (!IS_UNDEF(map->entries[iter].key)) {
          break;
        }
      }
      if (iter >= map->capacity) {
        return false; //< Stop iteration.
      }

      *value = map->entries[iter].key;
      *iterator = VAR_NUM((double)iter + 1);
      return true;
    }

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
    case OBJ_FIBER:
    case OBJ_USER:
      TODO;
      break;
    default:
      UNREACHABLE();
  }
  CHECK_MISSING_OBJ_TYPE(7);
  UNREACHABLE();
  return false;
}