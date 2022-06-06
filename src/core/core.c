/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <ctype.h>
#include <limits.h>
#include <math.h>

#ifndef PK_AMALGAMATED
#include "core.h"
#include "debug.h"
#include "utils.h"
#include "vm.h"
#endif

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
  VM_SET_ERROR(vm, stringFormat(vm, "$ must be an Integer.", name));
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
 VALIDATE_ARG_OBJ(Module, OBJ_MODULE, "module")

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

void initializeModule(PKVM* vm, Module* module, bool is_main) {

  String *path = module->path, *name = NULL;

  if (is_main) {
    // TODO: consider static string "@main" stored in PKVM. to reduce
    // allocations everytime here.
    name = newString(vm, "@main");
    module->name = name;
    vmPushTempRef(vm, &name->_super); // _main.
  } else {
    ASSERT(module->name != NULL, OOPS);
    name = module->name;
  }

  ASSERT(name != NULL, OOPS);

  // A script's path will always the absolute normalized path (the path
  // resolving function would do take care of it) which is something that
  // was added after python 3.9.
  if (path != NULL) {
    moduleSetGlobal(vm, module, "__file__", 8, VAR_OBJ(path));
  }

  moduleSetGlobal(vm, module, "_name", 5, VAR_OBJ(name));

  if (is_main) vmPopTempRef(vm); // _main.
}

/*****************************************************************************/
/* INTERNAL FUNCTIONS                                                        */
/*****************************************************************************/

String* varToString(PKVM* vm, Var self, bool repr) {
  if (IS_OBJ_TYPE(self, OBJ_INST)) {

    // The closure is retrieved from [self] thus, it doesn't need to be push
    // on the VM's temp references (since [self] should already be protected
    // from GC).
    Closure* closure = NULL;

    bool has_method = false;
    if (!repr) {
      String* name = newString(vm, LITS__str); // TODO: static vm string?.
      vmPushTempRef(vm, &name->_super); // name.
      has_method = hasMethod(vm, self, name, &closure);
      vmPopTempRef(vm); // name.
    }

    if (!has_method) {
      String* name = newString(vm, LITS__repr); // TODO: static vm string?.
      vmPushTempRef(vm, &name->_super); // name.
      has_method = hasMethod(vm, self, name, &closure);
      vmPopTempRef(vm); // name.
    }

    if (has_method) {
      Var ret = VAR_NULL;
      PkResult result = vmCallMethod(vm, self, closure, 0, NULL, &ret);
      if (result != PK_RESULT_SUCCESS) return NULL;

      if (!IS_OBJ_TYPE(ret, OBJ_STRING)) {
        VM_SET_ERROR(vm, newString(vm, "method " LITS__str " returned "
                                       "non-string type."));
        return NULL;
      }

      return (String*)AS_OBJ(ret);
    }

    // If we reached here, it doesn't have a to string override. just
    // "fall throught" and call 'toString()' bellow.
  }

  if (repr) return toRepr(vm, self);
  return toString(vm, self);
}

// Calls a unary operator overload method. If the method does not exists it'll
// return false, otherwise it'll call the method and return true. If any error
// occures it'll set an error.
static inline bool _callUnaryOpMethod(PKVM* vm, Var self,
                                      const char* method_name, Var* ret) {
  Closure* closure = NULL;
  String* name = newString(vm, method_name);
  vmPushTempRef(vm, &name->_super); // name.
  bool has_method = hasMethod(vm, self, name, &closure);
  vmPopTempRef(vm); // name.

  if (!has_method) return false;

  vmCallMethod(vm, self, closure, 0, NULL, ret);
  return true;
}

// Calls a binary operator overload method. If the method does not exists it'll
// return false, otherwise it'll call the method and return true. If any error
// occures it'll set an error.
static inline bool _callBinaryOpMethod(PKVM* vm, Var self, Var other,
                                      const char* method_name, Var* ret) {
  Closure* closure = NULL;
  String* name = newString(vm, method_name);
  vmPushTempRef(vm, &name->_super); // name.
  bool has_method = hasMethod(vm, self, name, &closure);
  vmPopTempRef(vm); // name.

  if (!has_method) return false;

  vmCallMethod(vm, self, closure, 1, &other, ret);
  return true;
}

/*****************************************************************************/
/* REFLECTION AND HELPER FUNCTIONS                                           */
/*****************************************************************************/

// Add all the methods recursively to the lits used for generating a list of
// attributes for the 'dir()' function.
static void _collectMethods(PKVM* vm, List* list, Class* cls) {
  if (cls == NULL) return;

  for (uint32_t i = 0; i < cls->methods.count; i++) {
    listAppend(vm, list,
      VAR_OBJ(newString(vm, cls->methods.data[i]->fn->name)));
  }
  _collectMethods(vm, list, cls->super_class);
}

/*****************************************************************************/
/* CORE BUILTIN FUNCTIONS                                                    */
/*****************************************************************************/

DEF(coreHelp,
  "help([value:Closure|MethodBind|Class]) -> Null",
  "It'll print the docstring the object and return.") {

  int argc = ARGC;
  if (argc != 0 && argc != 1) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (argc == 0) {
    // If there ins't an io function callback, we're done.
    if (vm->config.stdout_write == NULL) RET(VAR_NULL);
    vm->config.stdout_write(vm, "TODO: print help here\n");

  } else if (argc == 1) {

    // TODO: Extend help() to work with modules and classes.
    //       Add docstring (like python) to support it in pocketlang.

    if (vm->config.stdout_write == NULL) RET(VAR_NULL);
    Var value = ARG(1);

    if (IS_OBJ_TYPE(value, OBJ_CLOSURE)) {
      Closure* closure = (Closure*) AS_OBJ(value);
      // If there ins't an io function callback, we're done.

      if (closure->fn->docstring != NULL) {
        vm->config.stdout_write(vm, closure->fn->docstring);
        vm->config.stdout_write(vm, "\n\n");
      } else {
        vm->config.stdout_write(vm, "function '");
        vm->config.stdout_write(vm, closure->fn->name);
        vm->config.stdout_write(vm, "()' doesn't have a docstring.\n");
      }
    } else if (IS_OBJ_TYPE(value, OBJ_METHOD_BIND)) {
      MethodBind* mb = (MethodBind*) AS_OBJ(value);
      // If there ins't an io function callback, we're done.

      if (mb->method->fn->docstring != NULL) {
        vm->config.stdout_write(vm, mb->method->fn->docstring);
        vm->config.stdout_write(vm, "\n\n");
      } else {
        vm->config.stdout_write(vm, "method '");
        vm->config.stdout_write(vm, mb->method->fn->name);
        vm->config.stdout_write(vm, "()' doesn't have a docstring.\n");
      }
    } else if (IS_OBJ_TYPE(value, OBJ_CLASS)) {
      Class* cls = (Class*) AS_OBJ(value);
      if (cls->docstring != NULL) {
        vm->config.stdout_write(vm, cls->docstring);
        vm->config.stdout_write(vm, "\n\n");
      } else {
        vm->config.stdout_write(vm, "class '");
        vm->config.stdout_write(vm, cls->name->data);
        vm->config.stdout_write(vm, "' doesn't have a docstring.\n");
      }
    } else {
      RET_ERR(newString(vm, "Expected a Closure, MethodBind or "
                        "Class to get help."));
    }
  }

}

DEF(coreDir,
  "dir(v:Var) -> List[String]",
  "It'll return all the elements of the variable [v]. If [v] is a module "
  "it'll return the names of globals, functions, and classes. If it's an "
  "instance it'll return all the attributes and methods.") {

  Var v = ARG(1);
  switch (getVarType(v)) {

    case PK_NULL:
    case PK_BOOL:
    case PK_NUMBER:
    case PK_STRING:
    case PK_LIST:
    case PK_MAP:
    case PK_RANGE:
    case PK_CLOSURE:
    case PK_FIBER: {
      List* list = newList(vm, 8);
      vmPushTempRef(vm, &list->_super); // list.
      _collectMethods(vm, list, getClass(vm, v));
      vmPopTempRef(vm); // list.
      RET(VAR_OBJ(list));
    }

    case PK_MODULE: {
      Module* m = (Module*) AS_OBJ(v);
      List* list = newList(vm, 8);
      vmPushTempRef(vm, &list->_super); // list.
      for (uint32_t i = 0; i < m->globals.count; i++) {
        Var name = m->constants.data[m->global_names.data[i]];
        ASSERT(IS_OBJ_TYPE(name, OBJ_STRING), OOPS);
        listAppend(vm, list, name);
      }
      vmPopTempRef(vm); // list.
      RET(VAR_OBJ(list));
    } break;

    case PK_CLASS: {
      Class* cls = (Class*) AS_OBJ(v);
      List* list = newList(vm, 8);
      vmPushTempRef(vm, &list->_super); // list.
      _collectMethods(vm, list, cls);
      // TODO: if we add static variables to classes it should be
      // added here as well.
      vmPopTempRef(vm); // list.
      RET(VAR_OBJ(list));
    } break;

    case PK_INSTANCE: {
      Instance* inst = (Instance*) AS_OBJ(v);
      List* list = newList(vm, 8);
      vmPushTempRef(vm, &list->_super); // list.
      for (uint32_t i = 0; i < inst->attribs->capacity; i++) {
        Var key = (inst->attribs->entries + i)->key;
        if (!IS_UNDEF(key)) {
          ASSERT(IS_OBJ_TYPE(key, OBJ_STRING), OOPS);
          listAppend(vm, list, key);
        }
      }
      _collectMethods(vm, list, inst->cls);
      vmPopTempRef(vm); // list.
      RET(VAR_OBJ(list));
    } break;
  }

  UNREACHABLE();
}

