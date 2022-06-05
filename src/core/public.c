/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

// This file contains all the pocketlang public function implementations.

#include <math.h>

#ifndef PK_AMALGAMATED
#include <pocketlang.h>
#include "core.h"
#include "compiler.h"
#include "utils.h"
#include "value.h"
#include "vm.h"
#endif

// FIXME: Document this or Find a better way.
//
// Pocketlang core doesn't implement path resolving funcionality. Rather it
// should be provided the host application. By default we're using an
// implementation from the path library. However pocket core cannot be depend
// on its libs, otherwise it'll breaks the encapsulation.
//
// As a workaround we declare the default path resolver here and use it.
// But if someone wants to compile just the core pocketlang without libs
// they have to define PK_NO_LIBS to prevent the compiler from not be able
// to find functions when linking.
#ifndef PK_NO_LIBS
  void registerLibs(PKVM* vm);
  void cleanupLibs(PKVM* vm);
  char* pathResolveImport(PKVM* vm, const char* from, const char* path);

#ifndef PK_NO_DL
  void* osLoadDL(PKVM* vm, const char* path);
  PkHandle* osImportDL(PKVM* vm, void* handle);
  void osUnloadDL(PKVM* vm, void* handle);
#endif

#endif

#define CHECK_ARG_NULL(name) \
  ASSERT((name) != NULL, "Argument " #name " was NULL.");

#define CHECK_HANDLE_TYPE(handle, type)          \
  do {                                           \
    CHECK_ARG_NULL(handle);                      \
    ASSERT(IS_OBJ_TYPE(handle->value, type),     \
      "Given handle is not of type " #type "."); \
  } while (false)

#define VALIDATE_SLOT_INDEX(index)                                           \
  do {                                                                       \
    ASSERT(index >= 0, "Slot index was negative.");                          \
    ASSERT(index < pkGetSlotsCount(vm),                                      \
      "Slot index is too large. Did you forget to call pkReserveSlots()?."); \
  } while (false)

#define CHECK_FIBER_EXISTS(vm)                                           \
  do {                                                                   \
    ASSERT(vm->fiber != NULL,                                            \
           "No fiber exists. Did you forget to call pkReserveSlots()?"); \
  } while (false)

// A convenient macro to get the nth (1 based) argument of the current
// function.
#define ARG(n) (vm->fiber->ret[n])

// Nth slot is same as Nth argument, It'll also work if we allocate more
// slots but the caller should ensure the index.
#define SLOT(n) ARG(n)

// This will work.
#define SET_SLOT(n, val) SLOT(n) = (val);

// Evaluates to the current function's argument count.
#define ARGC ((int)(vm->fiber->sp - vm->fiber->ret) - 1)

// The default allocator that will be used to initialize the PKVM's
// configuration if the host doesn't provided any allocators for us.
static void* defaultRealloc(void* memory, size_t new_size, void* _);

static void stderrWrite(PKVM* vm, const char* text);
static void stdoutWrite(PKVM* vm, const char* text);
static char* stdinRead(PKVM* vm);
static char* loadScript(PKVM* vm, const char* path);

void* pkRealloc(PKVM* vm, void* ptr, size_t size) {
  ASSERT(vm->config.realloc_fn != NULL, "PKVM's allocator was NULL.");
  return vm->config.realloc_fn(ptr, size, vm->config.user_data);
}

PkConfiguration pkNewConfiguration() {
  PkConfiguration config;
  memset(&config, 0, sizeof(config));

  config.realloc_fn = defaultRealloc;

  config.stdout_write = stdoutWrite;
  config.stderr_write = stderrWrite;
  config.stdin_read = stdinRead;
#ifndef PK_NO_LIBS
  config.resolve_path_fn = pathResolveImport;

#ifndef PK_NO_DL
  config.load_dl_fn = osLoadDL;
  config.import_dl_fn = osImportDL;
  config.unload_dl_fn = osUnloadDL;
#endif

#endif
  config.load_script_fn = loadScript;

  return config;
}

PKVM* pkNewVM(PkConfiguration* config) {

  PkConfiguration default_config = pkNewConfiguration();

  if (config == NULL) config = &default_config;

  PKVM* vm = (PKVM*)config->realloc_fn(NULL, sizeof(PKVM), config->user_data);
  memset(vm, 0, sizeof(PKVM));

  vm->config = *config;
  vm->working_set_count = 0;
  vm->working_set_capacity = MIN_CAPACITY;
  vm->working_set = (Object**)vm->config.realloc_fn(
                       NULL, sizeof(Object*) * vm->working_set_capacity, NULL);
  vm->next_gc = INITIAL_GC_SIZE;
  vm->collecting_garbage = false;
  vm->min_heap_size = MIN_HEAP_SIZE;
  vm->heap_fill_percent = HEAP_FILL_PERCENT;

  vm->modules = newMap(vm);
  vm->search_paths = newList(vm, 8);

  vm->builtins_count = 0;

  // This is necessary to prevent garbage collection skip the entry in this
  // array while we're building it.
  for (int i = 0; i < PK_INSTANCE; i++) {
    vm->builtin_classes[i] = NULL;
  }

  initializeCore(vm);

#ifndef PK_NO_LIBS
  registerLibs(vm);
#endif

  return vm;
}

void pkFreeVM(PKVM* vm) {

#ifndef PK_NO_LIBS
  cleanupLibs(vm);
#endif

  Object* obj = vm->first;
  while (obj != NULL) {
    Object* next = obj->next;
    freeObject(vm, obj);
    obj = next;
  }

  vm->working_set = (Object**)vm->config.realloc_fn(
    vm->working_set, 0, vm->config.user_data);

  // Tell the host application that it forget to release all of it's handles
  // before freeing the VM.
  ASSERT(vm->handles == NULL, "Not all handles were released.");

  DEALLOCATE(vm, vm, PKVM);
}

void* pkGetUserData(const PKVM* vm) {
  return vm->config.user_data;
}

void pkSetUserData(PKVM* vm, void* user_data) {
  vm->config.user_data = user_data;
}

void pkRegisterBuiltinFn(PKVM* vm, const char* name, pkNativeFn fn,
                         int arity, const char* docstring) {
  ASSERT(vm->builtins_count < BUILTIN_FN_CAPACITY,
        "Maximum builtin function limit reached, To increase the limit set "
        "BUILTIN_FN_CAPACITY and recompile.");

  // TODO: if the functions are sorted, we can do a binary search, but builtin
  // functions are not searched at runtime, just looked up using it's index
  // O(1) However it'll decrease the compile time.
  for (int i = 0; i < vm->builtins_count; i++) {
    Closure* bfn = vm->builtins_funcs[i];
    ASSERT(strcmp(bfn->fn->name, name) != 0,
           "Overriding existing function not supported yet.");
  }

  Function* fptr = newFunction(vm, name, (int) strlen(name), NULL,
                               true, docstring, NULL);
  vmPushTempRef(vm, &fptr->_super); // fptr.
  fptr->native = fn;
  fptr->arity = arity;
  vm->builtins_funcs[vm->builtins_count++] = newClosure(vm, fptr);
  vmPopTempRef(vm); // fptr.
}

void pkAddSearchPath(PKVM* vm, const char* path) {
  CHECK_ARG_NULL(path);

  size_t length = strlen(path);
  ASSERT(length > 0, "Path size cannot be 0.");

  char last = path[length - 1];
  ASSERT(last == '/' || last == '\\', "Path should ends with "
                                      "either '/' or '\\'.");

  String* spath = newStringLength(vm, path, (uint32_t) length);
  vmPushTempRef(vm, &spath->_super); // spath.
  listAppend(vm, vm->search_paths, VAR_OBJ(spath));
  vmPopTempRef(vm); // spath.
}

PkHandle* pkNewModule(PKVM* vm, const char* name) {
  CHECK_ARG_NULL(name);
  Module* module = newModuleInternal(vm, name);

  vmPushTempRef(vm, &module->_super); // module.
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(module));
  vmPopTempRef(vm); // module.

  return handle;
}

void pkRegisterModule(PKVM* vm, PkHandle* module) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);

  Module* module_ = (Module*)AS_OBJ(module->value);
  vmRegisterModule(vm, module_, module_->name);
}

