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

/*****************************************************************************/
/* CORE PUBLIC API                                                           */
/*****************************************************************************/

// Create a new module with the given [name] and returns as a Script* for
// internal. Which will be wrapped by pkNewModule to return a pkHandle*.
static Script* newModuleInternal(PKVM* vm, const char* name);

// The internal function to add functions to a module.
static void moduleAddFunctionInternal(PKVM* vm, Script* script,
                                      const char* name, pkNativeFn fptr,
                                      int arity);

// pkNewModule implementation (see pocketlang.h for description).
PkHandle* pkNewModule(PKVM* vm, const char* name) {
  Script* module = newModuleInternal(vm, name);
  return vmNewHandle(vm, VAR_OBJ(module));
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
  // reference to the function name and it can be refactored into a index buffer
  // in an "increasing-name" order which can be used to binary search. Similer
  // for 'global_names' refactor them from VarBuffer to GlobalVarBuffer where
  // GlobalVar is struct { const char* name, Var value };
  // 
  // "nicreasing-name" order index buffer:
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

// A convinent macro to get the nth (1 based) argument of the current function.
#define ARG(n) (vm->fiber->ret[n])

// Convinent macros to get the 1st, 2nd, 3rd arguments.
#define ARG1 ARG(1)
#define ARG2 ARG(2)
#define ARG3 ARG(3)

// Evaluvates to the current function's argument count.
#define ARGC ((int)(vm->fiber->sp - vm->fiber->ret) - 1)

// Set return value for the current native function and return.
#define RET(value)             \
  do {                         \
    *(vm->fiber->ret) = value; \
    return;                    \
  } while (false)

#define RET_ERR(err)           \
  do {                         \
    vm->fiber->error = err;    \
    RET(VAR_NULL);             \
  } while(false)

// Check for errors in before calling the get arg public api function.
#define CHECK_GET_ARG_API_ERRORS()                               \
  do {                                                           \
    __ASSERT(vm->fiber != NULL,                                  \
             "This function can only be called at runtime.");    \
    __ASSERT(arg > 0 || arg <= ARGC, "Invalid argument index."); \
    __ASSERT(value != NULL, "Parameter [value] was NULL.");      \
  } while (false)

// Set error for incompatible type provided as an argument.
#define ERR_INVALID_ARG_TYPE(m_type)                           \
do {                                                           \
  char buff[STR_INT_BUFF_SIZE];                                \
  sprintf(buff, "%d", arg);                                    \
  vm->fiber->error = stringFormat(vm, "Expected a " m_type     \
                                  " at argument $.", buff);    \
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
    vm->fiber->error = stringFormat(vm,
      "Expected a $ at argument $.", getPkVarTypeName(type), buff);
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

// Check if a numeric value bool/number and set [value].
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

// Check if [var] is bool/number. If not set error and return false.
static inline bool validateNumeric(PKVM* vm, Var var, double* value,
                                   const char* name) {
  if (isNumeric(var, value)) return true;
  vm->fiber->error = stringFormat(vm, "$ must be a numeric value.", name);
  return false;
}

// Check if [var] is integer. If not set error and return false.
static inline bool validateInteger(PKVM* vm, Var var, int32_t* value,
                                   const char* name) {
  double number;
  if (isNumeric(var, &number)) {
    double truncated = floor(number);
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

// Check if [var] is string for argument at [arg]. If not set error and
// return false.
#define VALIDATE_ARG_OBJ(m_class, m_type, m_name)                            \
  static bool validateArg##m_class(PKVM* vm, int arg, m_class** value) {     \
    Var var = ARG(arg);                                                      \
    ASSERT(arg > 0 && arg <= ARGC, OOPS);                                    \
    if (!IS_OBJ(var) || AS_OBJ(var)->type != m_type) {                       \
      char buff[12]; sprintf(buff, "%d", arg);                               \
      vm->fiber->error = stringFormat(vm, "Expected a " m_name               \
        " at argument $.", buff, false);                                     \
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
  ASSERT_INDEX(index, vm->builtins_count);
  return vm->builtins[index].fn;
}

// getBuiltinFunctionName implementation (see core.h for description).
const char* getBuiltinFunctionName(const PKVM* vm, int index) {
  ASSERT_INDEX(index, vm->builtins_count);
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
    RET(VAR_BOOL(check(ARG1)));         \
  }

#define FN_IS_OBJ_TYPE(name, _enum)     \
  static void coreIs##name(PKVM* vm) {  \
    Var arg1 = ARG1;                    \
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

PK_DOC(coreTypeName,
  "type_name(value:var) -> string\n"
  "Returns the type name of the of the value.") {
  RET(VAR_OBJ(newString(vm, varTypeName(ARG1))));
}

PK_DOC(coreAssert,
  "assert(condition:bool [, msg:string]) -> void\n"
  "If the condition is false it'll terminate the current fiber with the "
  "optional error message") {
  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (!toBool(ARG1)) {
    String* msg = NULL;

    if (argc == 2) {
      if (AS_OBJ(ARG2)->type != OBJ_STRING) {
        msg = toString(vm, ARG2);
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

PK_DOC(coreYield,
  "yield([value]) -> var\n"
  "Return the current function with the yield [value] to current running "
  "fiber. If the fiber is resumed, it'll run from the next statement of the "
  "yield() call. If the fiber resumed with with a value, the return value of "
  "the yield() would be that value otherwise null.") {

  int argc = ARGC;
  if (argc > 1) { // yield() or yield(val).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  vmYieldFiber(vm, (argc == 1) ? &ARG1 : NULL);
}

PK_DOC(coreToString,
  "to_string(value:var) -> string\n"
  "Returns the string representation of the value.") {
  RET(VAR_OBJ(toString(vm, ARG1)));
}

PK_DOC(corePrint,
  "print(...) -> void\n"
  "Write each argument as comma seperated to the stdout and ends with a "
  "newline.") {
  // If the host appliaction donesn't provide any write function, discard the
  // output.
  if (vm->config.write_fn == NULL) return;

  for (int i = 1; i <= ARGC; i++) {
    if (i != 1) vm->config.write_fn(vm, " ");
    vm->config.write_fn(vm, toString(vm, ARG(i))->data);
  }

  vm->config.write_fn(vm, "\n");
}

PK_DOC(coreInput,
  "input([msg:var]) -> string\n"
  "Read a line from stdin and returns it without the line ending. Accepting "
  "an optional argument [msg] and prints it before reading.") {
  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  // If the host appliaction donesn't provide any write function, return.
  if (vm->config.read_fn == NULL) return;

  if (argc == 1) {
    vm->config.write_fn(vm, toString(vm, ARG1)->data);
  }

  PkStringPtr result = vm->config.read_fn(vm);
  String* line = newString(vm, result.string);
  if (result.on_done) result.on_done(vm, result);
  RET(VAR_OBJ(line));
}

// String functions.
// -----------------
PK_DOC(coreStrLower,
  "str_lower(value:string) -> string\n"
  "Returns a lower-case version of the given string.") {
  String* str;
  if (!validateArgString(vm, 1, &str)) return;

  String* result = newStringLength(vm, str->data, str->length);
  char* data = result->data;
  for (; *data; ++data) *data = (char)tolower(*data);
  // Since the string is modified re-hash it.
  result->hash = utilHashString(result->data);

  RET(VAR_OBJ(result));
}

PK_DOC(coreStrUpper,
  "str_upper(value:string) -> string\n"
  "Returns a upper-case version of the given string.") {
  String* str;
  if (!validateArgString(vm, 1, &str)) return;

  String* result = newStringLength(vm, str->data, str->length);
  char* data = result->data;
  for (; *data; ++data) *data = (char)toupper(*data);
  // Since the string is modified re-hash it.
  result->hash = utilHashString(result->data);
  
  RET(VAR_OBJ(result));
}

PK_DOC(coreStrStrip,
  "str_strip(value:string) -> string\n"
  "Returns a copy of the string as the leading and trailing white spaces are"
  "trimed.") {
  String* str;
  if (!validateArgString(vm, 1, &str)) return;

  const char* start = str->data;
  while (*start && isspace(*start)) start++;
  if (*start == '\0') RET(VAR_OBJ(newStringLength(vm, NULL, 0)));

  const char* end = str->data + str->length - 1;
  while (isspace(*end)) end--;

  RET(VAR_OBJ(newStringLength(vm, start, (uint32_t)(end - start + 1))));
}

PK_DOC(coreStrChr,
  "str_chr(value:number) -> string\n"
  "Returns the ASCII string value of the integer argument.") {
  int32_t num;
  if (!validateInteger(vm, ARG1, &num, "Argument 1")) return;

  char c = (char)num;
  RET(VAR_OBJ(newStringLength(vm, &c, 1)));
}

PK_DOC(coreStrOrd, 
  "str_ord(value:string) -> number\n"
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

PK_DOC(coreListAppend,
  "list_append(self:List, value:var) -> List\n"
  "Append the [value] to the list [self] and return the list.") {
  List* list;
  if (!validateArgList(vm, 1, &list)) return;
  Var elem = ARG(2);

  varBufferWrite(&list->elements, vm, elem);
  RET(VAR_OBJ(list));
}

// Map functions.
// --------------

PK_DOC(coreMapRemove,
  "map_remove(self:map, key:var) -> var\n"
  "Remove the [key] from the map [self] and return it's value if the key "
  "exists, otherwise it'll return null.") {
  Map* map;
  if (!validateArgMap(vm, 1, &map)) return;
  Var key = ARG(2);

  RET(mapRemoveKey(vm, map, key));
}

// Fiber functions.
// ----------------

PK_DOC(coreFiberNew,
  "fiber_new(fn:function) -> fiber\n"
  "Create and return a new fiber from the given function [fn].") {
  Function* fn;
  if (!validateArgFunction(vm, 1, &fn)) return;
  RET(VAR_OBJ(newFiber(vm, fn)));
}

PK_DOC(coreFiberGetFunc,
  "fiber_get_func(fb:fiber) -> function\n"
  "Retruns the fiber's functions. Which is usefull if you wan't to re-run the "
  "fiber, you can get the function and crate a new fiber.") {
  Fiber* fb;
  if (!validateArgFiber(vm, 1, &fb)) return;
  RET(VAR_OBJ(fb->func));
}

PK_DOC(coreFiberIsDone,
  "fiber_is_done(fb:fiber) -> bool\n"
  "Returns true if the fiber [fb] is done running and can no more resumed.") {
  Fiber* fb;
  if (!validateArgFiber(vm, 1, &fb)) return;
  RET(VAR_BOOL(fb->state == FIBER_DONE));
}

PK_DOC(coreFiberRun,
  "fiber_run(fb:fiber, ...) -> var\n"
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

PK_DOC(coreFiberResume,
  "fiber_resume(fb:fiber) -> var\n"
  "Resumes a yielded function from a previous call of fiber_run() function. "
  "Return it's return value or the yielded value if it's yielded." ) {

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

// An internal function to add a function to the given [script].
static void moduleAddFunctionInternal(PKVM* vm, Script* script,
                                      const char* name, pkNativeFn fptr,
                                      int arity) {

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
  if (!validateNumeric(vm, ARG1, &num, "Parameter 1")) return;

  RET(VAR_NUM(floor(num)));
}

void stdMathCeil(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG1, &num, "Parameter 1")) return;

  RET(VAR_NUM(ceil(num)));
}

void stdMathPow(PKVM* vm) {
  double num, ex;
  if (!validateNumeric(vm, ARG1, &num, "Parameter 1")) return;
  if (!validateNumeric(vm, ARG2, &ex, "Parameter 2")) return;

  RET(VAR_NUM(pow(num, ex)));
}

void stdMathSqrt(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG1, &num, "Parameter 1")) return;

  RET(VAR_NUM(sqrt(num)));
}

void stdMathAbs(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG1, &num, "Parameter 1")) return;
  if (num < 0) num = -num;
  RET(VAR_NUM(num));
}

void stdMathSign(PKVM* vm) {
  double num;
  if (!validateNumeric(vm, ARG1, &num, "Parameter 1")) return;
  if (num < 0) num = -1;
  else if (num > 0) num = +1;
  else num = 0;
  RET(VAR_NUM(num));
}

PK_DOC(stdMathHash,
  "hash(value:var) -> num\n"
  "Return the hash value of the variable, if it's not hashable it'll "
  "return null.");
void stdMathHash(PKVM* vm) {
  if (IS_OBJ(ARG1)) {
    if (!isObjectHashable(AS_OBJ(ARG1)->type)) {
      RET(VAR_NULL);
    }
  }
  RET(VAR_NUM((double)varHashValue(ARG1)));
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
  
  INITALIZE_BUILTIN_FN("assert",      coreAssert,    -1);
  INITALIZE_BUILTIN_FN("yield",       coreYield,     -1);
  INITALIZE_BUILTIN_FN("to_string",   coreToString,   1);
  INITALIZE_BUILTIN_FN("print",       corePrint,     -1);
  INITALIZE_BUILTIN_FN("input",       coreInput,     -1);

  // String functions.
  INITALIZE_BUILTIN_FN("str_lower",   coreStrLower,   1);
  INITALIZE_BUILTIN_FN("str_upper",   coreStrUpper,   1);
  INITALIZE_BUILTIN_FN("str_strip",   coreStrStrip,   1);
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
  moduleAddFunctionInternal(vm, lang, "write", stdLangWrite, -1);
#ifdef DEBUG
  moduleAddFunctionInternal(vm, lang, "debug_break", stdLangDebugBreak, 0);
#endif

  Script* math = newModuleInternal(vm, "math");
  moduleAddFunctionInternal(vm, math, "floor", stdMathFloor,  1);
  moduleAddFunctionInternal(vm, math, "ceil",  stdMathCeil,   1);
  moduleAddFunctionInternal(vm, math, "pow",   stdMathPow,    2);
  moduleAddFunctionInternal(vm, math, "sqrt",  stdMathSqrt,   1);
  moduleAddFunctionInternal(vm, math, "abs",   stdMathAbs,    1);
  moduleAddFunctionInternal(vm, math, "sign",  stdMathSign,   1);
  moduleAddFunctionInternal(vm, math, "hash",  stdMathHash,   1);
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
          return VAR_OBJ(stringJoin(vm, (String*)o1, (String*)o2));
        }
      } break;

      case OBJ_LIST:
      {
        if (o2->type == OBJ_LIST) {
          TODO;
        }
      }
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

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    //const String* str = (const String*)AS_OBJ(v1);
    TODO; // "fmt" % v2.
  }

  UNSUPPORT_OPERAND_TYPES("%");
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

// A convinent convenient macro used in varGetAttrib and varSetAttrib.
#define IS_ATTRIB(name) \
  (attrib->length == strlen(name) && strcmp(name, attrib->data) == 0)

// Set error for accessing non-existed attribute.
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
      TODO; // Not sure should I allow this(below).
      //Var value = mapGet((Map*)obj, VAR_OBJ(attrib));
      //if (IS_UNDEF(value)) {
      //  vm->fiber->error = stringFormat(vm, "Key (\"@\") not exists.",
      //                                  attrib);
      //  return VAR_NULL;
      //}
      //return value;
    }

    case OBJ_RANGE:
    {
      Range* range = (Range*)obj;

      if (IS_ATTRIB("as_list")) {
        List* list;
        if (range->from < range->to) {
          list = newList(vm, (uint32_t)(range->to - range->from));
          for (double i = range->from; i < range->to; i++) {
            varBufferWrite(&list->elements, vm, VAR_NUM(i));
          }
        } else {
          list = newList(vm, 0);
        }
        return VAR_OBJ(list);
      }

      ERR_NO_ATTRIB();
      return VAR_NULL;
    }

    case OBJ_SCRIPT: {
      Script* scr = (Script*)obj;

      // Search in functions.
      int index = scriptGetFunc(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX(index, scr->functions.count);
        return VAR_OBJ(scr->functions.data[index]);
      }

      // Search in globals.
      index = scriptGetGlobals(scr, attrib->data, attrib->length);
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
      int index = scriptGetGlobals(scr, attrib->data, attrib->length);
      if (index != -1) {
        ASSERT_INDEX(index, scr->globals.count);
        scr->globals.data[index] = value;
        return;
      }

      // Check function (Functions are immutable).
      index = scriptGetFunc(scr, attrib->data, attrib->length);
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
      int32_t index;
      VarBuffer* elems = &((List*)obj)->elements;
      if (!validateInteger(vm, key, &index, "List index")) {
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

        String* key_str = toString(vm, key);
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
}

void varsetSubscript(PKVM* vm, Var on, Var key, Var value) {
  if (!IS_OBJ(on)) {
    vm->fiber->error = stringFormat(vm, "$ type is not subscriptable.",
                                    varTypeName(on));
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
      if (!validateInteger(vm, key, &index, "List index")) return;
      if (!validateIndex(vm, index, (int)elems->count, "List")) return;
      elems->data[index] = value;
      return;
    }

    case OBJ_MAP:
    {
      if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
        vm->fiber->error = stringFormat(vm, "$ type is not hashable.",
                                        varTypeName(key));
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
  CHECK_MISSING_OBJ_TYPE(7);
  UNREACHABLE();
}