DEF(coreAssert,
  "assert(condition:Bool [, msg:String]) -> Null",
  "If the condition is false it'll terminate the current fiber with the "
  "optional error message") {

  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (!toBool(ARG(1))) {
    String* msg = NULL;

    if (argc == 2) {
      if (!IS_OBJ_TYPE(ARG(2), OBJ_STRING)) {
        msg = varToString(vm, ARG(2), false);
        if (msg == NULL) return; //< Error at _to_string override.

      } else {
        msg = (String*)AS_OBJ(ARG(2));
      }

      vmPushTempRef(vm, &msg->_super); // msg.
      VM_SET_ERROR(vm, stringFormat(vm, "Assertion failed: '@'.", msg));
      vmPopTempRef(vm); // msg.
    } else {
      VM_SET_ERROR(vm, newString(vm, "Assertion failed."));
    }
  }
}

DEF(coreBin,
  "bin(value:Number) -> String",
  "Returns as a binary value string with '0b' prefix.") {

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
  "hex(value:Number) -> String",
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
  "yield([value:Var]) -> Var",
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
  "str(valueVar) -> String",
  "Returns the string representation of the value.") {

  String* str = varToString(vm, ARG(1), false);
  if (str == NULL) RET(VAR_NULL);
  RET(VAR_OBJ(str));
}

DEF(coreChr,
  "chr(value:Num) -> String",
  "Returns the ASCII string value of the integer argument.") {

  int64_t num;
  if (!validateInteger(vm, ARG(1), &num, "Argument 1")) return;

  if (!(0 <= num && num <= 0xff)) {
    RET_ERR(newString(vm, "The number should be in range 0x00 to 0xff."));
  }

  char c = (char) num;
  RET(VAR_OBJ(newStringLength(vm, &c, 1)));
}

DEF(coreOrd,
  "ord(value:String) -> Number",
  "Returns integer value of the given ASCII character.") {

  String* c;
  if (!validateArgString(vm, 1, &c)) return;
  if (c->length != 1) {
    RET_ERR(newString(vm, "Expected a string of length 1."));

  } else {
    RET(VAR_NUM((double)c->data[0]));
  }
}

DEF(coreMin,
  "min(a:Var, b:Var) -> Bool",
  "Returns minimum of [a] and [b].") {

  Var a = ARG(1), b = ARG(2);
  Var islesser = varLesser(vm, a, b);
  if (VM_HAS_ERROR(vm)) RET(VAR_NULL);

  if (toBool(islesser)) RET(a);
  RET(b);
}

DEF(coreMax,
  "max(a:var, b:var) -> Bool",
  "Returns maximum of [a] and [b].") {

  Var a = ARG(1), b = ARG(2);
  Var islesser = varLesser(vm, a, b);
  if (VM_HAS_ERROR(vm)) RET(VAR_NULL);

  if (toBool(islesser)) RET(b);
  RET(a);
}

DEF(corePrint,
  "print(...) -> Null",
  "Write each argument as space seperated, to the stdout and ends with a "
  "newline.") {

  // If the host application doesn't provide any write function, discard the
  // output.
  if (vm->config.stdout_write == NULL) return;

  for (int i = 1; i <= ARGC; i++) {
    if (i != 1) vm->config.stdout_write(vm, " ");
    String* str = varToString(vm, ARG(i), false);
    if (str == NULL) RET(VAR_NULL);
    vm->config.stdout_write(vm, str->data);
  }

  vm->config.stdout_write(vm, "\n");
}

