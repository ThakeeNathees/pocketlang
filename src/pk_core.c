/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "pk_core.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#include "pk_debug.h"
#include "pk_utils.h"
#include "pk_vm.h"

// M_PI is non standard. The macro _USE_MATH_DEFINES defining before importing
// <math.h> will define the constants for MSVC. But for a portable solution,
// we're defining it ourselves if it isn't already.
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// Returns the docstring of the function, which is a static const char* defined
// just above the function by the DEF() macro below.
#define DOCSTRING(fn) _pk_doc_##fn

// A macro to declare a function, with docstring, which is defined as
// _pk_doc_<fn> = docstring; That'll used to generate function help text.
#define DEF(fn, docstring)                      \
  static const char* DOCSTRING(fn) = docstring; \
  static void fn(PKVM* vm)

// Create a new module with the given [name] and returns as a Module* for
// internal. Which will be wrapped by pkNewModule to return a pkHandle*.
static Module* newModuleInternal(PKVM* vm, const char* name);

// Adds a function to the module with the give properties and add the function
// to the module's globals variables.
static void moduleAddFunctionInternal(PKVM* vm, Module* module,
                                      const char* name, pkNativeFn fptr,
                                      int arity, const char* docstring);

/*****************************************************************************/
/* CORE PUBLIC API                                                           */
/*****************************************************************************/