void pkModuleAddFunction(PKVM* vm, PkHandle* module, const char* name,
                         pkNativeFn fptr, int arity, const char* docstring) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);
  CHECK_ARG_NULL(fptr);

  moduleAddFunctionInternal(vm, (Module*)AS_OBJ(module->value),
                            name, fptr, arity, docstring);
}

PkHandle* pkNewClass(PKVM* vm, const char* name,
                     PkHandle* base_class, PkHandle* module,
                     pkNewInstanceFn new_fn,
                     pkDeleteInstanceFn delete_fn,
                     const char* docstring) {
  CHECK_ARG_NULL(module);
  CHECK_ARG_NULL(name);
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);

  Class* super = vm->builtin_classes[PK_OBJECT];
  if (base_class != NULL) {
    CHECK_HANDLE_TYPE(base_class, OBJ_CLASS);
    super = (Class*)AS_OBJ(base_class->value);
  }

  Class* class_ = newClass(vm, name, (int)strlen(name),
                           super, (Module*)AS_OBJ(module->value),
                           docstring, NULL);
  class_->new_fn = new_fn;
  class_->delete_fn = delete_fn;

  vmPushTempRef(vm, &class_->_super); // class_.
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(class_));
  vmPopTempRef(vm); // class_.
  return handle;
}

