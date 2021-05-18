/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "core.h"

#include <ctype.h>
#include <math.h>
#include <time.h>

#include "utils.h"
#include "var.h"
#include "vm.h"

// Convert number var as int32_t. Check if it's number before using it.
#define _AS_INTEGER(var) (int32_t)trunc(AS_NUM(var))

static void initializeBuiltinFN(PKVM* vm, BuiltinFn* bfn, const char* name,
                               int length, int arity, pkNativeFn ptr) {
  bfn->name = name;
  bfn->length = length;

  bfn->fn = newFunction(vm, name, length, NULL, true);
  bfn->fn->arity = arity;
  bfn->fn->native = ptr;
}

int findBuiltinFunction(PKVM* vm, const char* name, uint32_t length) {
  for (int i = 0; i < vm->builtins_count; i++) {
    if (length == vm->builtins[i].length &&
        strncmp(name, vm->builtins[i].name, length) == 0) {
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
static inline bool validateNumeric(PKVM* vm, Var var, double* value,
  const char* name) {
  if (isNumeric(var, value)) return true;
  vm->fiber->error = stringFormat(vm, "$ must be a numeric value.", name);
  return false;
}

// Check if [var] is integer. If not set error and return false.
static inline bool validateIngeger(PKVM* vm, Var var, int32_t* value,
  const char* name) {
  double number;
  if (isNumeric(var, &number)) {
    double truncated = trunc(number);
    if (truncated == number) {
      *value = (int32_t)(truncated);
      return true;
    }
  }

  vm->fiber->error = stringFormat(vm, "$ must be an integer.", name);
  return false;
}

static inline bool validateIndex(PKVM* vm, int32_t index, int32_t size,
  const char* container) {
  if (index < 0 || size <= index) {
    vm->fiber->error = stringFormat(vm, "$ index out of range.", container);
    return false;
  }
  return true;
}

// Check if [var] is string for argument at [arg_ind]. If not set error and
// return false.
static bool validateArgString(PKVM* vm, Var var, String** value, int arg_ind) {
  if (!IS_OBJ(var) || AS_OBJ(var)->type != OBJ_STRING) {
    String* str_arg = toString(vm, VAR_NUM((double)arg_ind), false);
    vmPushTempRef(vm, &str_arg->_super);
    vm->fiber->error = stringFormat(vm, "Expected a string at argument @.",
                                    str_arg, false);
    vmPopTempRef(vm);
  }
  *value = (String*)AS_OBJ(var);
  return true;
}

/*****************************************************************************/
/* BUILTIN FUNCTIONS API                                                     */
/*****************************************************************************/

// Argument getter (1 based).
#define ARG(n) vm->fiber->ret[n]

// Convinent macros.
#define ARG1 ARG(1)
#define ARG2 ARG(2)
#define ARG3 ARG(3)

// Argument count used in variadic functions.
#define ARGC ((int)(vm->fiber->sp - vm->fiber->ret) - 1)

// Set return value.
#define RET(value)             \
  do {                         \
    *(vm->fiber->ret) = value; \
    return;                    \
  } while (false)

Function* getBuiltinFunction(PKVM* vm, int index) {
  ASSERT_INDEX(index, vm->builtins_count);
  return vm->builtins[index].fn;
}

const char* getBuiltinFunctionName(PKVM* vm, int index) {
  ASSERT_INDEX(index, vm->builtins_count);
  return vm->builtins[index].name;
}

Script* getCoreLib(PKVM* vm, String* name) {
  Var lib = mapGet(vm->core_libs, VAR_OBJ(&name->_super));
  if (IS_UNDEF(lib)) return NULL;
  ASSERT(IS_OBJ(lib) && AS_OBJ(lib)->type == OBJ_SCRIPT, OOPS);
  return (Script*)AS_OBJ(lib);
}

/*****************************************************************************/
/* CORE BUILTIN FUNCTIONS                                                    */
/*****************************************************************************/

#define FN_IS_PRIMITE_TYPE(name, check)       \
  void coreIs##name(PKVM* vm) {               \
    RET(VAR_BOOL(check(ARG1)));               \
  }

#define FN_IS_OBJ_TYPE(name, _enum)                     \
  void coreIs##name(PKVM* vm) {                         \
    Var arg1 = ARG1;                                    \
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

void coreAssert(PKVM* vm) {
  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    vm->fiber->error = newString(vm, "Invalid argument count.");
    return;
  }

  if (!toBool(ARG1)) {
    String* msg = NULL;

    if (argc == 2) {
      if (AS_OBJ(ARG2)->type != OBJ_STRING) {
        msg = toString(vm, ARG2, false);
      } else {
        msg = (String*)AS_OBJ(ARG2);
      }
      vmPushTempRef(vm, &msg->_super);
      vm->fiber->error = stringFormat(vm, "Assertion failed: '@'.", msg);
      vmPopTempRef(vm);
    } else {
      vm->fiber->error = newString(vm, "Assertion failed.");
    }
  }
}

// Return the has value of the variable, if it's not hashable it'll return null.
void coreHash(PKVM* vm) {
  if (IS_OBJ(ARG1)) {
    if (!isObjectHashable(AS_OBJ(ARG1)->type)) {
      RET(VAR_NULL);
    }
  }
  RET(VAR_NUM((double)varHashValue(ARG1)));
}

void coreToString(PKVM* vm) {
  RET(VAR_OBJ(&toString(vm, ARG1, false)->_super));
}

void corePrint(PKVM* vm) {
  // If the host appliaction donesn't provide any write function, discard the
  // output.
  if (vm->config.write_fn == NULL) return;

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

// string functions
// ----------------

void coreStrLower(PKVM* vm) {
  String* str;
  if (!validateArgString(vm, ARG1, &str, 1)) return;

  String* result = newStringLength(vm, str->data, str->length);
  char* data = result->data;
  for (; *data; ++data) *data = tolower(*data);
  // Since the string is modified re-hash it.
  result->hash = utilHashString(result->data);

  RET(VAR_OBJ(&result->_super));
}

void coreStrUpper(PKVM* vm) {
  String* str;
  if (!validateArgString(vm, ARG1, &str, 1)) return;

  String* result = newStringLength(vm, str->data, str->length);
  char* data = result->data;
  for (; *data; ++data) *data = toupper(*data);
  // Since the string is modified re-hash it.
  result->hash = utilHashString(result->data);
  
  RET(VAR_OBJ(&result->_super));
}

void coreStrStrip(PKVM* vm) {
  String* str;
  if (!validateArgString(vm, ARG1, &str, 1)) return;

  const char* start = str->data;
  while (*start && isspace(*start)) start++;
  if (*start == '\0') RET(VAR_OBJ(&newStringLength(vm, NULL, 0)->_super));

  const char* end = str->data + str->length - 1;
  while (isspace(*end)) end--;

  RET(VAR_OBJ(&newStringLength(vm, start, (uint32_t)(end - start + 1))->_super));
}

/*****************************************************************************/
/* CORE LIBRARY METHODS                                                      */
/*****************************************************************************/

// 'path' library methods.
// -----------------------

//  TODO: path library should be added by the cli (or the hosting application).
void stdPathAbspath(PKVM* vm) {
  Var relpath = ARG1;
  if (!IS_OBJ(relpath) || AS_OBJ(relpath)->type != OBJ_STRING) {
    vm->fiber->error = newString(vm, "Expected a string at argument 1.");
  }

  // TODO: abspath.
  RET(VAR_OBJ(newString(vm, "TODO: abspath")));
}

void stdPathCurdir(PKVM* vm) {
  RET(VAR_OBJ(newString(vm, "TODO: curdir")));
}

// 'lang' library methods.
// -----------------------

// Returns the number of seconds since the application started.
void stdLangClock(PKVM* vm) {
  RET(VAR_NUM((double)clock() / CLOCKS_PER_SEC));
}

// Trigger garbage collection and return the ammount of bytes cleaned.
void stdLangGC(PKVM* vm) {
  size_t bytes_before = vm->bytes_allocated;
  vmCollectGarbage(vm);
  size_t garbage = bytes_before - vm->bytes_allocated;
  RET(VAR_NUM((double)garbage));
}

// Write function, just like print function but it wont put space between args
// and write a new line at the end.
void stdLangWrite(PKVM* vm) {
  // If the host appliaction donesn't provide any write function, discard the
  // output.
  if (vm->config.write_fn == NULL) return;

  String* str; //< Will be cleaned by garbage collector;

  for (int i = 1; i <= ARGC; i++) {
    Var arg = ARG(i);
    // If it's already a string don't allocate a new string instead use it.
    if (IS_OBJ(arg) && AS_OBJ(arg)->type == OBJ_STRING) {
      str = (String*)AS_OBJ(arg);
    } else {
      str = toString(vm, arg, false);
    }

    vm->config.write_fn(vm, str->data);
  }
}

/*****************************************************************************/
/* CORE INITIALIZATION                                                       */
/*****************************************************************************/
void initializeCore(PKVM* vm) {

#define INITALIZE_BUILTIN_FN(name, fn, argc)                         \
  initializeBuiltinFN(vm, &vm->builtins[vm->builtins_count++], name, \
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
  
  INITALIZE_BUILTIN_FN("assert",      coreAssert,    -1);
  INITALIZE_BUILTIN_FN("hash",        coreHash,       1);
  INITALIZE_BUILTIN_FN("to_string",   coreToString,   1);
  INITALIZE_BUILTIN_FN("print",       corePrint,     -1);

  // string functions.
  INITALIZE_BUILTIN_FN("str_lower",   coreStrLower,   1);
  INITALIZE_BUILTIN_FN("str_upper",   coreStrUpper,   1);
  INITALIZE_BUILTIN_FN("str_strip",   coreStrStrip,   1);

  // Make STD scripts.
  Script* std;  // A temporary pointer to the current std script.
  Function* fn; // A temporary pointer to the allocated function function.
#define STD_NEW_SCRIPT(_name)                                                 \
  do {                                                                        \
    /* Create a new Script. */                                                \
    String* name = newString(vm, _name);                                      \
    vmPushTempRef(vm, &name->_super);                                         \
    std = newScript(vm, name);                                                \
    std->moudle = name; /* Core libs's path and the module are the same. */   \
    vmPopTempRef(vm);                                                         \
    /* Add the script to core_libs. */                                        \
    vmPushTempRef(vm, &std->_super);                                          \
    mapSet(vm->core_libs, vm, VAR_OBJ(&name->_super), VAR_OBJ(&std->_super)); \
    vmPopTempRef(vm);                                                         \
  } while (false)

#define STD_ADD_FUNCTION(_name, fptr, _arity)                   \
  do {                                                          \
    fn = newFunction(vm, _name, (int)strlen(_name), std, true); \
    fn->native = fptr;                                          \
    fn->arity = _arity;                                         \
  } while (false)

  // path
  STD_NEW_SCRIPT("path");
  STD_ADD_FUNCTION("abspath", stdPathAbspath, 1);
  STD_ADD_FUNCTION("curdir",  stdPathCurdir,  0);

  // lang
  STD_NEW_SCRIPT("lang");
  STD_ADD_FUNCTION("clock", stdLangClock,  0);
  STD_ADD_FUNCTION("gc",    stdLangGC,     0);
  STD_ADD_FUNCTION("write", stdLangWrite, -1);
}

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

#define UNSUPPORT_OPERAND_TYPES(op)                                    \
  vm->fiber->error = stringFormat(vm, "Unsupported operand types for " \
    "operator '" op "' $ and $", varTypeName(v1), varTypeName(v2))

Var varAdd(PKVM* vm, Var v1, Var v2) {

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
        TODO;

      case OBJ_MAP:
      case OBJ_RANGE:
      case OBJ_SCRIPT:
      case OBJ_FUNC:
      case OBJ_FIBER:
      case OBJ_USER:
        break;
    }
  }

  UNSUPPORT_OPERAND_TYPES("+");
  return VAR_NULL;
}

Var varSubtract(PKVM* vm, Var v1, Var v2) {
  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(d1 - d2);
    }
    return VAR_NULL;
  }

  UNSUPPORT_OPERAND_TYPES("-");
  return VAR_NULL;
}

