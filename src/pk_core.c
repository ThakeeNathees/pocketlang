/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "pk_core.h"

#include <ctype.h>
#include <math.h>
#include <time.h>

#include "pk_debug.h"
#include "pk_utils.h"
#include "pk_var.h"
#include "pk_vm.h"

// M_PI is non standard. The macro _USE_MATH_DEFINES defining before importing
// <math.h> will define the constants for MSVC. But for a portable solution,
// we're defining it ourselves if it isn't already.
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

/*****************************************************************************/
/* CORE PUBLIC API                                                           */
/*****************************************************************************/

// Create a new module with the given [name] and returns as a Script* for
// internal. Which will be wrapped by pkNewModule to return a pkHandle*.
static Script* newModuleInternal(PKVM* vm, const char* name);

// The internal function to add global value to a module.
static void moduleAddGlobalInternal(PKVM* vm, Script* script,
                                    const char* name, Var value);

// The internal function to add functions to a module.
static void moduleAddFunctionInternal(PKVM* vm, Script* script,
                                      const char* name, pkNativeFn fptr,
                                      int arity);

// pkNewModule implementation (see pocketlang.h for description).
PkHandle* pkNewModule(PKVM* vm, const char* name) {
  Script* module = newModuleInternal(vm, name);
  return vmNewHandle(vm, VAR_OBJ(module));
}

// pkModuleAddGlobal implementation (see pocketlang.h for description).
PK_PUBLIC void pkModuleAddGlobal(PKVM* vm, PkHandle* module,
                                 const char* name, PkHandle* value) {
  __ASSERT(module != NULL, "Argument module was NULL.");
  __ASSERT(value != NULL, "Argument value was NULL.");
  Var scr = module->value;
  __ASSERT(IS_OBJ_TYPE(scr, OBJ_SCRIPT), "Given handle is not a module");

  moduleAddGlobalInternal(vm, (Script*)AS_OBJ(scr), name, value->value);

}

// pkModuleAddFunction implementation (see pocketlang.h for description).
void pkModuleAddFunction(PKVM* vm, PkHandle* module, const char* name,
                         pkNativeFn fptr, int arity) {
  __ASSERT(module != NULL, "Argument module was NULL.");
  Var scr = module->value;
  __ASSERT(IS_OBJ_TYPE(scr, OBJ_SCRIPT), "Given handle is not a module");
  moduleAddFunctionInternal(vm, (Script*)AS_OBJ(scr), name, fptr, arity);
}