void pkClassAddMethod(PKVM* vm, PkHandle* cls,
                      const char* name,
                      pkNativeFn fptr, int arity, const char* docstring) {
  CHECK_ARG_NULL(cls);
  CHECK_ARG_NULL(fptr);
  CHECK_HANDLE_TYPE(cls, OBJ_CLASS);

  // TODO:
  // Check if the method name is valid, and validate argc for special
  // methods (like "@getter", "@call", "+", "-", etc).

  Class* class_ = (Class*)AS_OBJ(cls->value);

  Function* fn = newFunction(vm, name, (int)strlen(name),
                             class_->owner, true, docstring, NULL);
  vmPushTempRef(vm, &fn->_super); // fn.

  fn->arity = arity;
  fn->is_method = true;
  fn->native = fptr;

  // No need to push the function to temp references of the VM
  // since it's written to the constant pool of the module and the module
  // won't be garbage collected (class handle has reference to the module).

  Closure* method = newClosure(vm, fn);
  vmPopTempRef(vm); // fn.
  vmPushTempRef(vm, &method->_super); // method.
  {
    pkClosureBufferWrite(&class_->methods, vm, method);
    if (!strcmp(name, CTOR_NAME)) class_->ctor = method;
  }
  vmPopTempRef(vm); // method.
}

void pkModuleAddSource(PKVM* vm, PkHandle* module, const char* source) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);
  CHECK_ARG_NULL(source);
  // TODO: compiler options, maybe set to the vm and reuse it here.
  compile(vm, (Module*) AS_OBJ(module->value), source, NULL);
}

void pkReleaseHandle(PKVM* vm, PkHandle* handle) {
  ASSERT(handle != NULL, "Given handle was NULL.");

  // If the handle is the head of the vm's handle chain set it to the next one.
  if (handle == vm->handles) {
    vm->handles = handle->next;
  }

  // Remove the handle from the chain by connecting the both ends together.
  if (handle->next) handle->next->prev = handle->prev;
  if (handle->prev) handle->prev->next = handle->next;

  // Free the handle.
  DEALLOCATE(vm, handle, PkHandle);
}

PkResult pkRunString(PKVM* vm, const char* source) {

  PkResult result = PK_RESULT_SUCCESS;

  // Create a temproary module for the source.
  Module* module = newModule(vm);
  vmPushTempRef(vm, &module->_super); // module.
  {
    module->path = newString(vm, "@(String)");
    result = compile(vm, module, source, NULL);
    if (result != PK_RESULT_SUCCESS) return result;

    // Module initialized needs to be set to true just before executing their
    // main function to avoid cyclic inclusion crash the VM.
    module->initialized = true;

    Fiber* fiber = newFiber(vm, module->body);
    vmPushTempRef(vm, &fiber->_super); // fiber.
    vmPrepareFiber(vm, fiber, 0, NULL);
    vmPopTempRef(vm); // fiber.
    result = vmRunFiber(vm, fiber);
  }
  vmPopTempRef(vm); // module.

  return result;
}