#define CHECK_NULL(name) \
  ASSERT(name != NULL, "Argument " #name " was NULL.");

#define CHECK_TYPE(handle, type)              \
  ASSERT(IS_OBJ_TYPE(handle->value, type),    \
         "Given handle is not of type " #type ".");

PkHandle* pkNewModule(PKVM* vm, const char* name) {
  CHECK_NULL(name);

  Module* module = newModuleInternal(vm, name);
  return vmNewHandle(vm, VAR_OBJ(module));
}

void pkRegisterModule(PKVM* vm, PkHandle* module) {
  CHECK_NULL(module);
  CHECK_TYPE(module, OBJ_MODULE);

  Module* module_ = (Module*)AS_OBJ(module->value);
  vmRegisterModule(vm, module_, module_->name);
}

PkHandle* pkNewClass(PKVM* vm, PkHandle* module, const char* name) {
  CHECK_NULL(module);
  CHECK_NULL(name);
  CHECK_TYPE(module, OBJ_MODULE);

  Class* class_ = newClass(vm, name, (int)strlen(name),
                           (Module*)AS_OBJ(module->value), NULL, NULL);
  return vmNewHandle(vm, VAR_OBJ(class_));
}

void pkModuleAddGlobal(PKVM* vm, PkHandle* module,
                                 const char* name, PkHandle* value) {
  CHECK_NULL(module);
  CHECK_NULL(value);
  CHECK_TYPE(module, OBJ_MODULE);

  moduleAddGlobal(vm, (Module*)AS_OBJ(module->value),
                  name, (uint32_t)strlen(name), value->value);
}

PkHandle* pkModuleGetGlobal(PKVM* vm, PkHandle* module, const char* name) {
  CHECK_NULL(module);
  CHECK_NULL(name);
  CHECK_TYPE(module, OBJ_MODULE);

  Module* module_ = (Module*)AS_OBJ(module->value);
  int index = moduleGetGlobalIndex(module_, name, (uint32_t)strlen(name));
  if (index == -1) return NULL;
  return vmNewHandle(vm, module_->globals.data[index]);
}

void pkModuleAddFunction(PKVM* vm, PkHandle* module, const char* name,
                         pkNativeFn fptr, int arity) {
  CHECK_NULL(module);
  CHECK_NULL(fptr);
  CHECK_TYPE(module, OBJ_MODULE);

  moduleAddFunctionInternal(vm, (Module*)AS_OBJ(module->value),
                            name, fptr, arity,
                            NULL /*TODO: Public API for function docstring.*/);
}

void pkClassAddMethod(PKVM* vm, PkHandle* cls,
                      const char* name,
                      pkNativeFn fptr, int arity) {
  CHECK_NULL(cls);
  CHECK_NULL(fptr);
  CHECK_TYPE(cls, OBJ_MODULE);

  TODO;
}

PkHandle* pkModuleGetMainFunction(PKVM* vm, PkHandle* module) {
  CHECK_NULL(module);
  CHECK_TYPE(module, OBJ_MODULE);

  Module* _module = (Module*)AS_OBJ(module->value);

  int main_index = moduleGetGlobalIndex(_module, IMPLICIT_MAIN_NAME,
                                        (uint32_t)strlen(IMPLICIT_MAIN_NAME));
  if (main_index == -1) return NULL;
  ASSERT_INDEX(main_index, (int)_module->globals.count);
  Var main_fn = _module->globals.data[main_index];
  ASSERT(IS_OBJ_TYPE(main_fn, OBJ_CLOSURE), OOPS);
  return vmNewHandle(vm, main_fn);
}

// A convenient macro to get the nth (1 based) argument of the current
// function.
#define ARG(n) (vm->fiber->ret[n])

// Evaluates to the current function's argument count.
#define ARGC ((int)(vm->fiber->sp - vm->fiber->ret) - 1)

// Set return value for the current native function and return.
#define RET(value)             \
  do {                         \
    *(vm->fiber->ret) = value; \
    return;                    \
  } while (false)

#define RET_ERR(err)           \
  do {                         \
    VM_SET_ERROR(vm, err);     \
    RET(VAR_NULL);             \
  } while(false)

// Check for errors in before calling the get arg public api function.
#define CHECK_GET_ARG_API_ERRORS()                                          \
  do {                                                                      \
    ASSERT(vm->fiber != NULL,                                               \
             "This function can only be called at runtime.");               \
    if (arg != 0) {/* If Native setter, the value would be at fiber->ret */ \
      ASSERT(arg > 0 && arg <= ARGC, "Invalid argument index.");            \
    }                                                                       \
    ASSERT(value != NULL, "Argument [value] was NULL.");                    \
  } while (false)

// Set error for incompatible type provided as an argument. (TODO: got type).
#define ERR_INVALID_ARG_TYPE(m_type)                                        \
do {                                                                        \
  if (arg != 0) { /* If Native setter, arg index would be 0. */             \
    char buff[STR_INT_BUFF_SIZE];                                           \
    sprintf(buff, "%d", arg);                                               \
    VM_SET_ERROR(vm, stringFormat(vm, "Expected a '$' at argument $.",      \
                                      m_type, buff));                       \
  } else {                                                                  \
    VM_SET_ERROR(vm, stringFormat(vm, "Expected a '$'.", m_type));          \
  }                                                                         \
} while (false)

int pkGetArgc(const PKVM* vm) {
  ASSERT(vm->fiber != NULL, "This function can only be called at runtime.");
  return ARGC;
}

bool pkCheckArgcRange(PKVM* vm, int argc, int min, int max) {
  ASSERT(min <= max, "invalid argc range (min > max).");

  if (argc < min) {
    char buff[STR_INT_BUFF_SIZE]; sprintf(buff, "%d", min);
    VM_SET_ERROR(vm, stringFormat(vm, "Expected at least %s argument(s).",
                                       buff));
    return false;

  } else if (argc > max) {
    char buff[STR_INT_BUFF_SIZE]; sprintf(buff, "%d", max);
    VM_SET_ERROR(vm, stringFormat(vm, "Expected at most %s argument(s).",
                                       buff));
    return false;
  }

  return true;
}

PkVar pkGetArg(const PKVM* vm, int arg) {
  ASSERT(vm->fiber != NULL, "This function can only be called at runtime.");
  ASSERT(arg > 0 || arg <= ARGC, "Invalid argument index.");

  return &(ARG(arg));
}

void* pkGetSelf(const PKVM* vm) {
  // Assert we're inside of a method.
  TODO;
}

void pkSetSelf(PKVM* vm, void* self) {
  // Assert it's a constructor.
  TODO;
}

bool pkGetArgBool(PKVM* vm, int arg, bool* value) {
  CHECK_GET_ARG_API_ERRORS();

  Var val = ARG(arg);
  *value = toBool(val);
  return true;
}

bool pkGetArgNumber(PKVM* vm, int arg, double* value) {
  CHECK_GET_ARG_API_ERRORS();

  Var val = ARG(arg);
  if (IS_NUM(val)) {
    *value = AS_NUM(val);

  } else if (IS_BOOL(val)) {
    *value = AS_BOOL(val) ? 1 : 0;

  } else {
    ERR_INVALID_ARG_TYPE("number");
    return false;
  }

  return true;
}

bool pkGetArgString(PKVM* vm, int arg, const char** value, uint32_t* length) {
  CHECK_GET_ARG_API_ERRORS();

  Var val = ARG(arg);
  if (IS_OBJ_TYPE(val, OBJ_STRING)) {
    String* str = (String*)AS_OBJ(val);
    *value = str->data;
    if (length) *length = str->length;

  } else {
    ERR_INVALID_ARG_TYPE("string");
    return false;
  }

  return true;
}

bool pkGetArgValue(PKVM* vm, int arg, PkVarType type, PkVar* value) {
  CHECK_GET_ARG_API_ERRORS();

  Var val = ARG(arg);
  if (pkGetValueType((PkVar)&val) != type) {
    char buff[STR_INT_BUFF_SIZE]; sprintf(buff, "%d", arg);
    VM_SET_ERROR(vm, stringFormat(vm, "Expected a $ at argument $.",
                 getPkVarTypeName(type), buff));
    return false;
  }

  *value = (PkVar)&val;
  return true;
}

void pkReturnNull(PKVM* vm) {
  RET(VAR_NULL);
}

void pkReturnBool(PKVM* vm, bool value) {
  RET(VAR_BOOL(value));
}

void pkReturnNumber(PKVM* vm, double value) {
  RET(VAR_NUM(value));
}

void pkReturnString(PKVM* vm, const char* value) {
  RET(VAR_OBJ(newString(vm, value)));
}

void pkReturnStringLength(PKVM* vm, const char* value, size_t length) {
  RET(VAR_OBJ(newStringLength(vm, value, (uint32_t)length)));
}

void pkReturnValue(PKVM* vm, PkVar value) {
  RET(*(Var*)value);
}

void pkReturnHandle(PKVM* vm, PkHandle* handle) {
  RET(handle->value);
}

const char* pkStringGetData(const PkVar value) {
  const Var str = (*(const Var*)value);
  ASSERT(IS_OBJ_TYPE(str, OBJ_STRING), "Value should be of type string.");
  return ((String*)AS_OBJ(str))->data;
}

PkVar pkFiberGetReturnValue(const PkHandle* fiber) {
  ASSERT(fiber != NULL, "Handle fiber was NULL.");
  Var fb = fiber->value;
  ASSERT(IS_OBJ_TYPE(fb, OBJ_FIBER), "Given handle is not a fiber");
  Fiber* _fiber = (Fiber*)AS_OBJ(fb);
  return (PkVar)_fiber->ret;
}

bool pkFiberIsDone(const PkHandle* fiber) {
  ASSERT(fiber != NULL, "Handle fiber was NULL.");
  Var fb = fiber->value;
  ASSERT(IS_OBJ_TYPE(fb, OBJ_FIBER), "Given handle is not a fiber");
  Fiber* _fiber = (Fiber*)AS_OBJ(fb);
  return _fiber->state == FIBER_DONE;
}

#undef CHECK_NULL
#undef CHECK_TYPE

/*****************************************************************************/
/* VALIDATORS                                                                */
/*****************************************************************************/

// Evaluated to true of the [num] is in byte range.
#define IS_NUM_BYTE(num) ((CHAR_MIN <= (num)) && ((num) <= CHAR_MAX))

// Check if [var] is a numeric value (bool/number) and set [value].
static inline bool isNumeric(Var var, double* value) {
  if (IS_NUM(var)) {
    *value = AS_NUM(var);
    return true;
  }
  if (IS_BOOL(var)) {
    *value = AS_BOOL(var);
    return true;
  }
  return false;
}

// Check if [var] is a numeric value (bool/number) and set [value].
static inline bool isInteger(Var var, int64_t* value) {
  double number;
  if (isNumeric(var, &number)) {
    // TODO: check if the number is larger for a 64 bit integer.
    if (floor(number) == number) {
      ASSERT(INT64_MIN <= number && number <= INT64_MAX,
        "TODO: Large numbers haven't handled yet. Please report!");
      *value = (int64_t)(number);
      return true;
    }
  }
  return false;
}

// Check if [var] is bool/number. If not, it'll set error and return false.
static inline bool validateNumeric(PKVM* vm, Var var, double* value,
                                   const char* name) {
  if (isNumeric(var, value)) return true;
  VM_SET_ERROR(vm, stringFormat(vm, "$ must be a numeric value.", name));
  return false;
}

// Check if [var] is 32 bit integer. If not, it'll set error and return false.
static inline bool validateInteger(PKVM* vm, Var var, int64_t* value,
                                   const char* name) {
  if (isInteger(var, value)) return true;
  VM_SET_ERROR(vm, stringFormat(vm, "$ must be a whole number.", name));
  return false;
}

// Index is could be larger than 32 bit integer, but the size in pocketlang
// limited to 32 unsigned bit integer
static inline bool validateIndex(PKVM* vm, int64_t index, uint32_t size,
                                 const char* container) {
  if (index < 0 || size <= index) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ index out of bound.", container));
    return false;
  }
  return true;
}

// Check if the [condition] is true. If not, it'll set an error and return
// false.
static inline bool validateCond(PKVM* vm, bool condition, const char* err) {
  if (!condition) {
    VM_SET_ERROR(vm, newString(vm, err));
    return false;
  }
  return true;
}

// Check if [var] is string for argument at [arg]. If not set error and
// return false.
#define VALIDATE_ARG_OBJ(m_class, m_type, m_name)                            \
  static bool validateArg##m_class(PKVM* vm, int arg, m_class** value) {     \
    Var var = ARG(arg);                                                      \
    ASSERT(arg > 0 && arg <= ARGC, OOPS);                                    \
    if (!IS_OBJ(var) || AS_OBJ(var)->type != m_type) {                       \
      char buff[12]; sprintf(buff, "%d", arg);                               \
      VM_SET_ERROR(vm, stringFormat(vm, "Expected a " m_name                 \
                   " at argument $.", buff, false));                         \
      return false;                                                          \
    }                                                                        \
    *value = (m_class*)AS_OBJ(var);                                          \
    return true;                                                             \
   }
 VALIDATE_ARG_OBJ(String, OBJ_STRING, "string")
 VALIDATE_ARG_OBJ(List, OBJ_LIST, "list")
 VALIDATE_ARG_OBJ(Map, OBJ_MAP, "map")
 VALIDATE_ARG_OBJ(Closure, OBJ_CLOSURE, "closure")
 VALIDATE_ARG_OBJ(Fiber, OBJ_FIBER, "fiber")
 VALIDATE_ARG_OBJ(Class, OBJ_CLASS, "class")