PkHandle* pkGetFunction(PKVM* vm, PkHandle* module,
                                  const char* name) {
  __ASSERT(module != NULL, "Argument module was NULL.");
  Var scr = module->value;
  __ASSERT(IS_OBJ_TYPE(scr, OBJ_SCRIPT), "Given handle is not a module");
  Script* script = (Script*)AS_OBJ(scr);

  // TODO: Currently it's O(n) and could be optimized to O(log(n)) but does it
  //       worth it?
  //
  // 'function_names' buffer is un-necessary since the function itself has the
  // reference to the function name and it can be refactored into a index
  // buffer in an "increasing-name" order which can be used to binary search.
  // Similer for 'global_names' refactor them from VarBuffer to GlobalVarBuffer
  // where GlobalVar is struct { const char* name, Var value };
  //
  // "increasing-name" order index buffer:
  //   A buffer of int where each is an index in the function buffer and each
  //   points to different functions in an "increasing-name" (could be hash
  //   value) order. If we have more than some threshold number of function
  //   use binary search. (remember to skip literal functions).
  for (uint32_t i = 0; i < script->functions.count; i++) {
    const char* fn_name = script->functions.data[i]->name;
    if (strcmp(name, fn_name) == 0) {
      return vmNewHandle(vm, VAR_OBJ(script->functions.data[i]));
    }
  }

  return NULL;
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
#define CHECK_GET_ARG_API_ERRORS()                               \
  do {                                                           \
    __ASSERT(vm->fiber != NULL,                                  \
             "This function can only be called at runtime.");    \
    __ASSERT(arg > 0 || arg <= ARGC, "Invalid argument index."); \
    __ASSERT(value != NULL, "Argument [value] was NULL.");       \
  } while (false)

// Set error for incompatible type provided as an argument.
#define ERR_INVALID_ARG_TYPE(m_type)                             \
do {                                                             \
  char buff[STR_INT_BUFF_SIZE];                                  \
  sprintf(buff, "%d", arg);                                      \
  VM_SET_ERROR(vm, stringFormat(vm, "Expected a " m_type         \
                                " at argument $.", buff));       \
} while (false)

// pkGetArgc implementation (see pocketlang.h for description).
int pkGetArgc(const PKVM* vm) {
  __ASSERT(vm->fiber != NULL, "This function can only be called at runtime.");
  return ARGC;
}

// pkGetArg implementation (see pocketlang.h for description).
PkVar pkGetArg(const PKVM* vm, int arg) {
  __ASSERT(vm->fiber != NULL, "This function can only be called at runtime.");
  __ASSERT(arg > 0 || arg <= ARGC, "Invalid argument index.");

  return &(ARG(arg));
}

// pkGetArgBool implementation (see pocketlang.h for description).
bool pkGetArgBool(PKVM* vm, int arg, bool* value) {
  CHECK_GET_ARG_API_ERRORS();

  Var val = ARG(arg);
  *value = toBool(val);
  return true;
}

// pkGetArgNumber implementation (see pocketlang.h for description).
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

// pkGetArgString implementation (see pocketlang.h for description).
bool pkGetArgString(PKVM* vm, int arg, const char** value) {
  CHECK_GET_ARG_API_ERRORS();

  Var val = ARG(arg);
  if (IS_OBJ_TYPE(val, OBJ_STRING)) {
    *value = ((String*)AS_OBJ(val))->data;

  } else {
    ERR_INVALID_ARG_TYPE("string");
    return false;
  }

  return true;
}

// pkGetArgValue implementation (see pocketlang.h for description).
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

// pkReturnNull implementation (see pocketlang.h for description).
void pkReturnNull(PKVM* vm) {
  RET(VAR_NULL);
}

// pkReturnBool implementation (see pocketlang.h for description).
void pkReturnBool(PKVM* vm, bool value) {
  RET(VAR_BOOL(value));
}

// pkReturnNumber implementation (see pocketlang.h for description).
void pkReturnNumber(PKVM* vm, double value) {
  RET(VAR_NUM(value));
}

// pkReturnString implementation (see pocketlang.h for description).
void pkReturnString(PKVM* vm, const char* value) {
  RET(VAR_OBJ(newString(vm, value)));
}

// pkReturnStringLength implementation (see pocketlang.h for description).
void pkReturnStringLength(PKVM* vm, const char* value, size_t length) {
  RET(VAR_OBJ(newStringLength(vm, value, (uint32_t)length)));
}

// pkReturnValue implementation (see pocketlang.h for description).
void pkReturnValue(PKVM* vm, PkVar value) {
  RET(*(Var*)value);
}

const char* pkStringGetData(const PkVar value) {
  const Var str = (*(const Var*)value);
  __ASSERT(IS_OBJ_TYPE(str, OBJ_STRING), "Value should be of type string.");
  return ((String*)AS_OBJ(str))->data;
}

PkVar pkFiberGetReturnValue(const PkHandle* fiber) {
  __ASSERT(fiber != NULL, "Handle fiber was NULL.");
  Var fb = fiber->value;
  __ASSERT(IS_OBJ_TYPE(fb, OBJ_FIBER), "Given handle is not a fiber");
  Fiber* _fiber = (Fiber*)AS_OBJ(fb);
  return (PkVar)_fiber->ret;
}

bool pkFiberIsDone(const PkHandle* fiber) {
  __ASSERT(fiber != NULL, "Handle fiber was NULL.");
  Var fb = fiber->value;
  __ASSERT(IS_OBJ_TYPE(fb, OBJ_FIBER), "Given handle is not a fiber");
  Fiber* _fiber = (Fiber*)AS_OBJ(fb);
  return _fiber->state == FIBER_DONE;
}

/*****************************************************************************/
/* VALIDATORS                                                                */
/*****************************************************************************/

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
    double floor_val = floor(number);
    if (floor_val == number) {
      *value = (int64_t)(floor_val);
      return true;
    }
  }
  return false;
}

// Check if [var] is bool/number. If not set error and return false.
static inline bool validateNumeric(PKVM* vm, Var var, double* value,
                                   const char* name) {
  if (isNumeric(var, value)) return true;
  VM_SET_ERROR(vm, stringFormat(vm, "$ must be a numeric value.", name));
  return false;
}

// Check if [var] is 32 bit integer. If not set error and return false.
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
    }                                                                        \
    *value = (m_class*)AS_OBJ(var);                                          \
    return true;                                                             \
   }
 VALIDATE_ARG_OBJ(String, OBJ_STRING, "string")
 VALIDATE_ARG_OBJ(List, OBJ_LIST, "list")
 VALIDATE_ARG_OBJ(Map, OBJ_MAP, "map")
 VALIDATE_ARG_OBJ(Function, OBJ_FUNC, "function")
 VALIDATE_ARG_OBJ(Fiber, OBJ_FIBER, "fiber")