DEF(coreInput,
  "input([msg:Var]) -> String",
  "Read a line from stdin and returns it without the line ending. Accepting "
  "an optional argument [msg] and prints it before reading.") {

  int argc = ARGC;
  if (argc > 1) { // input() or input(str).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  // If the host application doesn't provide any write function, return.
  if (vm->config.stdin_read == NULL) return;

  if (argc == 1) {
    String* str = varToString(vm, ARG(1), false);
    if (str == NULL) RET(VAR_NULL);
    vm->config.stdout_write(vm, str->data);
  }

  char* str = vm->config.stdin_read(vm);
  if (str == NULL) { //< Input failed !?
    RET_ERR(newString(vm, "Input function failed."));
  }

  String* line = newString(vm, str);
  pkRealloc(vm, str, 0);
  RET(VAR_OBJ(line));
}

DEF(coreExit,
  "exit([value:Number]) -> Null",
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

// List functions.
// ---------------

DEF(coreListAppend,
  "list_append(self:List, value:Var) -> List",
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
  "list_join(self:List) -> String",
  "Concatinate the elements of the list and return as a string.") {

  List* list;
  if (!validateArgList(vm, 1, &list)) return;

  pkByteBuffer buff;
  pkByteBufferInit(&buff);

  for (uint32_t i = 0; i < list->elements.count; i++) {
    String* str = varToString(vm, list->elements.data[i], false);
    if (str == NULL) RET(VAR_NULL);
    vmPushTempRef(vm, &str->_super); // elem
    pkByteBufferAddString(&buff, vm, str->data, str->length);
    vmPopTempRef(vm); // elem
  }

  String* str = newStringLength(vm, (const char*)buff.data, buff.count);
  pkByteBufferClear(&buff, vm);
  RET(VAR_OBJ(str));
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
#define INITIALIZE_BUILTIN_FN(name, fn, argc)                              \
  initializeBuiltinFN(vm, &vm->builtins_funcs[vm->builtins_count++], name, \
                      (int)strlen(name), argc, fn, DOCSTRING(fn));
  // General functions.
  INITIALIZE_BUILTIN_FN("help",      coreHelp,    -1);
  INITIALIZE_BUILTIN_FN("dir",       coreDir,      1);
  INITIALIZE_BUILTIN_FN("assert",    coreAssert,  -1);
  INITIALIZE_BUILTIN_FN("bin",       coreBin,      1);
  INITIALIZE_BUILTIN_FN("hex",       coreHex,      1);
  INITIALIZE_BUILTIN_FN("yield",     coreYield,   -1);
  INITIALIZE_BUILTIN_FN("str",       coreToString, 1);
  INITIALIZE_BUILTIN_FN("chr",       coreChr,      1);
  INITIALIZE_BUILTIN_FN("ord",       coreOrd,      1);
  INITIALIZE_BUILTIN_FN("min",       coreMin,      2);
  INITIALIZE_BUILTIN_FN("max",       coreMax,      2);
  INITIALIZE_BUILTIN_FN("print",     corePrint,   -1);
  INITIALIZE_BUILTIN_FN("input",     coreInput,   -1);
  INITIALIZE_BUILTIN_FN("exit",      coreExit,    -1);

  // List functions.
  INITIALIZE_BUILTIN_FN("list_append", coreListAppend, 2);
  INITIALIZE_BUILTIN_FN("list_join",   coreListJoin,   1);

#undef INITIALIZE_BUILTIN_FN
}

/*****************************************************************************/
/* CORE MODULE METHODS                                                       */
/*****************************************************************************/

// Create a module and add it to the vm's core modules, returns the module.
Module* newModuleInternal(PKVM* vm, const char* name) {

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

  initializeModule(vm, module, false);
  return module;
}

// An internal function to add a function to the given [module].
void moduleAddFunctionInternal(PKVM* vm, Module* module,
                               const char* name, pkNativeFn fptr,
                               int arity, const char* docstring) {

  Function* fn = newFunction(vm, name, (int)strlen(name),
                             module, true, docstring, NULL);
  fn->native = fptr;
  fn->arity = arity;

  vmPushTempRef(vm, &fn->_super); // fn.
  Closure* closure = newClosure(vm, fn);
  moduleSetGlobal(vm, module, name, (uint32_t)strlen(name), VAR_OBJ(closure));
  vmPopTempRef(vm); // fn.
}

// 'lang' library methods.

DEF(stdLangGC,
  "lang.gc() -> Number",
  "Trigger garbage collection and return the amount of bytes cleaned.") {

  size_t bytes_before = vm->bytes_allocated;
  vmCollectGarbage(vm);
  size_t garbage = bytes_before - vm->bytes_allocated;
  RET(VAR_NUM((double)garbage));
}

DEF(stdLangDisas,
  "lang.disas(fn:Closure) -> String",
  "Returns the disassembled opcode of the function [fn].") {

  // TODO: support dissasemble class constructors and module main body.

  Closure* closure;
  if (!validateArgClosure(vm, 1, &closure)) return;

  if (!validateCond(vm, !closure->fn->is_native,
                    "Cannot disassemble native functions.")) return;

  dumpFunctionCode(vm, closure->fn);
}

DEF(stdLangBackTrace,
  "lang.backtrace() -> String",
  "Returns the backtrace as a string, each line is formated as "
  "'<function>;<file>;<line>\n'.") {

  // FIXME:
  // All of the bellow code were copied from "debug.c" file, consider
  // refactor the functionality in a way that it's possible to re use them.

  pkByteBuffer bb;
  pkByteBufferInit(&bb);

  Fiber* fiber = vm->fiber;
  ASSERT(fiber != NULL, OOPS);

  while (fiber) {

    for (int i = fiber->frame_count - 1; i >= 0; i--) {
      CallFrame* frame = &fiber->frames[i];
      const Function* fn = frame->closure->fn;

      // After fetching the instruction the ip will be inceased so we're
      // reducing it by 1. But stack overflows are occure before executing
      // any instruction of that function, so the instruction_index possibly
      // be -1 (set it to zero in that case).
      int instruction_index = (int)(frame->ip - fn->fn->opcodes.data) - 1;
      if (instruction_index == -1) instruction_index = 0;
      int line = fn->fn->oplines.data[instruction_index];

      // Note that path can be null.
      const char* path = (fn->owner->path) ? fn->owner->path->data : "<?>";
      const char* fn_name = (fn->name) ? fn->name : "<?>";

      pkByteBufferAddStringFmt(&bb, vm, "%s;%s;%i\n", fn_name, path, line);
    }

    if (fiber->caller) fiber = fiber->caller;
    else fiber = fiber->native;
  }

  // bb.count not including the null byte and which is the length.
  String* bt = newStringLength(vm, bb.data, bb.count);
  vmPushTempRef(vm, &bt->_super); // bt.
  pkByteBufferClear(&bb, vm);
  vmPopTempRef(vm); // bt.

  RET(VAR_OBJ(bt));
}

DEF(stdLangModules,
  "lang.modules() -> List",
  "Returns the list of all registered modules.") {

  List* list = newList(vm, 8);
  vmPushTempRef(vm, &list->_super); // list.
  for (uint32_t i = 0; i < vm->modules->capacity; i++) {
    if (!IS_UNDEF(vm->modules->entries[i].key)) {
      Var entry = vm->modules->entries[i].value;
      ASSERT(IS_OBJ_TYPE(entry, OBJ_MODULE), OOPS);
      Module* module = (Module*) AS_OBJ(entry);
      ASSERT(module->name != NULL, OOPS);
      if (module->name->data[0] == SPECIAL_NAME_CHAR) {
        continue;
      }
      listAppend(vm, list, entry);
    }
  }
  vmPopTempRef(vm); // list.
  RET(VAR_OBJ(list));
}

#ifdef DEBUG
DEF(stdLangDebugBreak,
  "lang.debug_break() -> Null",
  "A debug function for development (will be removed).") {

  DEBUG_BREAK();
}
#endif

static void initializeCoreModules(PKVM* vm) {
#define MODULE_ADD_FN(module, name, fn, argc) \
  moduleAddFunctionInternal(vm, module, name, fn, argc, DOCSTRING(fn))

#define NEW_MODULE(module, name_string)                \
  Module* module = newModuleInternal(vm, name_string); \
  vmPushTempRef(vm, &module->_super); /* module */     \
  vmRegisterModule(vm, module, module->name);          \
  vmPopTempRef(vm) /* module */

  NEW_MODULE(lang, "lang");
  MODULE_ADD_FN(lang, "gc", stdLangGC, 0);
  MODULE_ADD_FN(lang, "disas", stdLangDisas, 1);
  MODULE_ADD_FN(lang, "backtrace", stdLangBackTrace, 0);
  MODULE_ADD_FN(lang, "modules", stdLangModules, 0);
#ifdef DEBUG
  MODULE_ADD_FN(lang, "debug_break", stdLangDebugBreak, 0);
#endif

#undef MODULE_ADD_FN
#undef NEW_MODULE
}

/*****************************************************************************/
/* BUILTIN CLASS CONSTRUCTORS                                                */
/*****************************************************************************/

static void _ctorNull(PKVM* vm) {
  RET(VAR_NULL);
}

static void _ctorBool(PKVM* vm) {
  RET(VAR_BOOL(toBool(ARG(1))));
}

static void _ctorNumber(PKVM* vm) {
  double value;

  if (isNumeric(ARG(1), &value)) {
    RET(VAR_NUM(value));
  }

  if (IS_OBJ_TYPE(ARG(1), OBJ_STRING)) {
    String* str = (String*) AS_OBJ(ARG(1));
    const char* err = utilToNumber(str->data, &value);
    if (err == NULL) RET(VAR_NUM(value));
    VM_SET_ERROR(vm, newString(vm, err));
    RET(VAR_NULL);
  }

  VM_SET_ERROR(vm, newString(vm, "Argument must be numeric or string."));
}

static void _ctorString(PKVM* vm) {
  if (!pkCheckArgcRange(vm, ARGC, 0, 1)) return;
  if (ARGC == 0) {
    RET(VAR_OBJ(newStringLength(vm, NULL, 0)));
    return;
  }
  String* str = varToString(vm, ARG(1), false);
  if (str == NULL) RET(VAR_NULL);
  RET(VAR_OBJ(str));
}

static void _ctorList(PKVM* vm) {
  List* list = newList(vm, ARGC);
  vmPushTempRef(vm, &list->_super); // list.
  for (int i = 0; i < ARGC; i++) {
    listAppend(vm, list, ARG(i + 1));
  }
  vmPopTempRef(vm); // list.
  RET(VAR_OBJ(list));
}

static void _ctorMap(PKVM* vm) {
  RET(VAR_OBJ(newMap(vm)));
}

static void _ctorRange(PKVM* vm) {
  double from, to;
  if (!validateNumeric(vm, ARG(1), &from, "Argument 1")) return;
  if (!validateNumeric(vm, ARG(2), &to, "Argument 2")) return;

  RET(VAR_OBJ(newRange(vm, from, to)));
}

static void _ctorFiber(PKVM* vm) {
  Closure* closure;
  if (!validateArgClosure(vm, 1, &closure)) return;
  RET(VAR_OBJ(newFiber(vm, closure)));
}

/*****************************************************************************/
/* BUILTIN CLASS METHODS                                                     */
/*****************************************************************************/

#define SELF (vm->fiber->self)

DEF(_objTypeName,
  "Object.typename() -> String",
  "Returns the type name of the object.") {
  RET(VAR_OBJ(newString(vm, varTypeName(SELF))));
}

DEF(_objRepr,
  "Object._repr() -> String",
  "Returns the repr string of the object.") {
  RET(VAR_OBJ(toRepr(vm, SELF)));
}

DEF(_numberTimes,
  "Number.times(f:Closure)",
  "Iterate the function [f] n times. Here n is the integral value of the "
  "number. If the number is not an integer the floor value will be taken.") {

  ASSERT(IS_NUM(SELF), OOPS);
  double n = AS_NUM(SELF);

  Closure* closure;
  if (!validateArgClosure(vm, 1, &closure)) return;

  for (int64_t i = 0; i < n; i++) {
    Var _i = VAR_NUM((double)i);
    PkResult result = vmCallFunction(vm, closure, 1, &_i, NULL);
    if (result != PK_RESULT_SUCCESS) break;
  }

  RET(VAR_NULL);
}

DEF(_numberIsint,
    "Number.isint() -> Bool",
    "Returns true if the number is a whold number, otherwise false.") {
  double n = AS_NUM(SELF);
  RET(VAR_BOOL(floor(n) == n));
}

DEF(_numberIsbyte,
  "Number.isbyte() -> bool",
  "Returns true if the number is an integer and is between 0x00 and 0xff.") {
  double n = AS_NUM(SELF);
  RET(VAR_BOOL((floor(n) == n) && (0x00 <= n && n <= 0xff)));
}

DEF(_stringFind,
  "String.find(sub:String[, start:Number=0]) -> Number",
  "Returns the first index of the substring [sub] found from the "
  "[start] index") {

  if (!pkCheckArgcRange(vm, ARGC, 1, 2)) return;

  String* sub;
  if (!validateArgString(vm, 1, &sub)) return;

  int64_t start = 0;
  if (ARGC == 2) {
    if (!validateInteger(vm, ARG(2), &start, "Argument 1")) return;
  }

  String* self = (String*) AS_OBJ(SELF);

  if (self->length <= start) {
    RET(VAR_NUM((double) -1));
  }

  // FIXME: Pocketlang strings can contain \x00 ie. NULL byte and strstr
  // doesn't support them. However pocketlang strings always ends with a null
  // byte so the match won't go outside of the string.
  const char* match = strstr(self->data + start, sub->data);

  if (match == NULL) RET(VAR_NUM((double) -1));

  ASSERT_INDEX(match - self->data, self->capacity);
  RET(VAR_NUM((double) (match - self->data)));
}

DEF(_stringReplace,
  "String.replace(old:Sttring, new:String[, count:Number=-1]) -> String",
  "Returns a copy of the string where [count] occurrence of the substring "
  "[old] will be replaced with [new]. If [count] == -1 all the occurrence "
  "will be replaced.") {

  if (!pkCheckArgcRange(vm, ARGC, 2, 3)) return;

  String *old, *new_;
  if (!validateArgString(vm, 1, &old)) return;
  if (!validateArgString(vm, 2, &new_)) return;

  String* self = (String*) AS_OBJ(SELF);

  int64_t count = -1;
  if (ARGC == 3) {
    if (!validateInteger(vm, ARG(3), &count, "Argument 3")) return;
    if (count < 0 && count != -1) {
      RET_ERR(newString(vm, "count should either be >= 0 or -1"));
    }
  }

  RET(VAR_OBJ(stringReplace(vm, self, old, new_, (int32_t) count)));
}

DEF(_stringSplit,
  "String.split(sep:String) -> List",
  "Split the string into a list of string seperated by [sep] delimeter.") {

  String* sep;
  if (!validateArgString(vm, 1, &sep)) return;

  if (sep->length == 0) {
    RET_ERR(newString(vm, "Cannot use empty string as a seperator."));
  }

  RET(VAR_OBJ(stringSplit(vm, (String*)AS_OBJ(SELF), sep)));
}

DEF(_stringStrip,
  "String.strip() -> String",
  "Returns a copy of the string where the leading and trailing whitespace "
  "removed.") {
  RET(VAR_OBJ(stringStrip(vm, (String*) AS_OBJ(SELF))));
}

DEF(_stringLower,
  "String.lower() -> String",
  "Returns a copy of the string where all the characters are converted to "
  "lower case letters.") {
  RET(VAR_OBJ(stringLower(vm, (String*) AS_OBJ(SELF))));
}

DEF(_stringUpper,
  "String.lower() -> String",
  "Returns a copy of the string where all the characters are converted to "
  "upper case letters.") {
  RET(VAR_OBJ(stringUpper(vm, (String*) AS_OBJ(SELF))));
}

DEF(_stingStartswith,
  "String.startswith(prefix: String | List) -> Bool",
  "Returns true if the string starts the specified prefix.") {

  Var prefix = ARG(1);
  String* self = (String*) AS_OBJ(SELF);

  if (IS_OBJ_TYPE(prefix, OBJ_STRING)) {
    String* pre = (String*) AS_OBJ(prefix);
    if (pre->length > self->length) RET(VAR_FALSE);
    RET(VAR_BOOL((strncmp(self->data, pre->data, pre->length) == 0)));

  } else if (IS_OBJ_TYPE(prefix, OBJ_LIST)) {

    List* prefixes = (List*) AS_OBJ(prefix);
    for (uint32_t i = 0; i < prefixes->elements.count; i++) {
      Var pre_var = prefixes->elements.data[i];
      if (!IS_OBJ_TYPE(pre_var, OBJ_STRING)) {
        RET_ERR(newString(vm, "Expected a String for prefix."));
      }
      String* pre = (String*) AS_OBJ(pre_var);
      if (pre->length > self->length) RET(VAR_FALSE);
      if (strncmp(self->data, pre->data, pre->length) == 0) RET(VAR_TRUE);
    }
    RET(VAR_FALSE);

  } else {
    RET_ERR(newString(vm, "Expected a String or a List of prifiexes."));
  }
}

DEF(_stingEndswith,
  "String.endswith(suffix: String | List) -> Bool",
  "Returns true if the string ends with the specified suffix.") {

  Var suffix = ARG(1);
  String* self = (String*)AS_OBJ(SELF);

  if (IS_OBJ_TYPE(suffix, OBJ_STRING)) {
    String* suf = (String*)AS_OBJ(suffix);
    if (suf->length > self->length) RET(VAR_FALSE);

    const char* start = (self->data + (self->length - suf->length));
    RET(VAR_BOOL((strncmp(start, suf->data, suf->length) == 0)));

  } else if (IS_OBJ_TYPE(suffix, OBJ_LIST)) {

    List* suffixes = (List*)AS_OBJ(suffix);
    for (uint32_t i = 0; i < suffixes->elements.count; i++) {
      Var suff_var = suffixes->elements.data[i];
      if (!IS_OBJ_TYPE(suff_var, OBJ_STRING)) {
        RET_ERR(newString(vm, "Expected a String for suffix."));
      }
      String* suf = (String*)AS_OBJ(suff_var);
      if (suf->length > self->length) RET(VAR_FALSE);

      const char* start = (self->data + (self->length - suf->length));
      if (strncmp(start, suf->data, suf->length) == 0) RET(VAR_TRUE);
    }
    RET(VAR_FALSE);

  } else {
    RET_ERR(newString(vm, "Expected a String or a List of suffixes."));
  }
}

DEF(_listAppend,
  "List.append(value:Var) -> List",
  "Append the [value] to the list and return the List.") {

  ASSERT(IS_OBJ_TYPE(SELF, OBJ_LIST), OOPS);

  listAppend(vm, ((List*) AS_OBJ(SELF)), ARG(1));
  RET(SELF);
}

DEF(_listInsert,
  "List.insert(index:Number, value:Var) -> Null",
  "Insert the element at the given index. The index should be "
  "0 <= index <= list.length.") {

  List* self = (List*)AS_OBJ(SELF);

  int64_t index;
  if (!validateInteger(vm, ARG(1), &index, "Argument 1")) return;

  if (index < 0 || index > self->elements.count) {
    RET_ERR(newString(vm, "List.insert index out of bounds."));
  }

  listInsert(vm, self, (uint32_t) index, ARG(2));
}

DEF(_listPop,
  "List.pop(index:Number=-1) -> Var",
  "Removes the last element of the list and return it.") {

  ASSERT(IS_OBJ_TYPE(SELF, OBJ_LIST), OOPS);
  List* self = (List*) AS_OBJ(SELF);

  if (!pkCheckArgcRange(vm, ARGC, 0, 1)) return;

  if (self->elements.count == 0) {
    RET_ERR(newString(vm, "Cannot pop from an empty list."));
  }

  int64_t index = -1;
  if (ARGC == 1) {
    if (!validateInteger(vm, ARG(1), &index, "Argument 1")) return;
  }
  if (index < 0) index = self->elements.count + index;

  if (index < 0 || index >= self->elements.count) {
    RET_ERR(newString(vm, "List.pop index out of bounds."));
  }
  RET(listRemoveAt(vm, self, (uint32_t) index));
}

DEF(_listFind,
  "List.find(value:Var) -> Number",
  "Find the value and return its index. If the vlaue not exists "
  "it'll return -1.") {

  ASSERT(IS_OBJ_TYPE(SELF, OBJ_LIST), OOPS);
  List* self = (List*)AS_OBJ(SELF);

  Var* it = self->elements.data;
  if (it == NULL) RET(VAR_NUM(-1)); // Empty list.

  for (; it < self->elements.data + self->elements.count; it++) {
    if (isValuesEqual(*it, ARG(1))) {
      RET(VAR_NUM((double) (it - self->elements.data)));
    }
  }

  RET(VAR_NUM(-1));
}

DEF(_listClear,
  "List.clear() -> Null",
  "Removes all the entries in the list.") {
  listClear(vm, (List*) AS_OBJ(SELF));
}

DEF(_mapClear,
  "Map.clear() -> Null",
  "Removes all the entries in the map.") {
  Map* self = (Map*) AS_OBJ(SELF);
  mapClear(vm, self);
}

DEF(_mapGet,
  "Map.get(key:Var, default=Null) -> Var",
  "Returns the key if its in the map, otherwise the default value will "
  "be returned.") {

  if (!pkCheckArgcRange(vm, ARGC, 1, 2)) return;

  Var default_ = (ARGC == 1) ? VAR_NULL : ARG(2);

  Map* self = (Map*) AS_OBJ(SELF);

  Var value = mapGet(self, ARG(1));
  if (IS_UNDEF(value)) RET(default_);
  RET(value);
}

DEF(_mapHas,
  "Map.has(key:Var) -> Bool",
  "Returns true if the key exists.") {

  Map* self = (Map*)AS_OBJ(SELF);
  Var value = mapGet(self, ARG(1));
  RET(VAR_BOOL(!IS_UNDEF(value)));
}

DEF(_mapPop,
  "Map.pop(key:Var) -> Var",
  "Pops the value at the key and return it.") {

  Map* self = (Map*)AS_OBJ(SELF);
  Var value = mapRemoveKey(vm, self, ARG(1));
  if (IS_UNDEF(value)) {
    RET_ERR(stringFormat(vm, "Key '@' does not exists.", toRepr(vm, ARG(1))));
  }
  RET(value);
}

DEF(_methodBindBind,
  "MethodBind.bind(instance:Var) -> MethodBind",
  "Bind the method to the instance and the method bind will be returned. The "
  "method should be a valid method of the instance. ie. the instance's "
  "interitance tree should contain the method.") {

  MethodBind* self = (MethodBind*) AS_OBJ(SELF);

  // We can only bind the method if the instance has that method.
  String* method_name = newString(vm, self->method->fn->name);
  vmPushTempRef(vm, &method_name->_super); // method_name.

  Var instance = ARG(1);

  Closure* method;
  if (!hasMethod(vm, instance, method_name, &method)
                     || method != self->method) {
    VM_SET_ERROR(vm, newString(vm, "Cannot bind method, instance and method "
                                   "types miss-match."));
    return;
  }

  self->instance = instance;
  vmPopTempRef(vm); // method_name.

  RET(SELF);
}

DEF(_classMethods,
  "Class.methods() -> List",
  "Returns a list of unbound MethodBind of the class.") {

  Class* self = (Class*) AS_OBJ(SELF);

  List* list = newList(vm, self->methods.count);
  vmPushTempRef(vm, &list->_super); // list.
  for (int i = 0; i < (int) self->methods.count; i++) {
    Closure* method = self->methods.data[i];
    ASSERT(method->fn->name, OOPS);
    if (method->fn->name[0] == SPECIAL_NAME_CHAR) continue;
    MethodBind* mb = newMethodBind(vm, method);
    vmPushTempRef(vm, &mb->_super); // mb.
    listAppend(vm, list, VAR_OBJ(mb));
    vmPopTempRef(vm); // mb.
  }
  vmPopTempRef(vm); // list.

  RET(VAR_OBJ(list));

}

DEF(_moduleGlobals,
  "Module.globals() -> List",
  "Returns a list of all the globals in the module. Since classes and "
  "functinos are also globals to a module it'll contain them too.") {

  Module* self = (Module*) AS_OBJ(SELF);

  List* list = newList(vm, self->globals.count);
  vmPushTempRef(vm, &list->_super); // list.
  for (int i = 0; i < (int) self->globals.count; i++) {
    if (moduleGetStringAt(self,
      self->global_names.data[i])->data[0] == SPECIAL_NAME_CHAR) {
      continue;
    }
    listAppend(vm, list, self->globals.data[i]);
  }
  vmPopTempRef(vm); // list.

  RET(VAR_OBJ(list));
}

DEF(_fiberRun,
  "Fiber.run(...) -> Var",
  "Runs the fiber's function with the provided arguments and returns it's "
  "return value or the yielded value if it's yielded.") {

  ASSERT(IS_OBJ_TYPE(SELF, OBJ_FIBER), OOPS);
  Fiber* self = (Fiber*) AS_OBJ(SELF);

  // Switch fiber and start execution. New fibers are marked as running in
  // either it's stats running with vmRunFiber() or here -- inserting a
  // fiber over a running (callee) fiber.
  if (vmPrepareFiber(vm, self, ARGC, &ARG(1))) {
    self->caller = vm->fiber;
    vm->fiber = self;
    self->state = FIBER_RUNNING;
  }
}

DEF(_fiberResume,
  "Fiber.resume() -> Var",
  "Resumes a yielded function from a previous call of fiber_run() function. "
  "Return it's return value or the yielded value if it's yielded.") {

  ASSERT(IS_OBJ_TYPE(SELF, OBJ_FIBER), OOPS);
  Fiber* self = (Fiber*) AS_OBJ(SELF);

  if (!pkCheckArgcRange(vm, ARGC, 0, 1)) return;

  Var value = (ARGC == 1) ? ARG(1) : VAR_NULL;

  // Switch fiber and resume execution.
  if (vmSwitchFiber(vm, self, &value)) {
    self->state = FIBER_RUNNING;
  }
}

#undef SELF

/*****************************************************************************/
/* BUILTIN CLASS INITIALIZATION                                              */
/*****************************************************************************/

static void initializePrimitiveClasses(PKVM* vm) {
  for (int i = 0; i < PK_INSTANCE; i++) {
    Class* super = NULL;
    if (i != 0) super = vm->builtin_classes[PK_OBJECT];
    const char* name = getPkVarTypeName((PkVarType)i);
    Class* cls = newClass(vm, name, (int)strlen(name),
                          super, NULL, NULL, NULL);
    vm->builtin_classes[i] = cls;
    cls->class_of = (PkVarType)i;
  }

#define ADD_CTOR(type, name, ptr, arity_)                    \
  do {                                                       \
    Function* fn = newFunction(vm, name, (int)strlen(name),  \
                               NULL, true, NULL, NULL);      \
    fn->native = ptr;                                        \
    fn->arity = arity_;                                      \
    vmPushTempRef(vm, &fn->_super); /* fn. */                \
    vm->builtin_classes[type]->ctor = newClosure(vm, fn);    \
    vmPopTempRef(vm); /* fn. */                              \
  } while (false)

  ADD_CTOR(PK_NULL,   "@ctorNull",   _ctorNull,    0);
  ADD_CTOR(PK_BOOL,   "@ctorBool",   _ctorBool,    1);
  ADD_CTOR(PK_NUMBER, "@ctorNumber", _ctorNumber,  1);
  ADD_CTOR(PK_STRING, "@ctorString", _ctorString, -1);
  ADD_CTOR(PK_RANGE,  "@ctorRange",  _ctorRange,   2);
  ADD_CTOR(PK_LIST,   "@ctorList",   _ctorList,   -1);
  ADD_CTOR(PK_MAP,    "@ctorMap",    _ctorMap,     0);
  ADD_CTOR(PK_FIBER,  "@ctorFiber",  _ctorFiber,   1);
#undef ADD_CTOR

#define ADD_METHOD(type, name, ptr, arity_)                       \
  do {                                                            \
    Function* fn = newFunction(vm, name, (int)strlen(name),       \
                               NULL, true, DOCSTRING(ptr), NULL); \
    fn->is_method = true;                                         \
    fn->native = ptr;                                             \
    fn->arity = arity_;                                           \
    vmPushTempRef(vm, &fn->_super); /* fn. */                     \
    pkClosureBufferWrite(&vm->builtin_classes[type]->methods,     \
                         vm, newClosure(vm, fn));                 \
    vmPopTempRef(vm); /* fn. */                                   \
  } while (false)

  // TODO: write docs.
  ADD_METHOD(PK_OBJECT, "typename", _objTypeName,    0);
  ADD_METHOD(PK_OBJECT, "_repr",    _objRepr,        0);

  ADD_METHOD(PK_NUMBER, "times",  _numberTimes,     1);
  ADD_METHOD(PK_NUMBER, "isint",  _numberIsint,     0);
  ADD_METHOD(PK_NUMBER, "isbyte", _numberIsbyte,    0);

  ADD_METHOD(PK_STRING, "strip",   _stringStrip,    0);
  ADD_METHOD(PK_STRING, "lower",   _stringLower,    0);
  ADD_METHOD(PK_STRING, "upper",   _stringUpper,    0);
  ADD_METHOD(PK_STRING, "find",    _stringFind,    -1);
  ADD_METHOD(PK_STRING, "replace", _stringReplace, -1);
  ADD_METHOD(PK_STRING, "split",   _stringSplit,    1);
  ADD_METHOD(PK_STRING, "startswith", _stingStartswith, 1);
  ADD_METHOD(PK_STRING, "endswith", _stingEndswith, 1);

  ADD_METHOD(PK_LIST,   "clear",  _listClear,      0);
  ADD_METHOD(PK_LIST,   "find",   _listFind,       1);
  ADD_METHOD(PK_LIST,   "append", _listAppend,     1);
  ADD_METHOD(PK_LIST,   "pop",    _listPop,       -1);
  ADD_METHOD(PK_LIST,   "insert", _listInsert,     2);

  ADD_METHOD(PK_MAP,    "clear",  _mapClear,       0);
  ADD_METHOD(PK_MAP,    "get",    _mapGet,        -1);
  ADD_METHOD(PK_MAP,    "has",    _mapHas,         1);
  ADD_METHOD(PK_MAP,    "pop",    _mapPop,         1);

  ADD_METHOD(PK_METHOD_BIND, "bind", _methodBindBind, 1);

  ADD_METHOD(PK_CLASS, "methods", _classMethods, 0);

  ADD_METHOD(PK_MODULE, "globals", _moduleGlobals, 0);

  ADD_METHOD(PK_FIBER,  "run",    _fiberRun,      -1);
  ADD_METHOD(PK_FIBER,  "resume", _fiberResume,   -1);

#undef ADD_METHOD
}

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

Var preConstructSelf(PKVM* vm, Class* cls) {

#define NO_INSTANCE(type_name)   \
  VM_SET_ERROR(vm, newString(vm, \
               "Class '" type_name "' cannot be instanciated."))

  switch (cls->class_of) {
    case PK_OBJECT:
      NO_INSTANCE("Object");
      return VAR_NULL;

    case PK_NULL:
    case PK_BOOL:
    case PK_NUMBER:
    case PK_STRING:
    case PK_LIST:
    case PK_MAP:
    case PK_RANGE:
      return VAR_NULL; // Constructor will override the null.

    case PK_MODULE:
      NO_INSTANCE("Module");
      return VAR_NULL;

    case PK_CLOSURE:
      NO_INSTANCE("Closure");
      return VAR_NULL;

    case PK_FIBER:
      return VAR_NULL;

    case PK_CLASS:
      NO_INSTANCE("Class");
      return VAR_NULL;

    case PK_INSTANCE:
      return VAR_OBJ(newInstance(vm, cls));
  }

  UNREACHABLE();
  return VAR_NULL;
}

Class* getClass(PKVM* vm, Var instance) {
  PkVarType type = getVarType(instance);
  if (0 <= type && type < PK_INSTANCE) {
    return vm->builtin_classes[type];
  }
  ASSERT(IS_OBJ_TYPE(instance, OBJ_INST), OOPS);
  Instance* inst = (Instance*)AS_OBJ(instance);
  return inst->cls;
}

// Returns a method on a class (it'll walk up the inheritance tree to search
// and if the method not found, it'll return NULL.
static inline Closure* clsGetMethod(Class* cls, String* name) {
  Class* cls_ = cls;
  do {
    for (int i = 0; i < (int)cls_->methods.count; i++) {
      Closure* method_ = cls_->methods.data[i];
      ASSERT(method_->fn->is_method, OOPS);
      const char* method_name = method_->fn->name;
      if (IS_CSTR_EQ(name, method_name, strlen(method_name))) {
        return method_;
      }
    }
    cls_ = cls_->super_class;
  } while (cls_ != NULL);
  return NULL;
}

bool hasMethod(PKVM* vm, Var self, String* name, Closure** _method) {
  Class* cls = getClass(vm, self);
  ASSERT(cls != NULL, OOPS);

  Closure* method_ = clsGetMethod(cls, name);
  if (method_ != NULL) {
    *_method = method_;
    return true;
  }

  return false;
}

Var getMethod(PKVM* vm, Var self, String* name, bool* is_method) {

  Closure* method;
  if (hasMethod(vm, self, name, &method)) {
    if (is_method) *is_method = true;
    return VAR_OBJ(method);
  }

  // If the attribute not found it'll set an error.
  if (is_method) *is_method = false;
  return varGetAttrib(vm, self, name);
}

Closure* getSuperMethod(PKVM* vm, Var self, String* name) {
  Class* super = getClass(vm, self)->super_class;
  if (super == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no parent class.", \
                 varTypeName(self)));
    return NULL;
  };

  Closure* method = clsGetMethod(super, name);
  if (method == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "'@' class has no method named '@'.", \
                 super->name, name));
  }
  return method;
}