/*****************************************************************************/
/* SHARED FUNCTIONS                                                          */
/*****************************************************************************/

static void initializeBuiltinFunctions(PKVM* vm);
static void initializeCoreModules(PKVM* vm);
static void initializePrimitiveClasses(PKVM* vm);

void initializeCore(PKVM* vm) {
  initializeBuiltinFunctions(vm);
  initializeCoreModules(vm);
  initializePrimitiveClasses(vm);
}

/*****************************************************************************/
/* CORE BUILTIN FUNCTIONS                                                    */
/*****************************************************************************/

DEF(coreTypeName,
  "type_name(value:var) -> string\n"
  "Returns the type name of the of the value.") {

  RET(VAR_OBJ(newString(vm, varTypeName(ARG(1)))));
}

DEF(coreHelp,
  "help([fn:Closure]) -> null\n"
  "This will write an error message to stdout and return null.") {

  int argc = ARGC;
  if (argc != 0 && argc != 1) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (argc == 0) {
    // If there ins't an io function callback, we're done.
    if (vm->config.write_fn == NULL) RET(VAR_NULL);
    vm->config.write_fn(vm, "TODO: print help here\n");

  } else if (argc == 1) {

    // TODO: Extend help() to work with modules and classes.
    //       Add docstring (like python) to support it in pocketlang.

    Closure* closure;
    if (!validateArgClosure(vm, 1, &closure)) return;

    // If there ins't an io function callback, we're done.
    if (vm->config.write_fn == NULL) RET(VAR_NULL);

    if (closure->fn->docstring != NULL) {
      vm->config.write_fn(vm, closure->fn->docstring);
      vm->config.write_fn(vm, "\n\n");
    } else {
      vm->config.write_fn(vm, "function '");
      vm->config.write_fn(vm, closure->fn->name);
      vm->config.write_fn(vm, "()' doesn't have a docstring.\n");
    }
  }
}

DEF(coreAssert,
  "assert(condition:bool [, msg:string]) -> void\n"
  "If the condition is false it'll terminate the current fiber with the "
  "optional error message") {

  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (!toBool(ARG(1))) {
    String* msg = NULL;

    if (argc == 2) {
      if (AS_OBJ(ARG(2))->type != OBJ_STRING) {
        msg = toString(vm, ARG(2));
      } else {
        msg = (String*)AS_OBJ(ARG(2));
      }
      vmPushTempRef(vm, &msg->_super);
      VM_SET_ERROR(vm, stringFormat(vm, "Assertion failed: '@'.", msg));
      vmPopTempRef(vm);
    } else {
      VM_SET_ERROR(vm, newString(vm, "Assertion failed."));
    }
  }
}

DEF(coreBin,
  "bin(value:num) -> string\n"
  "Returns as a binary value string with '0x' prefix.") {

  int64_t value;
  if (!validateInteger(vm, ARG(1), &value, "Argument 1")) return;

  char buff[STR_BIN_BUFF_SIZE];

  bool negative = (value < 0) ? true : false;
  if (negative) value = -value;

  char* ptr = buff + STR_BIN_BUFF_SIZE - 1;
  *ptr-- = '\0'; // NULL byte at the end of the string.

  if (value != 0) {
    while (value > 0) {
      *ptr-- = '0' + (value & 1);
      value >>= 1;
    }
  } else {
    *ptr-- = '0';
  }

  *ptr-- = 'b'; *ptr-- = '0';
  if (negative) *ptr-- = '-';

  uint32_t length = (uint32_t)((buff + STR_BIN_BUFF_SIZE - 1) - (ptr + 1));
  RET(VAR_OBJ(newStringLength(vm, ptr + 1, length)));
}

DEF(coreHex,
  "hex(value:num) -> string\n"
  "Returns as a hexadecimal value string with '0x' prefix.") {

  int64_t value;
  if (!validateInteger(vm, ARG(1), &value, "Argument 1")) return;

  char buff[STR_HEX_BUFF_SIZE];

  char* ptr = buff;
  if (value < 0) *ptr++ = '-';
  *ptr++ = '0'; *ptr++ = 'x';

  if (value > UINT32_MAX || value < -(int64_t)(UINT32_MAX)) {
    VM_SET_ERROR(vm, newString(vm, "Integer is too large."));
    RET(VAR_NULL);
  }

  // TODO: sprintf limits only to 8 character hex value, we need to do it
  // outself for a maximum of 16 character long (see bin() for reference).
  uint32_t _x = (uint32_t)((value < 0) ? -value : value);
  int length = sprintf(ptr, "%x", _x);

  RET(VAR_OBJ(newStringLength(vm, buff,
    (uint32_t)((ptr + length) - (char*)(buff)))));
}