/*****************************************************************************/
/* SHARED FUNCTIONS                                                          */
/*****************************************************************************/

// findBuiltinFunction implementation (see core.h for description).
int findBuiltinFunction(const PKVM* vm, const char* name, uint32_t length) {
   for (uint32_t i = 0; i < vm->builtins_count; i++) {
     if (length == vm->builtins[i].length &&
       strncmp(name, vm->builtins[i].name, length) == 0) {
       return i;
     }
   }
   return -1;
 }

// getBuiltinFunction implementation (see core.h for description).
Function* getBuiltinFunction(const PKVM* vm, int index) {
  ASSERT_INDEX((uint32_t)index, vm->builtins_count);
  return vm->builtins[index].fn;
}

// getBuiltinFunctionName implementation (see core.h for description).
const char* getBuiltinFunctionName(const PKVM* vm, int index) {
  ASSERT_INDEX((uint32_t)index, vm->builtins_count);
  return vm->builtins[index].name;
}

// getCoreLib implementation (see core.h for description).
Script* getCoreLib(const PKVM* vm, String* name) {
  Var lib = mapGet(vm->core_libs, VAR_OBJ(name));
  if (IS_UNDEF(lib)) return NULL;
  ASSERT(IS_OBJ_TYPE(lib, OBJ_SCRIPT), OOPS);
  return (Script*)AS_OBJ(lib);
}

/*****************************************************************************/
/* CORE BUILTIN FUNCTIONS                                                    */
/*****************************************************************************/

#define FN_IS_PRIMITE_TYPE(name, check) \
  static void coreIs##name(PKVM* vm) {  \
    RET(VAR_BOOL(check(ARG(1))));       \
  }

#define FN_IS_OBJ_TYPE(name, _enum)     \
  static void coreIs##name(PKVM* vm) {  \
    Var arg1 = ARG(1);                  \
    if (IS_OBJ_TYPE(arg1, _enum)) {     \
      RET(VAR_TRUE);                    \
    } else {                            \
      RET(VAR_FALSE);                   \
    }                                   \
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

PK_DOC(
  "type_name(value:var) -> string\n"
  "Returns the type name of the of the value.",
static void coreTypeName(PKVM* vm)) {
  RET(VAR_OBJ(newString(vm, varTypeName(ARG(1)))));
}

// TODO: Complete this and register it.
PK_DOC(
  "bin(value:num) -> string\n"
  "Returns as a binary value string with '0x' prefix.",
static void coreBin(PKVM* vm)) {
  int64_t value;
  if (!validateInteger(vm, ARG(1), &value, "Argument 1")) return;

  char buff[STR_BIN_BUFF_SIZE];

  char* ptr = buff;
  if (value < 0) *ptr++ = '-';
  *ptr++ = '0'; *ptr++ = 'b';

  TODO; // sprintf(ptr, "%b");
}

PK_DOC(
  "hex(value:num) -> string\n"
  "Returns as a hexadecimal value string with '0x' prefix.",
static void coreHex(PKVM* vm)) {
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

  uint32_t _x = (uint32_t)((value < 0) ? -value : value);
  int length = sprintf(ptr, "%x", _x);

  RET(VAR_OBJ(newStringLength(vm, buff,
              (uint32_t) ((ptr + length) - (char*)(buff)) )));
}