#define UNSUPPORTED_UNARY_OP(op)                                   \
  VM_SET_ERROR(vm, stringFormat(vm, "Unsupported operand ($) for " \
               "unary operator " op ".", varTypeName(v)))

#define UNSUPPORTED_BINARY_OP(op)                                    \
  VM_SET_ERROR(vm, stringFormat(vm, "Unsupported operand types for " \
    "operator '" op "' $ and $", varTypeName(v1), varTypeName(v2)))

#define RIGHT_OPERAND "Right operand"

#define CHECK_NUMERIC_OP_AS(op, as)                         \
  do {                                                      \
    double n1, n2;                                          \
    if (isNumeric(v1, &n1)) {                               \
      if (validateNumeric(vm, v2, &n2, RIGHT_OPERAND)) {    \
        return as(n1 op n2);                                \
      }                                                     \
      return VAR_NULL;                                      \
    }                                                       \
  } while (false)

#define CHECK_NUMERIC_OP(op) CHECK_NUMERIC_OP_AS(op, VAR_NUM)

#define CHECK_BITWISE_OP(op)                                \
  do {                                                      \
    int64_t i1, i2;                                         \
    if (isInteger(v1, &i1)) {                               \
      if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) {    \
        return VAR_NUM((double)(i1 op i2));                 \
      }                                                     \
      return VAR_NULL;                                      \
    }                                                       \
  } while (false)