DEF(coreYield,
  "yield([value]) -> var\n"
  "Return the current function with the yield [value] to current running "
  "fiber. If the fiber is resumed, it'll run from the next statement of the "
  "yield() call. If the fiber resumed with with a value, the return value of "
  "the yield() would be that value otherwise null.") {

  int argc = ARGC;
  if (argc > 1) { // yield() or yield(val).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  vmYieldFiber(vm, (argc == 1) ? &ARG(1) : NULL);
}

DEF(coreToString,
  "to_string(value:var) -> string\n"
  "Returns the string representation of the value.") {

  RET(VAR_OBJ(toString(vm, ARG(1))));
}

DEF(corePrint,
  "print(...) -> void\n"
  "Write each argument as space seperated, to the stdout and ends with a "
  "newline.") {

  // If the host application doesn't provide any write function, discard the
  // output.
  if (vm->config.write_fn == NULL) return;

  for (int i = 1; i <= ARGC; i++) {
    if (i != 1) vm->config.write_fn(vm, " ");
    vm->config.write_fn(vm, toString(vm, ARG(i))->data);
  }

  vm->config.write_fn(vm, "\n");
}

DEF(coreInput,
  "input([msg:var]) -> string\n"
  "Read a line from stdin and returns it without the line ending. Accepting "
  "an optional argument [msg] and prints it before reading.") {

  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  // If the host application doesn't provide any write function, return.
  if (vm->config.read_fn == NULL) return;

  if (argc == 1) {
    vm->config.write_fn(vm, toString(vm, ARG(1))->data);
  }

  PkStringPtr result = vm->config.read_fn(vm);
  String* line = newString(vm, result.string);
  if (result.on_done) result.on_done(vm, result);
  RET(VAR_OBJ(line));
}

DEF(coreExit,
  "exit([value:num]) -> null\n"
  "Exit the process with an optional exit code provided by the argument "
  "[value]. The default exit code is would be 0.") {

  int argc = ARGC;
  if (argc > 1) { // exit() or exit(val).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  int64_t value = 0;
  if (argc == 1) {
    if (!validateInteger(vm, ARG(1), &value, "Argument 1")) return;
  }

  // TODO: this actually needs to be the VM fiber being set to null though.
  exit((int)value);
}

// String functions.
// -----------------

DEF(coreStrSub,
  "str_sub(str:string, pos:num, len:num) -> string\n"
  "Returns a substring from a given string supplied. In addition, "
  "the position and length of the substring are provided when this "
  "function is called. For example: `str_sub(str, pos, len)`.") {

  String* str;
  int64_t pos, len;

  if (!validateArgString(vm, 1, &str)) return;
  if (!validateInteger(vm, ARG(2), &pos, "Argument 2")) return;
  if (!validateInteger(vm, ARG(3), &len, "Argument 3")) return;

  if (pos < 0 || str->length < pos)
    RET_ERR(newString(vm, "Index out of range."));

  if (str->length < pos + len)
    RET_ERR(newString(vm, "Substring length exceeded the limit."));

  // Edge case, empty string.
  if (len == 0) RET(VAR_OBJ(newStringLength(vm, "", 0)));

  RET(VAR_OBJ(newStringLength(vm, str->data + pos, (uint32_t)len)));
}

DEF(coreStrChr,
  "str_chr(value:num) -> string\n"
  "Returns the ASCII string value of the integer argument.") {

  int64_t num;
  if (!validateInteger(vm, ARG(1), &num, "Argument 1")) return;

  if (!IS_NUM_BYTE(num)) {
    RET_ERR(newString(vm, "The number is not in a byte range."));
  }

  char c = (char)num;
  RET(VAR_OBJ(newStringLength(vm, &c, 1)));
}

DEF(coreStrOrd,
  "str_ord(value:string) -> num\n"
  "Returns integer value of the given ASCII character.") {

  String* c;
  if (!validateArgString(vm, 1, &c)) return;
  if (c->length != 1) {
    RET_ERR(newString(vm, "Expected a string of length 1."));

  } else {
    RET(VAR_NUM((double)c->data[0]));
  }
}

// List functions.
// ---------------

DEF(coreListAppend,
  "list_append(self:List, value:var) -> List\n"
  "Append the [value] to the list [self] and return the list.") {

  List* list;
  if (!validateArgList(vm, 1, &list)) return;
  Var elem = ARG(2);

  listAppend(vm, list, elem);
  RET(VAR_OBJ(list));
}

// TODO: currently it takes one argument (to test string interpolation).
//       Add join delimeter as an optional argument.
DEF(coreListJoin,
  "list_join(self:List) -> String\n"
  "Concatinate the elements of the list and return as a string.") {

  List* list;
  if (!validateArgList(vm, 1, &list)) return;

  pkByteBuffer buff;
  pkByteBufferInit(&buff);

  for (uint32_t i = 0; i < list->elements.count; i++) {
    String* elem = toString(vm, list->elements.data[i]);
    vmPushTempRef(vm, &elem->_super); // elem
    pkByteBufferAddString(&buff, vm, elem->data, elem->length);
    vmPopTempRef(vm); // elem
  }

  String* str = newStringLength(vm, (const char*)buff.data, buff.count);
  pkByteBufferClear(&buff, vm);
  RET(VAR_OBJ(str));
}

// Map functions.
// --------------

DEF(coreMapRemove,
  "map_remove(self:map, key:var) -> var\n"
  "Remove the [key] from the map [self] and return it's value if the key "
  "exists, otherwise it'll return null.") {

  Map* map;
  if (!validateArgMap(vm, 1, &map)) return;
  Var key = ARG(2);

  RET(mapRemoveKey(vm, map, key));
}

static void initializeBuiltinFN(PKVM* vm, Closure** bfn, const char* name,
                                int length, int arity, pkNativeFn ptr,
                                const char* docstring) {
  Function* fn = newFunction(vm, name, length, NULL, true, docstring, NULL);
  fn->arity = arity;
  fn->native = ptr;
  vmPushTempRef(vm, &fn->_super); // fn.
  *bfn = newClosure(vm, fn);
  vmPopTempRef(vm); // fn.
}

static void initializeBuiltinFunctions(PKVM* vm) {
#define INITIALIZE_BUILTIN_FN(name, fn, argc)                        \
  initializeBuiltinFN(vm, &vm->builtins[vm->builtins_count++], name, \
                      (int)strlen(name), argc, fn, DOCSTRING(fn));
  // General functions.
  INITIALIZE_BUILTIN_FN("type_name", coreTypeName, 1);
  INITIALIZE_BUILTIN_FN("help",      coreHelp,    -1);
  INITIALIZE_BUILTIN_FN("assert",    coreAssert,  -1);
  INITIALIZE_BUILTIN_FN("bin",       coreBin,      1);
  INITIALIZE_BUILTIN_FN("hex",       coreHex,      1);
  INITIALIZE_BUILTIN_FN("yield",     coreYield,   -1);
  INITIALIZE_BUILTIN_FN("to_string", coreToString, 1);
  INITIALIZE_BUILTIN_FN("print",     corePrint,   -1);
  INITIALIZE_BUILTIN_FN("input",     coreInput,   -1);
  INITIALIZE_BUILTIN_FN("exit",      coreExit,    -1);

  // String functions.
  INITIALIZE_BUILTIN_FN("str_sub", coreStrSub, 3);
  INITIALIZE_BUILTIN_FN("str_chr", coreStrChr, 1);
  INITIALIZE_BUILTIN_FN("str_ord", coreStrOrd, 1);

  // List functions.
  INITIALIZE_BUILTIN_FN("list_append", coreListAppend, 2);
  INITIALIZE_BUILTIN_FN("list_join",   coreListJoin,   1);

  // Map functions.
  INITIALIZE_BUILTIN_FN("map_remove", coreMapRemove, 2);

#undef INITIALIZE_BUILTIN_FN
}

/*****************************************************************************/
/* CORE MODULE METHODS                                                       */
/*****************************************************************************/

// Create a module and add it to the vm's core modules, returns the module.
static Module* newModuleInternal(PKVM* vm, const char* name) {

  String* _name = newString(vm, name);
  vmPushTempRef(vm, &_name->_super); // _name

  // Check if any module with the same name already exists and assert to the
  // hosting application.
  if (vmGetModule(vm, _name) != NULL) {
    ASSERT(false, stringFormat(vm,
           "A module named '$' already exists", name)->data);
  }

  Module* module = newModule(vm);
  module->name = _name;
  module->initialized = true;
  vmPopTempRef(vm); // _name

  return module;
}

// An internal function to add a function to the given [module].
static void moduleAddFunctionInternal(PKVM* vm, Module* module,
                                      const char* name, pkNativeFn fptr,
                                      int arity, const char* docstring) {

  Function* fn = newFunction(vm, name, (int)strlen(name),
                             module, true, docstring, NULL);
  fn->native = fptr;
  fn->arity = arity;

  vmPushTempRef(vm, &fn->_super); // fn.
  Closure* closure = newClosure(vm, fn);
  moduleAddGlobal(vm, module, name, (uint32_t)strlen(name), VAR_OBJ(closure));
  vmPopTempRef(vm); // fn.
}

// 'lang' library methods.
// -----------------------

DEF(stdLangClock,
  "clock() -> num\n"
  "Returns the number of seconds since the application started") {

  RET(VAR_NUM((double)clock() / CLOCKS_PER_SEC));
}

DEF(stdLangGC,
  "gc() -> num\n"
  "Trigger garbage collection and return the amount of bytes cleaned.") {

  size_t bytes_before = vm->bytes_allocated;
  vmCollectGarbage(vm);
  size_t garbage = bytes_before - vm->bytes_allocated;
  RET(VAR_NUM((double)garbage));
}

DEF(stdLangDisas,
  "disas(fn:Closure) -> String\n"
  "Returns the disassembled opcode of the function [fn].") {

  // TODO: support dissasemble class constructors and module main body.

  Closure* closure;
  if (!validateArgClosure(vm, 1, &closure)) return;

  if (!validateCond(vm, !closure->fn->is_native,
                    "Cannot disassemble native functions.")) return;

  dumpFunctionCode(vm, closure->fn);
}

#ifdef DEBUG
DEF(stdLangDebugBreak,
  "debug_break() -> null\n"
  "A debug function for development (will be removed).") {

  DEBUG_BREAK();
}
#endif

DEF(stdLangWrite,
  "write(...) -> null\n"
  "Write function, just like print function but it wont put space between"
  "args and write a new line at the end.") {

  // If the host application doesn't provide any write function, discard the
  // output.
  if (vm->config.write_fn == NULL) return;

  String* str; //< Will be cleaned by garbage collector;

  for (int i = 1; i <= ARGC; i++) {
    Var arg = ARG(i);
    // If it's already a string don't allocate a new string instead use it.
    if (IS_OBJ_TYPE(arg, OBJ_STRING)) {
      str = (String*)AS_OBJ(arg);
    } else {
      str = toString(vm, arg);
    }

    vm->config.write_fn(vm, str->data);
  }
}

// TODO: Move math to cli as it's not part of the pocketlang core.
//
// 'math' library methods.
// -----------------------

DEF(stdMathFloor,
  "floor(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(floor(num)));
}

DEF(stdMathCeil,
  "ceil(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(ceil(num)));
}

DEF(stdMathPow,
  "pow(value:num) -> num\n") {

  double num, ex;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (!validateNumeric(vm, ARG(2), &ex, "Argument 2")) return;
  RET(VAR_NUM(pow(num, ex)));
}

DEF(stdMathSqrt,
  "sqrt(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(sqrt(num)));
}

DEF(stdMathAbs,
  "abs(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (num < 0) num = -num;
  RET(VAR_NUM(num));
}

DEF(stdMathSign,
  "sign(value:num) -> num\n") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (num < 0) num = -1;
  else if (num > 0) num = +1;
  else num = 0;
  RET(VAR_NUM(num));
}

DEF(stdMathHash,
  "hash(value:var) -> num\n"
  "Return the hash value of the variable, if it's not hashable it'll "
  "return null.") {

  if (IS_OBJ(ARG(1))) {
    if (!isObjectHashable(AS_OBJ(ARG(1))->type)) {
      RET(VAR_NULL);
    }
  }
  RET(VAR_NUM((double)varHashValue(ARG(1))));
}

DEF(stdMathSine,
  "sin(rad:num) -> num\n"
  "Return the sine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(sin(rad)));
}

DEF(stdMathCosine,
  "cos(rad:num) -> num\n"
  "Return the cosine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(cos(rad)));
}

DEF(stdMathTangent,
  "tan(rad:num) -> num\n"
  "Return the tangent value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(tan(rad)));
}

DEF(stdMathSinh,
  "sinh(val) -> val\n"
  "Return the hyperbolic sine value of the argument [val].") {

  double val;
  if (!validateNumeric(vm, ARG(1), &val, "Argument 1")) return;
  RET(VAR_NUM(sinh(val)));
}

DEF(stdMathCosh,
  "cosh(val) -> val\n"
  "Return the hyperbolic cosine value of the argument [val].") {

  double val;
  if (!validateNumeric(vm, ARG(1), &val, "Argument 1")) return;
  RET(VAR_NUM(cosh(val)));
}

DEF(stdMathTanh,
  "tanh(val) -> val\n"
  "Return the hyperbolic tangent value of the argument [val].") {

  double val;
  if (!validateNumeric(vm, ARG(1), &val, "Argument 1")) return;
  RET(VAR_NUM(tanh(val)));
}

DEF(stdMathArcSine,
  "asin(num) -> num\n"
  "Return the arcsine value of the argument [num] which is an angle "
  "expressed in radians.") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;

  if (num < -1 || 1 < num) {
    RET_ERR(newString(vm, "Argument should be between -1 and +1"));
  }

  RET(VAR_NUM(asin(num)));
}