PkResult pkRunFile(PKVM* vm, const char* path) {

  // Note: The file may have been imported by some other script and cached in
  // the VM's scripts cache. But we're not using that instead, we'll recompile
  // the file and update the cache.

  ASSERT(vm->config.load_script_fn != NULL,
         "No script loading functions defined.");

  PkResult result = PK_RESULT_SUCCESS;
  Module* module = NULL;

  // Resolve the path.
  char* resolved_ = NULL;
  if (vm->config.resolve_path_fn != NULL) {
    resolved_ = vm->config.resolve_path_fn(vm, NULL, path);
  }

  if (resolved_ == NULL) {
    // FIXME: Error print should be moved and check for ascii color codes.
    if (vm->config.stderr_write != NULL) {
      vm->config.stderr_write(vm, "Error finding script at \"");
      vm->config.stderr_write(vm, path);
      vm->config.stderr_write(vm, "\"\n");
    }
    return PK_RESULT_COMPILE_ERROR;
  }

  module = newModule(vm);
  vmPushTempRef(vm, &module->_super); // module.
  {
    // Set module path and and deallocate resolved.
    String* script_path = newString(vm, resolved_);
    vmPushTempRef(vm, &script_path->_super); // script_path.
    pkRealloc(vm, resolved_, 0);
    module->path = script_path;
    vmPopTempRef(vm); // script_path.

    initializeModule(vm, module, true);

    const char* _path = module->path->data;
    char* source = vm->config.load_script_fn(vm, _path);
    if (source == NULL) {
      result = PK_RESULT_COMPILE_ERROR;
      // FIXME: Error print should be moved and check for ascii color codes.
      if (vm->config.stderr_write != NULL) {
        vm->config.stderr_write(vm, "Error loading script at \"");
        vm->config.stderr_write(vm, _path);
        vm->config.stderr_write(vm, "\"\n");
      }
    } else {
      result = compile(vm, module, source, NULL);
      pkRealloc(vm, source, 0);
    }

    if (result == PK_RESULT_SUCCESS) {
      vmRegisterModule(vm, module, module->path);
    }
  }
  vmPopTempRef(vm); // module.

  if (result != PK_RESULT_SUCCESS) return result;

  // Module initialized needs to be set to true just before executing their
  // main function to avoid cyclic inclusion crash the VM.
  module->initialized = true;
  Fiber* fiber = newFiber(vm, module->body);
  vmPushTempRef(vm, &fiber->_super); // fiber.
  vmPrepareFiber(vm, fiber, 0, NULL);
  vmPopTempRef(vm); // fiber.
  return vmRunFiber(vm, fiber);
}

// FIXME: this should be moved to somewhere general.
//
// Returns true if the string is empty, used to check if the input line is
// empty to skip compilation of empty string in the bellow repl mode.
static inline bool isStringEmpty(const char* line) {
  ASSERT(line != NULL, OOPS);

  for (const char* c = line; *c != '\0'; c++) {
    if (!utilIsSpace(*c)) return false;
  }
  return true;
}

// FIXME:
// this should be moved to somewhere general along with isStringEmpty().
//
// This function will get the main function from the module to run it in the
// repl mode.
Closure* moduleGetMainFunction(PKVM* vm, Module* module) {

  int main_index = moduleGetGlobalIndex(module, IMPLICIT_MAIN_NAME,
                                        (uint32_t) strlen(IMPLICIT_MAIN_NAME));
  if (main_index == -1) return NULL;
  ASSERT_INDEX(main_index, (int) module->globals.count);
  Var main_fn = module->globals.data[main_index];
  ASSERT(IS_OBJ_TYPE(main_fn, OBJ_CLOSURE), OOPS);
  return (Closure*) AS_OBJ(main_fn);
}

PkResult pkRunREPL(PKVM* vm) {

  pkWriteFn printfn = vm->config.stdout_write;
  pkWriteFn printerrfn = vm->config.stderr_write;
  pkReadFn inputfn = vm->config.stdin_read;
  PkResult result = PK_RESULT_SUCCESS;

  CompileOptions options = newCompilerOptions();
  options.repl_mode = true;

  if (inputfn == NULL) {
    if (printerrfn) printerrfn(vm, "REPL failed to input.");
    return PK_RESULT_RUNTIME_ERROR;
  }

  // The main module that'll be used to compile and execute the input source.
  PkHandle* module = pkNewModule(vm, "@(REPL)");
  ASSERT(IS_OBJ_TYPE(module->value, OBJ_MODULE), OOPS);
  Module* _module = (Module*) AS_OBJ(module->value);
  initializeModule(vm, _module, true);

  // A buffer to store multiple lines read from stdin.
  pkByteBuffer lines;
  pkByteBufferInit(&lines);

  // Will be set to true if the compilation failed with unexpected EOF to add
  // more lines to the [lines] buffer.
  bool need_more_lines = false;

  bool done = false;
  do {

    const char* listening = (!need_more_lines) ? ">>> " : "... ";
    printfn(vm, listening);

    // Read a line from stdin and add the line to the lines buffer.
    char* line = inputfn(vm);
    if (line == NULL) {
      if (printerrfn) printerrfn(vm, "REPL failed to input.");
      result = PK_RESULT_RUNTIME_ERROR;
      break;
    }

    // If the line contains EOF, REPL should be stopped.
    size_t line_length = strlen(line);
    if (line_length >= 1 && *(line + line_length - 1) == EOF) {
      printfn(vm, "\n");
      result = PK_RESULT_SUCCESS;
      pkRealloc(vm, line, 0);
      break;
    }

    // If the line is empty, we don't have to compile it.
    if (isStringEmpty(line)) {
      if (need_more_lines) ASSERT(lines.count != 0, OOPS);
      pkRealloc(vm, line, 0);
      continue;
    }

    // Add the line to the lines buffer.
    if (lines.count != 0) pkByteBufferWrite(&lines, vm, '\n');
    pkByteBufferAddString(&lines, vm, line, (uint32_t) line_length);
    pkRealloc(vm, line, 0);
    pkByteBufferWrite(&lines, vm, '\0');

    // Compile the buffer to the module.
    result = compile(vm, _module, (const char*) lines.data, &options);

    if (result == PK_RESULT_UNEXPECTED_EOF) {
      ASSERT(lines.count > 0 && lines.data[lines.count - 1] == '\0', OOPS);
      lines.count -= 1; // Remove the null byte to append a new string.
      need_more_lines = true;
      continue;
    }

    // We're buffering the lines for unexpected EOF, if we reached here that
    // means it's either successfully compiled or compilation error. Clean the
    // buffer for the next iteration.
    need_more_lines = false;
    pkByteBufferClear(&lines, vm);

    if (result != PK_RESULT_SUCCESS) continue;

    // Compiled source would be the "main" function of the module. Run it.
    Closure* _main = moduleGetMainFunction(vm, _module);
    ASSERT(_main != NULL, OOPS);
    result = vmCallFunction(vm, _main, 0, NULL, NULL);

  } while (!done);

  pkReleaseHandle(vm, module);

  return result;
}