#define CHECK_INST_UNARY_OP(name)                           \
  do {                                                      \
    if (IS_OBJ_TYPE(v, OBJ_INST)) {                         \
      Var result;                                           \
      if (_callUnaryOpMethod(vm, v, name, &result)) {       \
        return result;                                      \
      }                                                     \
    }                                                       \
  } while (false)

#define CHECK_INST_BINARY_OP(name)                          \
  do {                                                      \
    if (IS_OBJ_TYPE(v1, OBJ_INST)) {                        \
      Var result;                                           \
      if (inplace) {                                        \
        if (_callBinaryOpMethod(vm, v1, v2, name "=", &result)) { \
          return result;                                    \
        }                                                   \
      }                                                     \
      if (_callBinaryOpMethod(vm, v1, v2, name, &result)) { \
        return result;                                      \
      }                                                     \
    }                                                       \
  } while(false)

Var varPositive(PKVM* vm, Var v) {
  double n; if (isNumeric(v, &n)) return v;
  CHECK_INST_UNARY_OP("+self");
  UNSUPPORTED_UNARY_OP("unary +");
  return VAR_NULL;
}

Var varNegative(PKVM* vm, Var v) {
  double n; if (isNumeric(v, &n)) return VAR_NUM(-AS_NUM(v));
  CHECK_INST_UNARY_OP("-self");
  UNSUPPORTED_UNARY_OP("unary -");
  return VAR_NULL;
}