DEF(stdMathArcCosine,
  "acos(num) -> num\n"
  "Return the arc cosine value of the argument [num] which is "
  "an angle expressed in radians.") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;

  if (num < -1 || 1 < num) {
    RET_ERR(newString(vm, "Argument should be between -1 and +1"));
  }

  RET(VAR_NUM(acos(num)));
}

DEF(stdMathArcTangent,
  "atan(num) -> num\n"
  "Return the arc tangent value of the argument [num] which is "
  "an angle expressed in radians.") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(atan(num)));
}

DEF(stdMathLog10,
  "log10(value:num) -> num\n"
  "Return the logarithm to base 10 of argument [value]") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(log10(num)));
}

DEF(stdMathRound,
  "round(value:num) -> num\n"
  "Round to nearest integer, away from zero and return the number.") {

  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  RET(VAR_NUM(round(num)));
}

// 'Fiber' module methods.
// -----------------------

DEF(stdFiberNew,
  "new(fn:Closure) -> fiber\n"
  "Create and return a new fiber from the given function [fn].") {

  Closure* closure;
  if (!validateArgClosure(vm, 1, &closure)) return;
  RET(VAR_OBJ(newFiber(vm, closure)));
}

DEF(stdFiberRun,
  "run(fb:Fiber, ...) -> var\n"
  "Runs the fiber's function with the provided arguments and returns it's "
  "return value or the yielded value if it's yielded.") {

  int argc = ARGC;
  if (argc == 0) // Missing the fiber argument.
    RET_ERR(newString(vm, "Missing argument - fiber."));

  Fiber* fb;
  if (!validateArgFiber(vm, 1, &fb)) return;

  // Buffer of argument to call vmPrepareFiber().
  Var* args[MAX_ARGC];

  // ARG(1) is fiber, function arguments are ARG(2), ARG(3), ... ARG(argc).
  for (int i = 1; i < argc; i++) {
    args[i - 1] = &ARG(i + 1);
  }

  // Switch fiber and start execution.
  if (vmPrepareFiber(vm, fb, argc - 1, args)) {
    ASSERT(fb == vm->fiber, OOPS);
    fb->state = FIBER_RUNNING;
  }
}