PK_DOC(
  "assert(condition:bool [, msg:string]) -> void\n"
  "If the condition is false it'll terminate the current fiber with the "
  "optional error message",
static void coreAssert(PKVM* vm)) {
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

PK_DOC(
  "yield([value]) -> var\n"
  "Return the current function with the yield [value] to current running "
  "fiber. If the fiber is resumed, it'll run from the next statement of the "
  "yield() call. If the fiber resumed with with a value, the return value of "
  "the yield() would be that value otherwise null.",
static void coreYield(PKVM* vm)) {

  int argc = ARGC;
  if (argc > 1) { // yield() or yield(val).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  vmYieldFiber(vm, (argc == 1) ? &ARG(1) : NULL);
}

PK_DOC(
  "to_string(value:var) -> string\n"
  "Returns the string representation of the value.",
static void coreToString(PKVM* vm)) {
  RET(VAR_OBJ(toString(vm, ARG(1))));
}

PK_DOC(
  "print(...) -> void\n"
  "Write each argument as comma seperated to the stdout and ends with a "
  "newline.",
static void corePrint(PKVM* vm)) {
  // If the host appliaction donesn't provide any write function, discard the
  // output.
  if (vm->config.write_fn == NULL) return;

  for (int i = 1; i <= ARGC; i++) {
    if (i != 1) vm->config.write_fn(vm, " ");
    vm->config.write_fn(vm, toString(vm, ARG(i))->data);
  }

  vm->config.write_fn(vm, "\n");
}

PK_DOC(
  "input([msg:var]) -> string\n"
  "Read a line from stdin and returns it without the line ending. Accepting "
  "an optional argument [msg] and prints it before reading.",
static void coreInput(PKVM* vm)) {
  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  // If the host appliaction donesn't provide any write function, return.
  if (vm->config.read_fn == NULL) return;

  if (argc == 1) {
    vm->config.write_fn(vm, toString(vm, ARG(1))->data);
  }

  PkStringPtr result = vm->config.read_fn(vm);
  String* line = newString(vm, result.string);
  if (result.on_done) result.on_done(vm, result);
  RET(VAR_OBJ(line));
}

// String functions.
// -----------------

// TODO: substring.

PK_DOC(
  "str_chr(value:number) -> string\n"
  "Returns the ASCII string value of the integer argument.",
static void coreStrChr(PKVM* vm)) {
  int64_t num;
  if (!validateInteger(vm, ARG(1), &num, "Argument 1")) return;

  // TODO: validate num is a byte.

  char c = (char)num;
  RET(VAR_OBJ(newStringLength(vm, &c, 1)));
}

PK_DOC(
  "str_ord(value:string) -> number\n"
  "Returns integer value of the given ASCII character.",
static void coreStrOrd(PKVM* vm)) {
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

PK_DOC(
  "list_append(self:List, value:var) -> List\n"
  "Append the [value] to the list [self] and return the list.",
static void coreListAppend(PKVM* vm)) {
  List* list;
  if (!validateArgList(vm, 1, &list)) return;
  Var elem = ARG(2);

  listAppend(vm, list, elem);
  RET(VAR_OBJ(list));
}

// Map functions.
// --------------

PK_DOC(
  "map_remove(self:map, key:var) -> var\n"
  "Remove the [key] from the map [self] and return it's value if the key "
  "exists, otherwise it'll return null.",
static void coreMapRemove(PKVM* vm)) {
  Map* map;
  if (!validateArgMap(vm, 1, &map)) return;
  Var key = ARG(2);

  RET(mapRemoveKey(vm, map, key));
}

// Fiber functions.
// ----------------

PK_DOC(
  "fiber_new(fn:Function) -> fiber\n"
  "Create and return a new fiber from the given function [fn].",
static void coreFiberNew(PKVM* vm)) {
  Function* fn;
  if (!validateArgFunction(vm, 1, &fn)) return;
  RET(VAR_OBJ(newFiber(vm, fn)));
}

PK_DOC(
  "fiber_get_func(fb:Fiber) -> function\n"
  "Retruns the fiber's functions. Which is usefull if you wan't to re-run the "
  "fiber, you can get the function and crate a new fiber.",
static void coreFiberGetFunc(PKVM* vm)) {
  Fiber* fb;
  if (!validateArgFiber(vm, 1, &fb)) return;
  RET(VAR_OBJ(fb->func));
}

PK_DOC(
  "fiber_is_done(fb:Fiber) -> bool\n"
  "Returns true if the fiber [fb] is done running and can no more resumed.",
static void coreFiberIsDone(PKVM* vm)) {
  Fiber* fb;
  if (!validateArgFiber(vm, 1, &fb)) return;
  RET(VAR_BOOL(fb->state == FIBER_DONE));
}

PK_DOC(
  "fiber_run(fb:Fiber, ...) -> var\n"
  "Runs the fiber's function with the provided arguments and returns it's "
  "return value or the yielded value if it's yielded.",
static void coreFiberRun(PKVM* vm)) {

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

PK_DOC(
  "fiber_resume(fb:Fiber) -> var\n"
  "Resumes a yielded function from a previous call of fiber_run() function. "
  "Return it's return value or the yielded value if it's yielded.",
static void coreFiberResume(PKVM* vm)) {

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

/*****************************************************************************/
/* CORE MODULE METHODS                                                       */
/*****************************************************************************/

// Create a module and add it to the vm's core modules, returns the script.
static Script* newModuleInternal(PKVM* vm, const char* name) {

  // Create a new Script for the module.
  String* _name = newString(vm, name);
  vmPushTempRef(vm, &_name->_super);

  // Check if any module with the same name already exists and assert to the
  // hosting application.
  if (!IS_UNDEF(mapGet(vm->core_libs, VAR_OBJ(_name)))) {
    vmPopTempRef(vm); // _name
    __ASSERT(false, stringFormat(vm,
             "A module named '$' already exists", name)->data);
  }

  Script* scr = newScript(vm, _name);
  scr->moudle = _name;
  vmPopTempRef(vm); // _name

  // Add the script to core_libs.
  vmPushTempRef(vm, &scr->_super);
  mapSet(vm, vm->core_libs, VAR_OBJ(_name), VAR_OBJ(scr));
  vmPopTempRef(vm);

  return scr;
}

// This will fail an assertion if a function or a global with the [name]
// already exists in the module.
static inline void assertModuleNameDef(PKVM* vm, Script* script,
                                       const char* name) {
  // Check if function with the same name already exists.
  if (scriptGetFunc(script, name, (uint32_t)strlen(name)) != -1) {
    __ASSERT(false, stringFormat(vm, "A function named '$' already esists "
      "on module '@'", name, script->moudle)->data);
  }

  // Check if a global variable with the same name already exists.
  if (scriptGetGlobals(script, name, (uint32_t)strlen(name)) != -1) {
    __ASSERT(false, stringFormat(vm, "A global variable named '$' already "
      "esists on module '@'", name, script->moudle)->data);
  }
}

// The internal function to add global value to a module.
static void moduleAddGlobalInternal(PKVM* vm, Script* script,
                                    const char* name, Var value) {

  // Ensure the name isn't predefined.
  assertModuleNameDef(vm, script, name);

  // TODO: move this to pk_var.h and use it in the compilerAddVariable
  // function.
  uint32_t name_index = scriptAddName(script, vm, name,
                                      (uint32_t)strlen(name));
  pkUintBufferWrite(&script->global_names, vm, name_index);
  pkVarBufferWrite(&script->globals, vm, value);
}

// An internal function to add a function to the given [script].
static void moduleAddFunctionInternal(PKVM* vm, Script* script,
                                      const char* name, pkNativeFn fptr,
                                      int arity) {

  // Ensure the name isn't predefined.
  assertModuleNameDef(vm, script, name);

  Function* fn = newFunction(vm, name, (int)strlen(name), script, true);
  fn->native = fptr;
  fn->arity = arity;
}

// TODO: make the below module functions as PK_DOC(name, doc);

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

PK_DOC(
  "disas(fn:Function) -> String\n"
  "Returns the disassembled opcode of the function [fn]. ",
static void stdLangDisas(PKVM* vm)) {
  Function* fn;
  if (!validateArgFunction(vm, 1, &fn)) return;

  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  dumpFunctionCode(vm, fn, &buff);
  String* dump = newString(vm, (const char*)buff.data);
  pkByteBufferClear(&buff, vm);

  RET(VAR_OBJ(dump));
}

// A debug function for development (will be removed).
void stdLangDebugBreak(PKVM* vm) {
  DEBUG_BREAK();
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
    if (IS_OBJ_TYPE(arg, OBJ_STRING)) {
      str = (String*)AS_OBJ(arg);
    } else {
      str = toString(vm, arg);
    }

    vm->config.write_fn(vm, str->data);
  }
}

// 'math' library methods.
// -----------------------

void stdMathFloor(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;

  RET(VAR_NUM(floor(num)));
}

void stdMathCeil(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;

  RET(VAR_NUM(ceil(num)));
}

void stdMathPow(PKVM* vm) {
  double num, ex;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (!validateNumeric(vm, ARG(2), &ex, "Argument 2")) return;

  RET(VAR_NUM(pow(num, ex)));
}

void stdMathSqrt(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;

  RET(VAR_NUM(sqrt(num)));
}

void stdMathAbs(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (num < 0) num = -num;
  RET(VAR_NUM(num));
}

void stdMathSign(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1")) return;
  if (num < 0) num = -1;
  else if (num > 0) num = +1;
  else num = 0;
  RET(VAR_NUM(num));
}

PK_DOC(
  "hash(value:var) -> num\n"
  "Return the hash value of the variable, if it's not hashable it'll "
  "return null.",
static void stdMathHash(PKVM* vm)) {
  if (IS_OBJ(ARG(1))) {
    if (!isObjectHashable(AS_OBJ(ARG(1))->type)) {
      RET(VAR_NULL);
    }
  }
  RET(VAR_NUM((double)varHashValue(ARG(1))));
}

PK_DOC(
  "sin(rad:num) -> num\n"
  "Return the sine value of the argument [rad] which is an angle expressed "
  "in radians.",
static void stdMathSine(PKVM* vm)) {
  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(sin(rad)));
}

PK_DOC(
  "cos(rad:num) -> num\n"
  "Return the cosine value of the argument [rad] which is an angle expressed "
  "in radians.",
static void stdMathCosine(PKVM* vm)) {
  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(cos(rad)));
}

PK_DOC(
  "tan(rad:num) -> num\n"
  "Return the tangent value of the argument [rad] which is an angle expressed "
  "in radians.",
static void stdMathTangent(PKVM* vm)) {
  double rad;
  if (!validateNumeric(vm, ARG(1), &rad, "Argument 1")) return;
  RET(VAR_NUM(tan(rad)));
}

/*****************************************************************************/
/* CORE INITIALIZATION                                                       */
/*****************************************************************************/

static void initializeBuiltinFN(PKVM* vm, BuiltinFn* bfn, const char* name,
                                int length, int arity, pkNativeFn ptr) {
  bfn->name = name;
  bfn->length = length;

  bfn->fn = newFunction(vm, name, length, NULL, true);
  bfn->fn->arity = arity;
  bfn->fn->native = ptr;
}

void initializeCore(PKVM* vm) {

#define INITALIZE_BUILTIN_FN(name, fn, argc)                         \
  initializeBuiltinFN(vm, &vm->builtins[vm->builtins_count++], name, \
                      (int)strlen(name), argc, fn);

  // Initialize builtin functions.
  INITALIZE_BUILTIN_FN("type_name",   coreTypeName,   1);

  // TOOD: (maybe remove is_*() functions) suspend by type_name.
  //       and add is keyword with modules for builtin types
  // ex: val is Num; val is null; val is List; val is Range
  //     List.append(l, e) # List is implicitly imported core module.
  //     String.lower(s)

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

  INITALIZE_BUILTIN_FN("hex",         coreHex,        1);
  INITALIZE_BUILTIN_FN("assert",      coreAssert,    -1);
  INITALIZE_BUILTIN_FN("yield",       coreYield,     -1);
  INITALIZE_BUILTIN_FN("to_string",   coreToString,   1);
  INITALIZE_BUILTIN_FN("print",       corePrint,     -1);
  INITALIZE_BUILTIN_FN("input",       coreInput,     -1);

  // String functions.
  INITALIZE_BUILTIN_FN("str_chr",     coreStrChr,     1);
  INITALIZE_BUILTIN_FN("str_ord",     coreStrOrd,     1);

  // List functions.
  INITALIZE_BUILTIN_FN("list_append", coreListAppend, 2);

  // Map functions.
  INITALIZE_BUILTIN_FN("map_remove",  coreMapRemove,  2);

  // Fiber functions.
  INITALIZE_BUILTIN_FN("fiber_new",      coreFiberNew,     1);
  INITALIZE_BUILTIN_FN("fiber_get_func", coreFiberGetFunc, 1);
  INITALIZE_BUILTIN_FN("fiber_run",      coreFiberRun,    -1);
  INITALIZE_BUILTIN_FN("fiber_is_done",  coreFiberIsDone,  1);
  INITALIZE_BUILTIN_FN("fiber_resume",   coreFiberResume, -1);

  // Core Modules /////////////////////////////////////////////////////////////

  Script* lang = newModuleInternal(vm, "lang");
  moduleAddFunctionInternal(vm, lang, "clock", stdLangClock,  0);
  moduleAddFunctionInternal(vm, lang, "gc",    stdLangGC,     0);
  moduleAddFunctionInternal(vm, lang, "disas", stdLangDisas,  1);
  moduleAddFunctionInternal(vm, lang, "write", stdLangWrite, -1);
#ifdef DEBUG
  moduleAddFunctionInternal(vm, lang, "debug_break", stdLangDebugBreak, 0);
#endif

  Script* math = newModuleInternal(vm, "math");
  moduleAddFunctionInternal(vm, math, "floor", stdMathFloor,   1);
  moduleAddFunctionInternal(vm, math, "ceil",  stdMathCeil,    1);
  moduleAddFunctionInternal(vm, math, "pow",   stdMathPow,     2);
  moduleAddFunctionInternal(vm, math, "sqrt",  stdMathSqrt,    1);
  moduleAddFunctionInternal(vm, math, "abs",   stdMathAbs,     1);
  moduleAddFunctionInternal(vm, math, "sign",  stdMathSign,    1);
  moduleAddFunctionInternal(vm, math, "hash",  stdMathHash,    1);
  moduleAddFunctionInternal(vm, math, "sin",   stdMathSine,    1);
  moduleAddFunctionInternal(vm, math, "cos",   stdMathCosine,  1);
  moduleAddFunctionInternal(vm, math, "tan",   stdMathTangent, 1);

  // TODO: low priority - sinh, cosh, tanh, asin, acos, atan.

  // Note that currently it's mutable (since it's a global variable, not
  // constant and pocketlang doesn't support constant) so the user shouldn't
  // modify the PI, like in python.
  // TODO: at varSetAttrib() we can detect if the user try to change an
  // attribute of a core module and we can throw an error.
  moduleAddGlobalInternal(vm, math, "PI", VAR_NUM(M_PI));

}

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

#define UNSUPPORT_OPERAND_TYPES(op)                                    \
  VM_SET_ERROR(vm, stringFormat(vm, "Unsupported operand types for "   \
    "operator '" op "' $ and $", varTypeName(v1), varTypeName(v2)))

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
          return VAR_OBJ(stringJoin(vm, (String*)o1, (String*)o2));
        }
      } break;

      case OBJ_LIST:
      {
        if (o2->type == OBJ_LIST) {
          List* l1 = (List*)o1, * l2 = (List*)o2;
          TODO;
        }
      } break;

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

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    //const String* str = (const String*)AS_OBJ(v1);
    TODO; // "fmt" % v2.
  }

  UNSUPPORT_OPERAND_TYPES("%");
  return VAR_NULL;
}