Var varNot(PKVM* vm, Var v) {
  CHECK_INST_UNARY_OP("!self");
  return VAR_BOOL(!toBool(v));
}

Var varBitNot(PKVM* vm, Var v) {
  int64_t i;
  if (isInteger(v, &i)) return VAR_NUM((double)(~i));
  CHECK_INST_UNARY_OP("~self");
  UNSUPPORTED_UNARY_OP("unary ~");
  return VAR_NULL;
}

Var varAdd(PKVM* vm, Var v1, Var v2, bool inplace) {

  CHECK_NUMERIC_OP(+);

  if (IS_OBJ(v1)) {
    Object *o1 = AS_OBJ(v1);
    switch (o1->type) {

      case OBJ_STRING: {
        if (!IS_OBJ(v2)) break;
        Object* o2 = AS_OBJ(v2);
        if (o2->type == OBJ_STRING) {
          return VAR_OBJ(stringJoin(vm, (String*)o1, (String*)o2));
        }
      } break;

      case OBJ_LIST: {
        if (!IS_OBJ(v2)) break;
        Object* o2 = AS_OBJ(v2);
        if (o2->type == OBJ_LIST) {
          if (inplace) {
            pkVarBufferConcat(&((List*)o1)->elements, vm,
                              &((List*)o2)->elements);
            return v1;
          } else {
            return VAR_OBJ(listAdd(vm, (List*)o1, (List*)o2));
          }
        }
      } break;

      default: break;
    }
  }
  CHECK_INST_BINARY_OP("+");
  UNSUPPORTED_BINARY_OP("+");
  return VAR_NULL;
}

Var varModulo(PKVM* vm, Var v1, Var v2, bool inplace) {
  double n1, n2;
  if (isNumeric(v1, &n1)) {
    if (validateNumeric(vm, v2, &n2, RIGHT_OPERAND)) {
      return VAR_NUM(fmod(n1, n2));
    }
    return VAR_NULL;
  }

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    TODO; // "fmt" % v2.
  }

  CHECK_INST_BINARY_OP("%");
  UNSUPPORTED_BINARY_OP("%");
  return VAR_NULL;
}