DEF(stdFiberResume,
  "resume(fb:Fiber) -> var\n"
  "Resumes a yielded function from a previous call of fiber_run() function. "
  "Return it's return value or the yielded value if it's yielded.") {

  int argc = ARGC;
  if (argc == 0) // Missing the fiber argument.
    RET_ERR(newString(vm, "Expected at least 1 argument(s)."));
  if (argc > 2) // Can only accept 1 argument for resume.
    RET_ERR(newString(vm, "Expected at most 2 argument(s)."));

  Fiber* fb;
  if (!validateArgFiber(vm, 1, &fb)) return;

  Var value = (argc == 1) ? VAR_NULL : ARG(2);

  // Switch fiber and resume execution.
  if (vmSwitchFiber(vm, fb, &value)) {
    ASSERT(fb == vm->fiber, OOPS);
    fb->state = FIBER_RUNNING;
  }
}

static void initializeCoreModules(PKVM* vm) {
#define MODULE_ADD_FN(module, name, fn, argc) \
  moduleAddFunctionInternal(vm, module, name, fn, argc, DOCSTRING(fn))

#define NEW_MODULE(module, name_string)                \
  Module* module = newModuleInternal(vm, name_string); \
  vmPushTempRef(vm, &module->_super); /* module */     \
  vmRegisterModule(vm, module, module->name);          \
  vmPopTempRef(vm) /* module */

  NEW_MODULE(lang, "lang");
  MODULE_ADD_FN(lang, "clock", stdLangClock,  0);
  MODULE_ADD_FN(lang, "gc",    stdLangGC,     0);
  MODULE_ADD_FN(lang, "disas", stdLangDisas,  1);
  MODULE_ADD_FN(lang, "write", stdLangWrite, -1);
#ifdef DEBUG
  MODULE_ADD_FN(lang, "debug_break", stdLangDebugBreak, 0);
#endif

  NEW_MODULE(math, "math");
  MODULE_ADD_FN(math, "floor",  stdMathFloor,      1);
  MODULE_ADD_FN(math, "ceil",   stdMathCeil,       1);
  MODULE_ADD_FN(math, "pow",    stdMathPow,        2);
  MODULE_ADD_FN(math, "sqrt",   stdMathSqrt,       1);
  MODULE_ADD_FN(math, "abs",    stdMathAbs,        1);
  MODULE_ADD_FN(math, "sign",   stdMathSign,       1);
  MODULE_ADD_FN(math, "hash",   stdMathHash,       1);
  MODULE_ADD_FN(math, "sin",    stdMathSine,       1);
  MODULE_ADD_FN(math, "cos",    stdMathCosine,     1);
  MODULE_ADD_FN(math, "tan",    stdMathTangent,    1);
  MODULE_ADD_FN(math, "sinh",   stdMathSinh,       1);
  MODULE_ADD_FN(math, "cosh",   stdMathCosh,       1);
  MODULE_ADD_FN(math, "tanh",   stdMathTanh,       1);
  MODULE_ADD_FN(math, "asin",   stdMathArcSine,    1);
  MODULE_ADD_FN(math, "acos",   stdMathArcCosine,  1);
  MODULE_ADD_FN(math, "atan",   stdMathArcTangent, 1);
  MODULE_ADD_FN(math, "log10",  stdMathLog10,      1);
  MODULE_ADD_FN(math, "round",  stdMathRound,      1);

  // Note that currently it's mutable (since it's a global variable, not
  // constant and pocketlang doesn't support constant) so the user shouldn't
  // modify the PI, like in python.
  moduleAddGlobal(vm, math, "PI", 2, VAR_NUM(M_PI));

  // FIXME:
  // This module should be removed once method implemented on primitive types.
  NEW_MODULE(fiber, "_Fiber");
  MODULE_ADD_FN(fiber, "new",    stdFiberNew,     1);
  MODULE_ADD_FN(fiber, "run",    stdFiberRun,    -1);
  MODULE_ADD_FN(fiber, "resume", stdFiberResume, -1);

#undef MODULE_ADD_FN
#undef NEW_MODULE
}