/*****************************************************************************/
/* RUNTIME                                                                   */
/*****************************************************************************/

void pkSetRuntimeError(PKVM* vm, const char* message) {
  CHECK_FIBER_EXISTS(vm);
  VM_SET_ERROR(vm, newString(vm, message));
}

void pkSetRuntimeErrorFmt(PKVM* vm, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  VM_SET_ERROR(vm, newStringVaArgs(vm, fmt, args));
  va_end(args);
}

void* pkGetSelf(const PKVM* vm) {
  CHECK_FIBER_EXISTS(vm);
  ASSERT(IS_OBJ_TYPE(vm->fiber->self, OBJ_INST), OOPS);
  Instance* inst = (Instance*) AS_OBJ(vm->fiber->self);
  ASSERT(inst->native != NULL, OOPS);
  return inst->native;
}

int pkGetArgc(const PKVM* vm) {
  CHECK_FIBER_EXISTS(vm);
  return ARGC;
}

bool pkCheckArgcRange(PKVM* vm, int argc, int min, int max) {
  CHECK_FIBER_EXISTS(vm);
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

// Set error for incompatible type provided as an argument. (TODO: got type).
#define ERR_INVALID_SLOT_TYPE(slot, ty_name)                       \
  do {                                                             \
    char buff[STR_INT_BUFF_SIZE];                                  \
    sprintf(buff, "%d", slot);                                     \
    VM_SET_ERROR(vm, stringFormat(vm, "Expected a '$' at slot $.", \
                                      ty_name, buff));             \
  } while (false)

// FIXME: If the user needs just the boolean value of the object, they should
// use pkGetSlotBool().
bool pkValidateSlotBool(PKVM* vm, int slot, bool* value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);

  Var val = ARG(slot);
  if (!IS_BOOL(val)) {
    ERR_INVALID_SLOT_TYPE(slot, "Boolean");
    return false;
  }

  if (value) *value = AS_BOOL(val);
  return true;
}

bool pkValidateSlotNumber(PKVM* vm, int slot, double* value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);

  Var val = ARG(slot);
  if (!IS_NUM(val)) {
    ERR_INVALID_SLOT_TYPE(slot, "Number");
    return false;
  }

  if (value) *value = AS_NUM(val);
  return true;
}

bool pkValidateSlotInteger(PKVM* vm, int slot, int32_t* value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);

  double n;
  if (!pkValidateSlotNumber(vm, slot, &n)) return false;

  if (floor(n) != n) {
    VM_SET_ERROR(vm, newString(vm, "Expected an integer got float."));
    return false;
  }

  if (value) *value = (int32_t) n;
  return true;
}

bool pkValidateSlotString(PKVM* vm, int slot, const char** value,
                                    uint32_t* length) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);

  Var val = ARG(slot);
  if (!IS_OBJ_TYPE(val, OBJ_STRING)) {
    ERR_INVALID_SLOT_TYPE(slot, "String");
    return false;
  }
  String* str = (String*)AS_OBJ(val);
  if (value) *value = str->data;
  if (length) *length = str->length;
  return true;
}

bool pkValidateSlotType(PKVM* vm, int slot, PkVarType type) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);
  if (getVarType(ARG(slot)) != type) {
    ERR_INVALID_SLOT_TYPE(slot, getPkVarTypeName(type));
    return false;
  }

  return true;
}