Var varMultiply(PKVM* vm, Var v1, Var v2) {

  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(d1 * d2);
    }
    return VAR_NULL;
  }

  UNSUPPORT_OPERAND_TYPES("*");
  return VAR_NULL;
}

Var varDivide(PKVM* vm, Var v1, Var v2) {
  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(d1 / d2);
    }
    return VAR_NULL;
  }

  UNSUPPORT_OPERAND_TYPES("/");
  return VAR_NULL;
}

Var varModulo(PKVM* vm, Var v1, Var v2) {

  double d1, d2;
  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, "Right operand")) {
      return VAR_NUM(fmod(d1, d2));
    }
    return VAR_NULL;
  }

  if (IS_OBJ(v1) && AS_OBJ(v1)->type == OBJ_STRING) {
    String* str = (String*)AS_OBJ(v1);
    TODO; // "fmt" % v2.
  }

  UNSUPPORT_OPERAND_TYPES("%");
  return VAR_NULL;
}

bool varGreater(PKVM* vm, Var v1, Var v2) {
  double d1, d2;
  if (isNumeric(v1, &d1) && isNumeric(v2, &d2)) {
    return d1 > d2;
  }

  TODO;
  return false;
}

bool varLesser(PKVM* vm, Var v1, Var v2) {
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

#define ERR_NO_ATTRIB()                                               \
  vm->fiber->error = stringFormat(vm, "'$' objects has no attribute " \
                                       "named '$'",                   \
                                  varTypeName(on), attrib->data);

Var varGetAttrib(PKVM* vm, Var on, String* attrib) {

  if (!IS_OBJ(on)) {
    vm->fiber->error = stringFormat(vm, "$ type is not subscriptable.",
                                    varTypeName(on));
    return VAR_NULL;
  }

  Object* obj = AS_OBJ(on);
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
        vm->fiber->error = stringFormat(vm, "Key (\"@\") not exists.", attrib);
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

void varSetAttrib(PKVM* vm, Var on, String* attrib, Var value) {

#define ATTRIB_IMMUTABLE(prop)                                                \
do {                                                                          \
  if (IS_ATTRIB(prop)) {                                                      \
    vm->fiber->error = stringFormat(vm, "'$' attribute is immutable.", prop); \
    return;                                                                   \
  }                                                                           \
} while (false)

  if (!IS_OBJ(on)) {
    vm->fiber->error = stringFormat(vm, "$ type is not subscriptable.",
                                    varTypeName(on));
    return;
  }

  Object* obj = AS_OBJ(on);
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

Var varGetSubscript(PKVM* vm, Var on, Var key) {
  if (!IS_OBJ(on)) {
    vm->fiber->error = stringFormat(vm, "$ type is not subscriptable.",
                                    varTypeName(on));
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
      String* c = newStringLength(vm, str->data + index, 1);
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
        if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
          vm->fiber->error = stringFormat(vm, "Invalid key '@'.", key_str);
        } else {
          vm->fiber->error = stringFormat(vm, "Key '@' not exists", key_str);
        }
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

void varsetSubscript(PKVM* vm, Var on, Var key, Var value) {
  if (!IS_OBJ(on)) {
    vm->fiber->error = stringFormat(vm, "$ type is not subscriptable.", varTypeName(on));
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      vm->fiber->error = newString(vm, "String objects are immutable.");
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
        vm->fiber->error = stringFormat(vm, "$ type is not hashable.", varTypeName(key));
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

bool varIterate(PKVM* vm, Var seq, Var* iterator, Var* value) {

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
      vm->fiber->error = newString(vm, "Null is not iterable.");
    } else if (IS_BOOL(seq)) {
      vm->fiber->error = newString(vm, "Boolenan is not iterable.");
    } else if (IS_NUM(seq)) {
      vm->fiber->error = newString(vm, "Number is not iterable.");
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
      *value = VAR_OBJ(newStringLength(vm, str->data + iter, 1));
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