#undef IS_NUM_BYTE
#undef DOCSTRING
#undef DEF

/*****************************************************************************/
/* PRIMITIVE TYPES CLASS                                                     */
/*****************************************************************************/

static void initializePrimitiveClasses(PKVM* vm) {

  Class* cls_obj = newClass(vm, "Object", 6, NULL, NULL, NULL);
  vm->primitives[0] = cls_obj;

  for (int i = 0; i < OBJ_INST; i++) {
    const char* type_name = getObjectTypeName((ObjectType)i);
    vm->primitives[i + 1] = newClass(vm, type_name, (int)strlen(type_name),
                                     NULL, NULL, NULL);
  }

  // TODO: Add methods to those types.
}

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

#define UNSUPPORTED_OPERAND_TYPES(op)                                  \
  VM_SET_ERROR(vm, stringFormat(vm, "Unsupported operand types for "   \
    "operator '" op "' $ and $", varTypeName(v1), varTypeName(v2)))

#define RIGHT_OPERAND "Right operand"

Var varAdd(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
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
          return VAR_OBJ(stringJoin(vm, (String*)o1, (String*)o2));
        }
      } break;

      case OBJ_LIST:
      {
        if (o2->type == OBJ_LIST) {
          return VAR_OBJ(listJoin(vm, (List*)o1, (List*)o2));
        }
      } break;
    }
  }

  UNSUPPORTED_OPERAND_TYPES("+");
  return VAR_NULL;
}

Var varSubtract(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(d1 - d2);
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("-");
  return VAR_NULL;
}

Var varMultiply(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(d1 * d2);
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("*");
  return VAR_NULL;
}

Var varDivide(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(d1 / d2);
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("/");
  return VAR_NULL;
}

Var varModulo(PKVM* vm, Var v1, Var v2) {
  double d1, d2;

  if (isNumeric(v1, &d1)) {
    if (validateNumeric(vm, v2, &d2, RIGHT_OPERAND)) {
      return VAR_NUM(fmod(d1, d2));
    }
    return VAR_NULL;
  }

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    //const String* str = (const String*)AS_OBJ(v1);
    TODO; // "fmt" % v2.
  }

  UNSUPPORTED_OPERAND_TYPES("%");
  return VAR_NULL;
}

Var varBitAnd(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 & i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("&");
  return VAR_NULL;
}

Var varBitOr(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 | i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("|");
  return VAR_NULL;
}

Var varBitXor(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 ^ i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("^");
  return VAR_NULL;
}

Var varBitLshift(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 << i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES("<<");
  return VAR_NULL;
}

Var varBitRshift(PKVM* vm, Var v1, Var v2) {
  int64_t i1, i2;

  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {
      return VAR_NUM((double)(i1 >> i2));
    }
    return VAR_NULL;
  }

  UNSUPPORTED_OPERAND_TYPES(">>");
  return VAR_NULL;
}

Var varBitNot(PKVM* vm, Var v) {
  int64_t i;
  if (!validateInteger(vm, v, &i, "Unary operand")) return VAR_NULL;
  return VAR_NUM((double)(~i));
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

#undef RIGHT_OPERAND
#undef UNSUPPORTED_OPERAND_TYPES

bool varContains(PKVM* vm, Var elem, Var container) {
  if (!IS_OBJ(container)) {
    VM_SET_ERROR(vm, stringFormat(vm, "'$' is not iterable.",
                 varTypeName(container)));
  }
  Object* obj = AS_OBJ(container);

  switch (obj->type) {
    case OBJ_STRING: {
      if (!IS_OBJ_TYPE(elem, OBJ_STRING)) {
        VM_SET_ERROR(vm, stringFormat(vm, "Expected a string operand."));
        return false;
      }

      String* sub = (String*)AS_OBJ(elem);
      String* str = (String*)AS_OBJ(container);
      if (sub->length > str->length) return false;

      TODO;

    } break;

    case OBJ_LIST: {
      List* list = (List*)AS_OBJ(container);
      for (uint32_t i = 0; i < list->elements.count; i++) {
        if (isValuesEqual(elem, list->elements.data[i])) return true;
      }
      return false;
    } break;

    case OBJ_MAP: {
      Map* map = (Map*)AS_OBJ(container);
      return !IS_UNDEF(mapGet(map, elem));
    } break;
  }

  VM_SET_ERROR(vm, stringFormat(vm, "Argument of type $ is not iterable.",
               varTypeName(container)));
  return VAR_NULL;
}

Var varGetAttrib(PKVM* vm, Var on, String* attrib) {

#define ERR_NO_ATTRIB(vm, on, attrib)                                        \
  VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no attribute named '$'", \
                                varTypeName(on), attrib->data))

  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return VAR_NULL;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING: {
      String* str = (String*)obj;
      switch (attrib->hash) {

        case CHECK_HASH("length", 0x83d03615):
          return VAR_NUM((double)(str->length));

        case CHECK_HASH("lower", 0xb51d04ba):
          return VAR_OBJ(stringLower(vm, str));

        case CHECK_HASH("upper", 0xa8c6a47):
          return VAR_OBJ(stringUpper(vm, str));

        case CHECK_HASH("strip", 0xfd1b18d1):
          return VAR_OBJ(stringStrip(vm, str));
      }
    } break;

    case OBJ_LIST: {
      List* list = (List*)obj;
      switch (attrib->hash) {

        case CHECK_HASH("length", 0x83d03615):
          return VAR_NUM((double)(list->elements.count));
      }
    } break;

    case OBJ_MAP: {
      // map = { "foo" : 42, "can't access" : 32 }
      // val = map.foo ## <-- This should be error
      // Only the map's attributes are accessed here.
      TODO;
    } break;

    case OBJ_RANGE: {
      Range* range = (Range*)obj;
      switch (attrib->hash) {

        case CHECK_HASH("as_list", 0x1562c22):
          return VAR_OBJ(rangeAsList(vm, range));

        // We can't use 'start', 'end' since 'end' in pocketlang is a
        // keyword. Also we can't use 'from', 'to' since 'from' is a keyword
        // too. So, we're using 'first' and 'last' to access the range limits.

        case CHECK_HASH("first", 0x4881d841):
          return VAR_NUM(range->from);

        case CHECK_HASH("last", 0x63e1d819):
          return VAR_NUM(range->to);
      }
    } break;

    case OBJ_MODULE: {
      Module* module = (Module*)obj;

      // Search in globals.
      int index = moduleGetGlobalIndex(module, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, module->globals.count);
        return module->globals.data[index];
      }
    } break;

    case OBJ_FUNC:
      break;

    case OBJ_CLOSURE: {
      Closure* closure = (Closure*)obj;
      switch (attrib->hash) {

        case CHECK_HASH("arity", 0x3e96bd7a):
          return VAR_NUM((double)(closure->fn->arity));

        case CHECK_HASH("name", 0x8d39bde6):
          return VAR_OBJ(newString(vm, closure->fn->name));
      }
    } break;

    case OBJ_UPVALUE:
      UNREACHABLE(); // Upvalues aren't first class objects.
      break;

    case OBJ_FIBER: {
      Fiber* fb = (Fiber*)obj;
      switch (attrib->hash) {

        case CHECK_HASH("is_done", 0x789c2706):
          return VAR_BOOL(fb->state == FIBER_DONE);

        case CHECK_HASH("function", 0x9ed64249):
          return VAR_OBJ(fb->closure);
      }
    } break;

    case OBJ_CLASS:
      TODO;
      break;

    case OBJ_INST: {
      Var value;
      if (instGetAttrib(vm, (Instance*)obj, attrib, &value)) {
        return value;
      }
    } break;
  }

  ERR_NO_ATTRIB(vm, on, attrib);
  return VAR_NULL;