bool pkValidateSlotInstanceOf(PKVM* vm, int slot, int cls) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);
  VALIDATE_SLOT_INDEX(cls);

  Var instance = ARG(slot), class_ = SLOT(cls);
  if (!varIsType(vm, instance, class_)) {
    // If [class_] is not a valid class, it's already an error.
    if (VM_HAS_ERROR(vm)) return false;
    ERR_INVALID_SLOT_TYPE(slot, ((Class*)AS_OBJ(class_))->name->data);
    return false;
  }

  return true;
}

bool pkIsSlotInstanceOf(PKVM* vm, int inst, int cls, bool* val) {
  CHECK_ARG_NULL(val);
  VALIDATE_SLOT_INDEX(inst);
  VALIDATE_SLOT_INDEX(cls);

  *val = varIsType(vm, inst, cls);
  return !VM_HAS_ERROR(vm);
}

void pkReserveSlots(PKVM* vm, int count) {
  if (vm->fiber == NULL) vm->fiber = newFiber(vm, NULL);
  int needed = (int)(vm->fiber->ret - vm->fiber->stack) + count;
  vmEnsureStackSize(vm, vm->fiber, needed);
}

int pkGetSlotsCount(PKVM* vm) {
  CHECK_FIBER_EXISTS(vm);
  return vm->fiber->stack_size - (int)(vm->fiber->ret - vm->fiber->stack);
}

PkVarType pkGetSlotType(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  return getVarType(SLOT(index));
}

bool pkGetSlotBool(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  Var value = SLOT(index);
  return toBool(value);
}

double pkGetSlotNumber(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  Var value = SLOT(index);
  ASSERT(IS_NUM(value), "Slot value wasn't a Number.");
  return AS_NUM(value);
}

const char* pkGetSlotString(PKVM* vm, int index, uint32_t* length) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  Var value = SLOT(index);
  ASSERT(IS_OBJ_TYPE(value, OBJ_STRING), "Slot value wasn't a String.");
  if (length != NULL) *length = ((String*)AS_OBJ(value))->length;
  return ((String*)AS_OBJ(value))->data;
}

PkHandle* pkGetSlotHandle(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  return vmNewHandle(vm, SLOT(index));
}

void* pkGetSlotNativeInstance(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  Var value = SLOT(index);
  ASSERT(IS_OBJ_TYPE(value, OBJ_INST), "Slot value wasn't an Instance");

  // TODO: If the native initializer (pkNewInstanceFn()) returned NULL,
  // [inst->native] will be null - handle.
  Instance* inst = (Instance*) AS_OBJ(value);
  ASSERT(inst->native != NULL, "Slot value wasn't a Native Instance");

  return inst->native;
}

void pkSetSlotNull(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_NULL);
}

void pkSetSlotBool(PKVM* vm, int index, bool value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_BOOL(value));
}

void pkSetSlotNumber(PKVM* vm, int index, double value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_NUM(value));
}

void pkSetSlotString(PKVM* vm, int index, const char* value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_OBJ(newString(vm, value)));
}

void pkSetSlotStringLength(PKVM* vm, int index,
                                     const char* value, uint32_t length) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_OBJ(newStringLength(vm, value, length)));
}

void pkSetSlotStringFmt(PKVM* vm, int index, const char* fmt, ...) {

  va_list args;
  va_start(args, fmt);
  SET_SLOT(index, VAR_OBJ(newStringVaArgs(vm, fmt, args)));
  va_end(args);
}

void pkSetSlotHandle(PKVM* vm, int index, PkHandle* handle) {
  CHECK_FIBER_EXISTS(vm);
  CHECK_ARG_NULL(handle);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, handle->value);
}

uint32_t pkGetSlotHash(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  Var value = SLOT(index);
  ASSERT(!IS_OBJ(value) || isObjectHashable(AS_OBJ(value)->type), OOPS);
  return varHashValue(value);
}

bool pkSetAttribute(PKVM* vm, int instance, const char* name, int value) {
  CHECK_FIBER_EXISTS(vm);
  CHECK_ARG_NULL(name);
  VALIDATE_SLOT_INDEX(instance);
  VALIDATE_SLOT_INDEX(value);

  String* sname = newString(vm, name);
  vmPushTempRef(vm, &sname->_super); // sname.
  varSetAttrib(vm, SLOT(instance), sname, SLOT(value));
  vmPopTempRef(vm); // sname.

  return !VM_HAS_ERROR(vm);
}