// TODO: the bellow function definitions can be written as macros.

Var varSubtract(PKVM* vm, Var v1, Var v2, bool inplace) {
  CHECK_NUMERIC_OP(-);
  CHECK_INST_BINARY_OP("-");
  UNSUPPORTED_BINARY_OP("-");
  return VAR_NULL;
}

Var varMultiply(PKVM* vm, Var v1, Var v2, bool inplace) {
  CHECK_NUMERIC_OP(*);
  CHECK_INST_BINARY_OP("*");

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    String* left = (String*) AS_OBJ(v1);
    int64_t right;
    if (isInteger(v2, &right)) {
      if (left->length == 0) return VAR_OBJ(left);
      if (right == 0) return VAR_OBJ(newString(vm, ""));

      // In python multiplying with negative number will result an empty
      // string so we're following the same rule here.
      if (right < 0) return VAR_OBJ(newString(vm, ""));

      String* str = newStringLength(vm, "", left->length * (uint32_t) right);
      char* buff = str->data;
      for (int i = 0; i < (int) right; i++) {
        memcpy(buff, left->data, left->length);
        buff += left->length;
      }
      ASSERT(buff == str->data + str->length, OOPS);
      str->hash = utilHashString(str->data);
      return VAR_OBJ(str);
    }
  }

  UNSUPPORTED_BINARY_OP("*");
  return VAR_NULL;
}

Var varDivide(PKVM* vm, Var v1, Var v2, bool inplace) {
  CHECK_NUMERIC_OP(/);
  CHECK_INST_BINARY_OP("/");
  UNSUPPORTED_BINARY_OP("/");
  return VAR_NULL;
}

Var varExponent(PKVM* vm, Var v1, Var v2, bool inplace) {
  double n1, n2;
  if (isNumeric(v1, &n1)) {
    if (validateNumeric(vm, v2, &n2, RIGHT_OPERAND)) {
      return VAR_NUM(pow(n1, n2));
    }
    return VAR_NULL;
  }

  CHECK_INST_BINARY_OP("**");
  UNSUPPORTED_BINARY_OP("**");
  return VAR_NULL;
}

Var varBitAnd(PKVM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(&);
  CHECK_INST_BINARY_OP("&");
  UNSUPPORTED_BINARY_OP("&");
  return VAR_NULL;
}

Var varBitOr(PKVM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(|);
  CHECK_INST_BINARY_OP("|");
  UNSUPPORTED_BINARY_OP("|");
  return VAR_NULL;
}

Var varBitXor(PKVM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(^);
  CHECK_INST_BINARY_OP("^");
  UNSUPPORTED_BINARY_OP("^");
  return VAR_NULL;
}

Var varBitLshift(PKVM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(<<);
  CHECK_INST_BINARY_OP("<<");
  UNSUPPORTED_BINARY_OP("<<");
  return VAR_NULL;
}

Var varBitRshift(PKVM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(>>);
  CHECK_INST_BINARY_OP(">>");
  UNSUPPORTED_BINARY_OP(">>");
  return VAR_NULL;
}

Var varEqals(PKVM* vm, Var v1, Var v2) {
  const bool inplace = false;
  CHECK_INST_BINARY_OP("==");
  return VAR_BOOL(isValuesEqual(v1, v2));
}

Var varGreater(PKVM* vm, Var v1, Var v2) {
  CHECK_NUMERIC_OP_AS(>, VAR_BOOL);
  const bool inplace = false;
  CHECK_INST_BINARY_OP(">");
  UNSUPPORTED_BINARY_OP(">");
  return VAR_NULL;
}

Var varLesser(PKVM* vm, Var v1, Var v2) {
  CHECK_NUMERIC_OP_AS(< , VAR_BOOL);
  const bool inplace = false;
  CHECK_INST_BINARY_OP("<");
  UNSUPPORTED_BINARY_OP("<");
  return VAR_NULL;
}

Var varOpRange(PKVM* vm, Var v1, Var v2) {
  if (IS_NUM(v1) && IS_NUM(v2)) {
    return VAR_OBJ(newRange(vm, AS_NUM(v1), AS_NUM(v2)));
  }

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    String* str = varToString(vm, v2, false);
    if (str == NULL) return VAR_NULL;
    String* concat = stringJoin(vm, (String*) AS_OBJ(v1), str);
    return VAR_OBJ(concat);
  }

  const bool inplace = false;
  CHECK_INST_BINARY_OP("..");
  UNSUPPORTED_BINARY_OP("..");
  return VAR_NULL;
}

#undef RIGHT_OPERAND
#undef CHECK_NUMERIC_OP
#undef CHECK_BITWISE_OP
#undef UNSUPPORTED_UNARY_OP
#undef UNSUPPORTED_BINARY_OP

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

      String* sub = (String*) AS_OBJ(elem);
      String* str = (String*) AS_OBJ(container);
      if (sub->length > str->length) return false;

      // FIXME: strstr function can only be used for null terminated strings.
      // But pocket lang strings can contain any byte values including a null
      // byte \x00.
      const char* match = strstr(str->data, sub->data);
      return match != NULL;

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

    default: break;
  }

#define v1 container
#define v2 elem
  const bool inplace = false;
  CHECK_INST_BINARY_OP("in");
#undef v1
#undef v2

  VM_SET_ERROR(vm, stringFormat(vm, "Argument of type $ is not iterable.",
               varTypeName(container)));
  return VAR_NULL;
}

bool varIsType(PKVM* vm, Var inst, Var type) {
  if (!IS_OBJ_TYPE(type, OBJ_CLASS)) {
    VM_SET_ERROR(vm, newString(vm, "Right operand must be a class."));
    return false;
  }

  Class* cls = (Class*)AS_OBJ(type);
  Class* cls_inst = getClass(vm, inst);

  do {
    if (cls_inst == cls) return true;
    cls_inst = cls_inst->super_class;
  } while (cls_inst != NULL);

  return false;
}