#undef ERR_NO_ATTRIB
}

void varSetAttrib(PKVM* vm, Var on, String* attrib, Var value) {

// Set error for accessing non-existed attribute.
#define ERR_NO_ATTRIB(vm, on, attrib)                               \
  VM_SET_ERROR(vm, stringFormat(vm,                                 \
                   "'$' object has no mutable attribute named '$'", \
                   varTypeName(on), attrib->data))

#define ATTRIB_IMMUTABLE(name)                                                \
do {                                                                          \
  if ((attrib->length == strlen(name) && strcmp(name, attrib->data) == 0)) {  \
    VM_SET_ERROR(vm, stringFormat(vm, "'$' attribute is immutable.", name));  \
    return;                                                                   \
  }                                                                           \
} while (false)

  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      ATTRIB_IMMUTABLE("length");
      ATTRIB_IMMUTABLE("lower");
      ATTRIB_IMMUTABLE("upper");
      ATTRIB_IMMUTABLE("strip");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_LIST:
      ATTRIB_IMMUTABLE("length");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_MAP:
      TODO;
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_RANGE:
      ATTRIB_IMMUTABLE("as_list");
      ATTRIB_IMMUTABLE("first");
      ATTRIB_IMMUTABLE("last");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_MODULE: {
      Module* module = (Module*)obj;
      int index = moduleGetGlobalIndex(module, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, module->globals.count);
        module->globals.data[index] = value;
        return;
      }
    } break;

    case OBJ_FUNC:
      UNREACHABLE(); // Functions aren't first class objects.
      return;

    case OBJ_CLOSURE:
      ATTRIB_IMMUTABLE("arity");
      ATTRIB_IMMUTABLE("name");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_UPVALUE:
      UNREACHABLE(); // Upvalues aren't first class objects.
      return;

    case OBJ_FIBER:
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_CLASS:
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_INST: {
      if (!instSetAttrib(vm, (Instance*)obj, attrib, value)) {
        // If we has error by now, that means the set value type is
        // incompatible. No need for us to set an other error, just return.
        if (VM_HAS_ERROR(vm)) return;
        ERR_NO_ATTRIB(vm, on, attrib);
      }

      // If we reached here, that means the attribute exists and we have
      // updated the value.
      return;
    } break;
  }

  ERR_NO_ATTRIB(vm, on, attrib);
  return;

#undef ATTRIB_IMMUTABLE
#undef ERR_NO_ATTRIB
}

Var varGetSubscript(PKVM* vm, Var on, Var key) {
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return VAR_NULL;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING: {
      int64_t index;
      String* str = ((String*)obj);
      if (!validateInteger(vm, key, &index, "List index")) {
        return VAR_NULL;
      }
      if (!validateIndex(vm, index, str->length, "String")) {
        return VAR_NULL;
      }
      String* c = newStringLength(vm, str->data + index, 1);
      return VAR_OBJ(c);
    } break;

    case OBJ_LIST: {
      int64_t index;
      pkVarBuffer* elems = &((List*)obj)->elements;
      if (!validateInteger(vm, key, &index, "List index")) {
        return VAR_NULL;
      }
      if (!validateIndex(vm, index, elems->count, "List")) {
        return VAR_NULL;
      }
      return elems->data[index];
    } break;

    case OBJ_MAP: {
      Var value = mapGet((Map*)obj, key);
      if (IS_UNDEF(value)) {

        String* key_str = toString(vm, key);
        vmPushTempRef(vm, &key_str->_super);
        if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
          VM_SET_ERROR(vm, stringFormat(vm, "Invalid key '@'.", key_str));
        } else {
          VM_SET_ERROR(vm, stringFormat(vm, "Key '@' not exists", key_str));
        }
        vmPopTempRef(vm);
        return VAR_NULL;
      }
      return value;
    } break;

    case OBJ_FUNC:
    case OBJ_UPVALUE:
      UNREACHABLE(); // Not first class objects.
  }

  VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
               varTypeName(on)));
  return VAR_NULL;
}

void varsetSubscript(PKVM* vm, Var on, Var key, Var value) {
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_LIST: {
      int64_t index;
      pkVarBuffer* elems = &((List*)obj)->elements;
      if (!validateInteger(vm, key, &index, "List index")) return;
      if (!validateIndex(vm, index, elems->count, "List")) return;
      elems->data[index] = value;
      return;
    } break;

    case OBJ_MAP: {
      if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
        VM_SET_ERROR(vm, stringFormat(vm, "$ type is not hashable.",
                                      varTypeName(key)));
      } else {
        mapSet(vm, (Map*)obj, key, value);
      }
      return;
    } break;

    case OBJ_FUNC:
    case OBJ_UPVALUE:
      UNREACHABLE();
  }

  VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
               varTypeName(on)));
  return;
}