bool pkGetAttribute(PKVM* vm, int instance, const char* name,
                              int index) {
  CHECK_FIBER_EXISTS(vm);
  CHECK_ARG_NULL(name);
  VALIDATE_SLOT_INDEX(instance);
  VALIDATE_SLOT_INDEX(index);

  String* sname = newString(vm, name);
  vmPushTempRef(vm, &sname->_super); // sname.
  SET_SLOT(index, varGetAttrib(vm, SLOT(instance), sname));
  vmPopTempRef(vm); // sname.

  return !VM_HAS_ERROR(vm);
}

static Var _newInstance(PKVM* vm, Class* cls, int argc, Var* argv) {
  Var instance = preConstructSelf(vm, cls);
  if (VM_HAS_ERROR(vm)) return VAR_NULL;

  if (IS_OBJ(instance)) vmPushTempRef(vm, AS_OBJ(instance)); // instance.

  Closure* ctor = cls->ctor;
  while (ctor == NULL) {
    cls = cls->super_class;
    if (cls == NULL) break;
    ctor = cls->ctor;
  }

  if (ctor != NULL) vmCallMethod(vm, instance, ctor, argc, argv, NULL);
  if (IS_OBJ(instance)) vmPopTempRef(vm); // instance.

  return instance;
}

bool pkNewInstance(PKVM* vm, int cls, int index, int argc, int argv) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  if (argc != 0) {
    VALIDATE_SLOT_INDEX(argv);
    VALIDATE_SLOT_INDEX(argv + argc - 1);
  }

  ASSERT(IS_OBJ_TYPE(SLOT(cls), OBJ_CLASS), "Slot value wasn't a class.");

  Class* class_ = (Class*) AS_OBJ(SLOT(cls));

  SET_SLOT(index, _newInstance(vm, class_, argc, vm->fiber->ret + argv));
  return !VM_HAS_ERROR(vm);
}

void pkNewRange(PKVM* vm, int index, double first, double last) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newRange(vm, first, last)));
}

void pkNewList(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newList(vm, 0)));
}

void pkNewMap(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newMap(vm)));
}

bool pkListInsert(PKVM* vm, int list, int32_t index, int value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(list);
  VALIDATE_SLOT_INDEX(value);

  ASSERT(IS_OBJ_TYPE(SLOT(list), OBJ_LIST), "Slot value wasn't a List");
  List* l = (List*) AS_OBJ(SLOT(list));
  if (index < 0) index = l->elements.count + index + 1;

  if (index < 0 || (uint32_t) index > l->elements.count) {
    VM_SET_ERROR(vm, newString(vm, "Index out of bounds."));
    return false;
  }

  listInsert(vm, l, (uint32_t) index, SLOT(value));
  return true;
}

bool pkListPop(PKVM* vm, int list, int32_t index, int popped) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(list);
  if (popped >= 0) VALIDATE_SLOT_INDEX(popped);

  ASSERT(IS_OBJ_TYPE(SLOT(list), OBJ_LIST), "Slot value wasn't a List");
  List* l = (List*) AS_OBJ(SLOT(list));
  if (index < 0) index += l->elements.count;

  if (index < 0 || (uint32_t) index >= l->elements.count) {
    VM_SET_ERROR(vm, newString(vm, "Index out of bounds."));
    return false;
  }

  Var p = listRemoveAt(vm, l, index);
  if (popped >= 0) SET_SLOT(popped, p);
  return true;
}

uint32_t pkListLength(PKVM* vm, int list) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(list);

  ASSERT(IS_OBJ_TYPE(SLOT(list), OBJ_LIST), "Slot value wasn't a List");
  List* l = (List*)AS_OBJ(SLOT(list));

  return l->elements.count;
}

bool pkCallFunction(PKVM* vm, int fn, int argc, int argv, int ret) {
  CHECK_FIBER_EXISTS(vm);
  ASSERT(IS_OBJ_TYPE(SLOT(fn), OBJ_CLOSURE), "Slot value wasn't a function");
  if (argc != 0) {
    VALIDATE_SLOT_INDEX(argv);
    VALIDATE_SLOT_INDEX(argv + argc - 1);
  }
  if (ret >= 0) VALIDATE_SLOT_INDEX(ret);

  // Calls a class == construct.
  if (IS_OBJ_TYPE(SLOT(fn), OBJ_CLASS)) {
    Var inst = _newInstance(vm, (Class*) AS_OBJ(SLOT(fn)), argc,
                            vm->fiber->ret + argv);
    if (ret >= 0) SET_SLOT(ret, inst);
    return !VM_HAS_ERROR(vm);
  }

  if (IS_OBJ_TYPE(SLOT(fn), OBJ_CLOSURE)) {

    Closure* func = (Closure*) AS_OBJ(SLOT(fn));

    // Methods are not first class. Accessing a method will return a method
    // bind instance which has a reference to an instance and invoking it will
    // calls the method with that instance.
    ASSERT(!func->fn->is_method, OOPS);

    Var retval;
    vmCallFunction(vm, func, argc,
                   vm->fiber->ret + argv, &retval);
    if (ret >= 0) SET_SLOT(ret, retval);
    return !VM_HAS_ERROR(vm);
  }

  VM_SET_ERROR(vm, newString(vm, "Expected a Callable."));
  return false;
}