Var varBitAnd(PKVM* vm, Var v1, Var v2) {

  int64_t i1, i2;
  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, "Right operand")) {
      return VAR_NUM((double)(i1 & i2));
    }
    return VAR_NULL;
  }

  UNSUPPORT_OPERAND_TYPES("&");
  return VAR_NULL;
}

Var varBitOr(PKVM* vm, Var v1, Var v2) {

  int64_t i1, i2;
  if (isInteger(v1, &i1)) {
    if (validateInteger(vm, v2, &i2, "Right operand")) {
      return VAR_NUM(i1 | i2);
    }
    return VAR_NULL;
  }

  UNSUPPORT_OPERAND_TYPES("|");
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

// Here we're switching the FNV-1a hash value of the name (cstring). Which is
// an efficient way than having multiple if (attrib == "name"). From O(n) * k
// to O(1) where n is the length of the string and k is the number of string
// comparison.
//
// ex:
//     SWITCH_ATTRIB(str) { // str = "length"
//       CASE_ATTRIB("length", 0x83d03615) : { return string->length; }
//     }
//
// In C++11 this can be achieved (in a better way) with user defined literals
// and constexpr. (Reference from my previous compiler written in C++).
// https://github.com/ThakeeNathees/carbon/blob/89b11800132cbfeedcac0c992593afb5f0357236/include/core/internal.h#L174-L180
// https://github.com/ThakeeNathees/carbon/blob/454d087f85f7fb9408eb0bc10ae702b8de844648/src/var/_string.cpp#L60-L77
//
// However there is a python script that's matching the CASE_ATTRIB() macro
// calls and validate if the string and the hash values are matching.
// TODO: port it to the CI/CD process at github actions.
//
#define SWITCH_ATTRIB(name) switch (utilHashString(name))
#define CASE_ATTRIB(name, hash) case hash
#define CASE_DEFAULT default

// Set error for accessing non-existed attribute.
#define ERR_NO_ATTRIB(vm, on, attrib)                                        \
  VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no attribute named '$'", \
                                varTypeName(on), attrib->data))

Var varGetAttrib(PKVM* vm, Var on, String* attrib) {

  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return VAR_NULL;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
    {
      String* str = (String*)obj;
      SWITCH_ATTRIB(attrib->data) {

        CASE_ATTRIB("length", 0x83d03615) :
          return VAR_NUM((double)(str->length));

        CASE_ATTRIB("lower", 0xb51d04ba) :
          return VAR_OBJ(stringLower(vm, str));

        CASE_ATTRIB("upper", 0xa8c6a47) :
          return VAR_OBJ(stringUpper(vm, str));

        CASE_ATTRIB("strip", 0xfd1b18d1) :
          return VAR_OBJ(stringStrip(vm, str));

        CASE_DEFAULT:
          ERR_NO_ATTRIB(vm, on, attrib);
          return VAR_NULL;
      }

      UNREACHABLE();
    }

    case OBJ_LIST:
    {
      List* list = (List*)obj;
      SWITCH_ATTRIB(attrib->data) {

        CASE_ATTRIB("length", 0x83d03615) :
          return VAR_NUM((double)(list->elements.count));

        CASE_DEFAULT:
          ERR_NO_ATTRIB(vm, on, attrib);
          return VAR_NULL;
      }

      UNREACHABLE();
    }

    case OBJ_MAP:
    {
      // Not sure should I allow string values could be accessed with
      // this way. ex:
      // map = { "foo" : 42, "can't access" : 32 }
      // val = map.foo ## 42
      TODO;
    }

    case OBJ_RANGE:
    {
      Range* range = (Range*)obj;
      SWITCH_ATTRIB(attrib->data) {

        CASE_ATTRIB("as_list", 0x1562c22) :
          return VAR_OBJ(rangeAsList(vm, range));

        CASE_DEFAULT:
          ERR_NO_ATTRIB(vm, on, attrib);
          return VAR_NULL;
      }

      UNREACHABLE();
    }

    case OBJ_SCRIPT: {
      Script* scr = (Script*)obj;

      // Search in functions.
      int index = scriptGetFunc(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->functions.count);
        return VAR_OBJ(scr->functions.data[index]);
      }

      // Search in globals.
      index = scriptGetGlobals(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->globals.count);
        return scr->globals.data[index];
      }

      ERR_NO_ATTRIB(vm, on, attrib);
      return VAR_NULL;
    }

    case OBJ_FUNC:
    {
      Function* fn = (Function*)obj;
      SWITCH_ATTRIB(attrib->data) {

        CASE_ATTRIB("arity", 0x3e96bd7a) :
          return VAR_NUM((double)(fn->arity));

        CASE_ATTRIB("name", 0x8d39bde6) :
          return VAR_OBJ(newString(vm, fn->name));

        CASE_DEFAULT:
          ERR_NO_ATTRIB(vm, on, attrib);
          return VAR_NULL;
      }
      UNREACHABLE();
    }

    case OBJ_FIBER:
    case OBJ_USER:
      TODO;

    default:
      UNREACHABLE();
  }

  UNREACHABLE();
}