Var varGetAttrib(PKVM* vm, Var on, String* attrib) {

#define ERR_NO_ATTRIB(vm, on, attrib)                                         \
  VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no attribute named '$'.", \
                                varTypeName(on), attrib->data))

  if (attrib->hash == CHECK_HASH("_class", 0xa2d93eae)) {
    return VAR_OBJ(getClass(vm, on));
  }

  if (!IS_OBJ(on)) {
    ERR_NO_ATTRIB(vm, on, attrib);
    return VAR_NULL;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING: {
      String* str = (String*)obj;
      switch (attrib->hash) {

        case CHECK_HASH("length", 0x83d03615):
          return VAR_NUM((double)(str->length));
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
      // TODO:
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

        case CHECK_HASH("name", 0x8d39bde6):
          return VAR_OBJ(newString(vm, closure->fn->name));

        case CHECK_HASH("_docs", 0x8fb536a9):
          if (closure->fn->docstring) {
            return VAR_OBJ(newString(vm, closure->fn->docstring));
          } else {
            return VAR_OBJ(newString(vm, ""));
          }

        case CHECK_HASH("arity", 0x3e96bd7a):
          return VAR_NUM((double)(closure->fn->arity));

      }
    } break;

    case OBJ_METHOD_BIND: {
      MethodBind* mb = (MethodBind*) obj;

      switch (attrib->hash) {
        case CHECK_HASH("_docs", 0x8fb536a9):
          if (mb->method->fn->docstring) {
            return VAR_OBJ(newString(vm, mb->method->fn->docstring));
          } else {
            return VAR_OBJ(newString(vm, ""));
          }

        case CHECK_HASH("name", 0x8d39bde6):
          return VAR_OBJ(newString(vm, mb->method->fn->name));

        case CHECK_HASH("instance", 0xb86d992):
          if (IS_UNDEF(mb->instance)) return VAR_NULL;
          return mb->instance;
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

    case OBJ_CLASS: {
      Class* cls = (Class*) obj;

      switch (attrib->hash) {
        case CHECK_HASH("_docs", 0x8fb536a9):
          if (cls->docstring) {
            return VAR_OBJ(newString(vm, cls->docstring));
          } else {
            return VAR_OBJ(newString(vm, ""));
          }

        case CHECK_HASH("name", 0x8d39bde6):
          return VAR_OBJ(newString(vm, cls->name->data));

        case CHECK_HASH("parent", 0xeacdfcfd):
          if (cls->super_class != NULL) {
            return VAR_OBJ(cls->super_class);
          } else {
            return VAR_NULL;
          }
      }

      Var value = mapGet(cls->static_attribs, VAR_OBJ(attrib));
      if (!IS_UNDEF(value)) return  value;

      for (int i = 0; i < (int)cls->methods.count; i++) {
        Closure* method_ = cls->methods.data[i];
        ASSERT(method_->fn->is_method, OOPS);
        const char* method_name = method_->fn->name;
        if (IS_CSTR_EQ(attrib, method_name, strlen(method_name))) {
          return VAR_OBJ(newMethodBind(vm, method_));
        }
      }

    } break;

    case OBJ_INST: {
      Instance* inst = (Instance*)obj;
      Var value = VAR_NULL;

      if (inst->native != NULL) {

        Closure* getter;
        // TODO: static vm string?
        String* getter_name = newString(vm, GETTER_NAME);
        vmPushTempRef(vm, &getter_name->_super); // getter_name.
        bool has_getter = hasMethod(vm, on, getter_name, &getter);
        vmPopTempRef(vm); // getter_name.

        if (has_getter) {
          Var attrib_name = VAR_OBJ(attrib);
          vmCallMethod(vm, on, getter, 1, &attrib_name, &value);
          return value; // If any error occure, it was already set.
        }
      }

      value = mapGet(inst->attribs, VAR_OBJ(attrib));
      if (!IS_UNDEF(value)) return value;

      Closure* method;
      if (hasMethod(vm, on, attrib, &method)) {
        MethodBind* mb = newMethodBind(vm, method);
        mb->instance = on;
        return VAR_OBJ(mb);
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

  if (!IS_OBJ(on)) {
    ERR_NO_ATTRIB(vm, on, attrib);
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {

    case OBJ_MODULE: {
      moduleSetGlobal(vm, (Module*) obj, attrib->data, attrib->length, value);
    } return;

    case OBJ_FUNC:
    case OBJ_UPVALUE:
      UNREACHABLE(); // Functions aren't first class objects.
      break;

    case OBJ_CLASS: {
      Class* cls = (Class*) obj;
      mapSet(vm, cls->static_attribs, VAR_OBJ(attrib), value);
      return;
    }

    case OBJ_INST: {

      Instance* inst = (Instance*)obj;
      if (inst->native != NULL) {
        Closure* setter;
        // TODO: static vm string?
        String* setter_name = newString(vm, SETTER_NAME);
        vmPushTempRef(vm, &setter_name->_super); // setter_name.
        bool has_setter = hasMethod(vm, VAR_OBJ(inst), setter_name, &setter);
        vmPopTempRef(vm); // setter_name.

        if (has_setter) {

          // FIXME:
          // Once we retreive values from directly the stack we can pass the
          // args pointer, pointing in the VM stack, instead of creating a temp
          // array as bellow.
          Var args[2] = { VAR_OBJ(attrib), value };

          vmCallMethod(vm, VAR_OBJ(inst), setter, 2, args, NULL);
          return; // If any error occure, it was already set.
        }
      }

      mapSet(vm, inst->attribs, VAR_OBJ(attrib), value);
      return;

    } break;

    default:
      break;
  }

  ERR_NO_ATTRIB(vm, on, attrib);
  return;

#undef ATTRIB_IMMUTABLE
#undef ERR_NO_ATTRIB
}

// Given a range. It'll "normalize" the range to slice an object (string or
// list) set the [start] index [length] and [reversed]. On success it'll return
// true.
static bool _normalizeSliceRange(PKVM* vm, Range* range, uint32_t count,
                             int32_t* start, int32_t* length, bool* reversed) {
  if ((floor(range->from) != range->from) ||
    (floor(range->to) != range->to)) {
    VM_SET_ERROR(vm, newString(vm, "Expected a whole number."));
    return false;
  }

  int32_t from = (int32_t)range->from;
  int32_t to = (int32_t)range->to;

  if (from < 0) from = count + from;
  if (to < 0) to = count + to;

  *reversed = false;
  if (to < from) {
    int32_t tmp = to;
    to = from;
    from = tmp;
    *reversed = true;
  }

  if (from < 0 || count <= (uint32_t) to) {

    // Special case we allow 0..0 or 0..-1, -1..0, -1..-1 to be valid slice
    // ranges for empty string/list,  and will gives an empty string/list.
    if (count == 0 && (from == 0 || from == -1) && (to == 0 || to == -1)) {
      *start = 0;
      *length = 0;
      *reversed = false;
      return true;
    }

    VM_SET_ERROR(vm, newString(vm, "Index out of bound."));
    return false;
  }

  *start = from;
  *length = to - from + 1;

  return true;
}

// Slice the string with the [range] and reutrn it. On error it'll set
// an error and return NULL.
static String* _sliceString(PKVM* vm, String* str, Range* range) {

  int32_t start, length; bool reversed;
  if (!_normalizeSliceRange(vm, range, str->length,
                           &start, &length, &reversed)) {
    return NULL;
  }

  // Optimize case.
  if (start == 0 && length == str->length && !reversed) return str;

  // TODO: check if length is 1 and return pre allocated character string.

  String* slice = newStringLength(vm, str->data + start, length);
  if (!reversed) return slice;

  for (int32_t i = 0; i < length / 2; i++) {
    char tmp = slice->data[i];
    slice->data[i] = slice->data[length - i - 1];
    slice->data[length - i - 1] = tmp;
  }
  slice->hash = utilHashString(slice->data);
  return slice;
}

// Slice the list with the [range] and reutrn it. On error it'll set
// an error and return NULL.
static List* _sliceList(PKVM* vm, List* list, Range* range) {

  int32_t start, length; bool reversed;
  if (!_normalizeSliceRange(vm, range, list->elements.count,
                            &start, &length, &reversed)) {
    return NULL;
  }

  List* slice = newList(vm, length);
  vmPushTempRef(vm, &slice->_super); // slice.

  for (int32_t i = 0; i < length; i++) {
    int32_t ind = (reversed) ? start + length - 1 - i : start + i;
    listAppend(vm, slice, list->elements.data[ind]);
  }

  vmPopTempRef(vm); // slice.
  return slice;
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

      if (isInteger(key, &index)) {
        // Normalize index.
        if (index < 0) index = str->length + index;
        if (index >= str->length || index < 0) {
          VM_SET_ERROR(vm, newString(vm, "String index out of bound."));
          return VAR_NULL;
        }
        // FIXME: Add static VM characters instead of allocating here.
        String* c = newStringLength(vm, str->data + index, 1);
        return VAR_OBJ(c);
      }

      if (IS_OBJ_TYPE(key, OBJ_RANGE)) {
        String* subs = _sliceString(vm, str, (Range*) AS_OBJ(key));
        if (subs != NULL) return VAR_OBJ(subs);
        return VAR_NULL;
      }

    } break;

    case OBJ_LIST: {
      int64_t index;
      pkVarBuffer* elems = &((List*)obj)->elements;

      if (isInteger(key, &index)) {
        // Normalize index.
        if (index < 0) index = elems->count + index;
        if (index >= elems->count || index < 0) {
          VM_SET_ERROR(vm, newString(vm, "List index out of bound."));
          return VAR_NULL;
        }
        return elems->data[index];
      }

      if (IS_OBJ_TYPE(key, OBJ_RANGE)) {
        List* sublist = _sliceList(vm, (List*)obj, (Range*)AS_OBJ(key));
        if (sublist != NULL) return VAR_OBJ(sublist);
        return VAR_NULL;
      }

    } break;

    case OBJ_MAP: {
      Var value = mapGet((Map*)obj, key);
      if (IS_UNDEF(value)) {

        if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
          VM_SET_ERROR(vm, stringFormat(vm, "Unhashable key '$'.",
                                        varTypeName(key)));
        } else {
          String* key_repr = varToString(vm, key, true);
          vmPushTempRef(vm, &key_repr->_super); // key_repr.
          VM_SET_ERROR(vm, stringFormat(vm, "Key '@' not exists", key_repr));
          vmPopTempRef(vm); // key_repr.
        }
        return VAR_NULL;
      }
      return value;
    } break;

    case OBJ_FUNC:
    case OBJ_UPVALUE:
      UNREACHABLE(); // Not first class objects.

    case OBJ_INST: {
      Var ret;
      if (_callBinaryOpMethod(vm, on, key, "[]", &ret)) {
        return ret;
      }
    } break;

    default:
      break;
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

      // Normalize index.
      if (index < 0) index = elems->count + index;
      if (index >= elems->count || index < 0) {
        VM_SET_ERROR(vm, newString(vm, "List index out of bound."));
        return;
      }
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

    case OBJ_INST: {

      Closure* closure = NULL;
      String* name = newString(vm, "[]=");
      vmPushTempRef(vm, &name->_super); // name.
      bool has_method = hasMethod(vm, on, name, &closure);
      vmPopTempRef(vm); // name.

      if (has_method) {
        Var args[2] = { key, value };
        vmCallMethod(vm, on, closure, 2, args, NULL);
        return;
      }

    } break;

    default:
      break;
  }

  VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.",
               varTypeName(on)));
  return;
}