bool pkCallMethod(PKVM* vm, int instance, const char* method,
                            int argc, int argv, int ret) {
  CHECK_FIBER_EXISTS(vm);
  CHECK_ARG_NULL(method);
  VALIDATE_SLOT_INDEX(instance);
  if (argc != 0) {
    VALIDATE_SLOT_INDEX(argv);
    VALIDATE_SLOT_INDEX(argv + argc - 1);
  }
  if (ret >= 0) VALIDATE_SLOT_INDEX(ret);

  bool is_method = false;
  String* smethod = newString(vm, method);
  vmPushTempRef(vm, &smethod->_super); // smethod.
  Var callable = getMethod(vm, SLOT(instance), smethod,
                          &is_method);
  vmPopTempRef(vm); // smethod.

  if (VM_HAS_ERROR(vm)) return false;

  // Calls a class == construct.
  if (IS_OBJ_TYPE(callable, OBJ_CLASS)) {
    Var inst = _newInstance(vm, (Class*) AS_OBJ(callable), argc,
                            vm->fiber->ret + argv);
    if (ret >= 0) SET_SLOT(ret, inst);
    return !VM_HAS_ERROR(vm);
  }

  if (IS_OBJ_TYPE(callable, OBJ_CLOSURE)) {
    Var retval;
    vmCallMethod(vm, SLOT(instance), (Closure*) AS_OBJ(callable), argc,
                 vm->fiber->ret + argv, &retval);
    if (ret >= 0) SET_SLOT(ret, retval);
    return !VM_HAS_ERROR(vm);
  }

  VM_SET_ERROR(vm, stringFormat(vm, "Instance has no method named '$'.",
                                method));
  return false;
}

void pkPlaceSelf(PKVM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, vm->fiber->self);
}

bool pkImportModule(PKVM* vm, const char* path, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  String* path_ = newString(vm, path);
  vmPushTempRef(vm, &path_->_super); // path_
  Var module = vmImportModule(vm, NULL, path_);
  vmPopTempRef(vm); // path_

  SET_SLOT(index, module);
  return !VM_HAS_ERROR(vm);
}

void pkGetClass(PKVM* vm, int instance, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(instance);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(getClass(vm, SLOT(instance))));
}

#undef ERR_INVALID_ARG_TYPE
#undef ARG
#undef SLOT
#undef SET_SLOT
#undef ARGC

/*****************************************************************************/
/* INTERNAL                                                                  */
/*****************************************************************************/

// The default allocator that will be used to initialize the vm's configuration
// if the host doesn't provided any allocators for us.
static void* defaultRealloc(void* memory, size_t new_size, void* _) {
  if (new_size == 0) {
    free(memory);
    return NULL;
  }
  return realloc(memory, new_size);
}

void stderrWrite(PKVM* vm, const char* text) {
  fprintf(stderr, "%s", text);
}

void stdoutWrite(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

static char* stdinRead(PKVM* vm) {

  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  char c;
  do {
    c = (char) fgetc(stdin);
    if (c == '\n') break;
    pkByteBufferWrite(&buff, vm, (uint8_t)c);
  } while (c != EOF);
  pkByteBufferWrite(&buff, vm, '\0');

  char* str = pkRealloc(vm, NULL, buff.count);
  memcpy(str, buff.data, buff.count);
  pkByteBufferClear(&buff, vm);
  return str;
}

static char* loadScript(PKVM* vm, const char* path) {

  FILE* file = fopen(path, "r");
  if (file == NULL) return NULL;

  // Get the source length. In windows the ftell will includes the cariage
  // return when using ftell with fseek. But that's not an issue since
  // we'll be allocating more memory than needed for fread().
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Allocate string + 1 for the NULL terminator.
  char* buff = pkRealloc(vm, NULL, file_size + 1);
  ASSERT(buff != NULL, "pkRealloc failed.");

  clearerr(file);
  size_t read = fread(buff, sizeof(char), file_size, file);
  ASSERT(read <= file_size, "fread() failed.");
  buff[read] = '\0';
  fclose(file);

  return buff;
}