void varSetAttrib(PKVM* vm, Var on, String* attrib, Var value) {

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
      // Not sure should I allow string values could be accessed with
      // this way. ex:
      // map = { "foo" : 42, "can't access" : 32 }
      // map.foo = 'bar'
      TODO;

      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_RANGE:
      ATTRIB_IMMUTABLE("as_list");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_SCRIPT: {
      Script* scr = (Script*)obj;

      // Check globals.
      int index = scriptGetGlobals(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->globals.count);
        scr->globals.data[index] = value;
        return;
      }

      // Check function (Functions are immutable).
      index = scriptGetFunc(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX((uint32_t)index, scr->functions.count);
        ATTRIB_IMMUTABLE(scr->functions.data[index]->name);
        return;
      }

      ERR_NO_ATTRIB(vm, on, attrib);
      return;
    }

    case OBJ_FUNC:
      ATTRIB_IMMUTABLE("arity");
      ATTRIB_IMMUTABLE("name");
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_FIBER:
      ERR_NO_ATTRIB(vm, on, attrib);
      return;

    case OBJ_USER:
      TODO; //ERR_NO_ATTRIB(vm, on, attrib);
      return;

    default:
      UNREACHABLE();
  }
  UNREACHABLE();

#undef ATTRIB_IMMUTABLE
}

#undef SWITCH_ATTRIB
#undef CASE_ATTRIB
#undef CASE_DEFAULT
#undef ERR_NO_ATTRIB

Var varGetSubscript(PKVM* vm, Var on, Var key) {
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return VAR_NULL;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
    {
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
    }

    case OBJ_LIST:
    {
      int64_t index;
      pkVarBuffer* elems = &((List*)obj)->elements;
      if (!validateInteger(vm, key, &index, "List index")) {
        return VAR_NULL;
      }
      if (!validateIndex(vm, index, elems->count, "List")) {
        return VAR_NULL;
      }
      return elems->data[index];
    }

    case OBJ_MAP:
    {
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

  UNREACHABLE();
}

void varsetSubscript(PKVM* vm, Var on, Var key, Var value) {
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
                                  varTypeName(on)));
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      VM_SET_ERROR(vm, newString(vm, "String objects are immutable."));
      return;

    case OBJ_LIST:
    {
      int64_t index;
      pkVarBuffer* elems = &((List*)obj)->elements;
      if (!validateInteger(vm, key, &index, "List index")) return;
      if (!validateIndex(vm, index, elems->count, "List")) return;
      elems->data[index] = value;
      return;
    }

    case OBJ_MAP:
    {
      if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
        VM_SET_ERROR(vm, stringFormat(vm, "$ type is not hashable.",
                                      varTypeName(key)));
      } else {
        mapSet(vm, (Map*)obj, key, value);
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

  UNREACHABLE();
}
