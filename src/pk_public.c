/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

// This file contains all the pocketlang public function implementations.

#include "include/pocketlang.h"

#include "pk_core.h"
#include "pk_value.h"
#include "pk_vm.h"

#define CHECK_ARG_NULL(name) \
  ASSERT((name) != NULL, "Argument " #name " was NULL.");

#define CHECK_HANDLE_TYPE(handle, type)          \
  do {                                           \
    CHECK_ARG_NULL(handle);                      \
    ASSERT(IS_OBJ_TYPE(handle->value, type),     \
      "Given handle is not of type " #type "."); \
  } while (false)

// The default allocator that will be used to initialize the PKVM's
// configuration if the host doesn't provided any allocators for us.
static void* defaultRealloc(void* memory, size_t new_size, void* _);

PkConfiguration pkNewConfiguration() {
  PkConfiguration config;
  config.realloc_fn = defaultRealloc;

  config.stdout_write = NULL;
  config.stderr_write = NULL;
  config.stdin_read = NULL;

  config.load_script_fn = NULL;
  config.resolve_path_fn = NULL;

  config.use_ansi_color = false;
  config.user_data = NULL;

  return config;
}

PkCompileOptions pkNewCompilerOptions() {
  PkCompileOptions options;
  options.debug = false;
  options.repl_mode = false;
  return options;
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
  vm->min_heap_size = MIN_HEAP_SIZE;
  vm->heap_fill_percent = HEAP_FILL_PERCENT;

  vm->modules = newMap(vm);
  vm->builtins_count = 0;

  // This is necessary to prevent garbage collection skip the entry in this
  // array while we're building it.
  for (int i = 0; i < PK_INSTANCE; i++) {
    vm->builtin_classes[i] = NULL;
  }

  initializeCore(vm);
  return vm;
}

void pkFreeVM(PKVM* vm) {

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

  DEALLOCATE(vm, vm);
}

void* pkGetUserData(const PKVM* vm) {
  return vm->config.user_data;
}

void pkSetUserData(PKVM* vm, void* user_data) {
  vm->config.user_data = user_data;
}

PkHandle* pkNewModule(PKVM* vm, const char* name) {
  CHECK_ARG_NULL(name);
  Module* module = newModuleInternal(vm, name);
  return vmNewHandle(vm, VAR_OBJ(module));
}

void pkRegisterModule(PKVM* vm, PkHandle* module) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);

  Module* module_ = (Module*)AS_OBJ(module->value);
  vmRegisterModule(vm, module_, module_->name);
}

void pkModuleAddFunction(PKVM* vm, PkHandle* module, const char* name,
                         pkNativeFn fptr, int arity) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);
  CHECK_ARG_NULL(fptr);

  moduleAddFunctionInternal(vm, (Module*)AS_OBJ(module->value),
                            name, fptr, arity,
                            NULL /*TODO: Public API for function docstring.*/);
}

PkHandle* pkModuleGetMainFunction(PKVM* vm, PkHandle* module) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);

  Module* _module = (Module*)AS_OBJ(module->value);

  int main_index = moduleGetGlobalIndex(_module, IMPLICIT_MAIN_NAME,
                                        (uint32_t)strlen(IMPLICIT_MAIN_NAME));
  if (main_index == -1) return NULL;
  ASSERT_INDEX(main_index, (int)_module->globals.count);
  Var main_fn = _module->globals.data[main_index];
  ASSERT(IS_OBJ_TYPE(main_fn, OBJ_CLOSURE), OOPS);
  return vmNewHandle(vm, main_fn);
}

PkHandle* pkNewClass(PKVM* vm, const char* name,
                     PkHandle* base_class, PkHandle* module,
                     pkNewInstanceFn new_fn,
                     pkDeleteInstanceFn delete_fn) {
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
                           NULL, NULL);
  class_->new_fn = new_fn;
  class_->delete_fn = delete_fn;

  return vmNewHandle(vm, VAR_OBJ(class_));
}

void pkClassAddMethod(PKVM* vm, PkHandle* cls,
                      const char* name,
                      pkNativeFn fptr, int arity) {
  CHECK_ARG_NULL(cls);
  CHECK_ARG_NULL(fptr);
  CHECK_HANDLE_TYPE(cls, OBJ_CLASS);

  Class* class_ = (Class*)AS_OBJ(cls->value);

  Function* fn = newFunction(vm, name, (int)strlen(name),
                             class_->owner, true, NULL, NULL);

  // No need to push the function to temp references of the VM
  // since it's written to the constant pool of the module and the module
  // won't be garbage collected (class handle has reference to the module).

  Closure* method = newClosure(vm, fn);

  // FIXME: name "_init" is literal everywhere.
  if (strcmp(name, "_init") == 0) {
    class_->ctor = method;

  } else {
    vmPushTempRef(vm, &method->_super); // method.
    pkClosureBufferWrite(&class_->methods, vm, method);
    vmPopTempRef(vm); // method.
  }
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
  DEALLOCATE(vm, handle);
}

PkResult pkCompileModule(PKVM* vm, PkHandle* module_handle, PkStringPtr source,
                         const PkCompileOptions* options) {
  CHECK_ARG_NULL(source.string);
  CHECK_HANDLE_TYPE(module_handle, OBJ_MODULE);

  Module* module = (Module*)AS_OBJ(module_handle->value);

  PkResult result = compile(vm, module, source.string, options);
  if (source.on_done) source.on_done(vm, source);
  return result;
}

// This function is responsible to call on_done function if it's done with the
// provided string pointers.
PkResult pkInterpretSource(PKVM* vm, PkStringPtr source, PkStringPtr path,
                           const PkCompileOptions* options) {

  String* path_ = newString(vm, path.string);
  if (path.on_done) path.on_done(vm, path);
  vmPushTempRef(vm, &path_->_super); // path_

  // FIXME:
  // Should I clean the module if it already exists before compiling it?

  // Load a new module to the vm's modules cache.
  Module* module = vmGetModule(vm, path_);
  if (module == NULL) {
    module = newModule(vm);
    module->path = path_;
    vmPushTempRef(vm, &module->_super); // module.
    vmRegisterModule(vm, module, path_);
    vmPopTempRef(vm); // module.
  }
  vmPopTempRef(vm); // path_

  // Compile the source.
  PkResult result = compile(vm, module, source.string, options);
  if (source.on_done) source.on_done(vm, source);
  if (result != PK_RESULT_SUCCESS) return result;

  // Set module initialized to true before the execution ends to prevent cyclic
  // inclusion cause a crash.
  module->initialized = true;

  return vmRunFiber(vm, newFiber(vm, module->body));
}

PkHandle* pkNewFiber(PKVM* vm, PkHandle* fn) {
  CHECK_HANDLE_TYPE(fn, OBJ_CLOSURE);

  Fiber* fiber = newFiber(vm, (Closure*)AS_OBJ(fn->value));
  vmPushTempRef(vm, &fiber->_super); // fiber
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(fiber));
  vmPopTempRef(vm); // fiber
  return handle;
}

PkResult pkRunFiber(PKVM* vm, PkHandle* fiber,
                    int argc, PkHandle** argv) {
  __ASSERT(fiber != NULL, "Handle fiber was NULL.");
  Var fb = fiber->value;
  __ASSERT(IS_OBJ_TYPE(fb, OBJ_FIBER), "Given handle is not a fiber.");
  Fiber* _fiber = (Fiber*)AS_OBJ(fb);

  Var* args[MAX_ARGC];
  for (int i = 0; i < argc; i++) {
    args[i] = &(argv[i]->value);
  }

  if (!vmPrepareFiber(vm, _fiber, argc, args)) {
    return PK_RESULT_RUNTIME_ERROR;
  }

  ASSERT(_fiber->frame_count == 1, OOPS);
  return vmRunFiber(vm, _fiber);
}

// TODO: Get resume argument.
PkResult pkResumeFiber(PKVM* vm, PkHandle* fiber) {
  __ASSERT(fiber != NULL, "Handle fiber was NULL.");
  Var fb = fiber->value;
  __ASSERT(IS_OBJ_TYPE(fb, OBJ_FIBER), "Given handle is not a fiber.");
  Fiber* _fiber = (Fiber*)AS_OBJ(fb);

  if (!vmSwitchFiber(vm, _fiber, NULL /* TODO: argument */)) {
    return PK_RESULT_RUNTIME_ERROR;
  }

  return vmRunFiber(vm, _fiber);
}

/*****************************************************************************/
/* RUNTIME                                                                   */
/*****************************************************************************/

#define CHECK_RUNTIME()                                     \
  do {                                                      \
    ASSERT(vm->fiber != NULL,                               \
           "This function can only be called at runtime."); \
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

void pkSetRuntimeError(PKVM* vm, const char* message) {
  CHECK_RUNTIME();
  VM_SET_ERROR(vm, newString(vm, message));
}

void* pkGetSelf(const PKVM* vm) {
  CHECK_RUNTIME();
  ASSERT(IS_OBJ_TYPE(vm->fiber->self, OBJ_INST), OOPS);
  Instance* inst = (Instance*)AS_OBJ(vm->fiber->self);
  ASSERT(inst->native != NULL, OOPS);
  return inst->native;
}

int pkGetArgc(const PKVM* vm) {
  CHECK_RUNTIME();
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

#define VALIDATE_SLOT_INDEX(index)                                           \
  do {                                                                       \
    ASSERT(index >= 0, "Slot index was negative.");                          \
    ASSERT(index < pkGetSlotsCount(vm),                                      \
      "Slot index is too large. Did you forget to call pkReserveSlots()?."); \
  } while (false)

// ARGC won't be the real arity if any slots allocated before calling argument
// validation calling this first is the callers responsibility.
#define VALIDATE_ARGC(arg) \
  ASSERT(arg > 0 && arg <= ARGC, "Invalid argument index.")

// Set error for incompatible type provided as an argument. (TODO: got type).
#define ERR_INVALID_ARG_TYPE(ty_name)                                  \
  do {                                                                 \
    char buff[STR_INT_BUFF_SIZE];                                      \
    sprintf(buff, "%d", arg);                                          \
    VM_SET_ERROR(vm, stringFormat(vm, "Expected a '$' at argument $.", \
                                      ty_name, buff));                 \
  } while (false)

// FIXME: If the user needs just the boolean value of the object, they should
// use pkGetSlotBool().
PK_PUBLIC bool pkValidateSlotBool(PKVM* vm, int arg, bool* value) {
  CHECK_RUNTIME();
  VALIDATE_ARGC(arg);

  Var val = ARG(arg);
  if (!IS_BOOL(val)) {
    ERR_INVALID_ARG_TYPE("Boolean");
    return false;
  }

  if (value) *value = AS_BOOL(val);
  return true;
}

PK_PUBLIC bool pkValidateSlotNumber(PKVM* vm, int arg, double* value) {
  CHECK_RUNTIME();
  VALIDATE_ARGC(arg);

  Var val = ARG(arg);
  if (!IS_NUM(val)) {
    ERR_INVALID_ARG_TYPE("Number");
    return false;
  }

  if (value) *value = AS_NUM(val);
  return true;
}

PK_PUBLIC bool pkValidateSlotString(PKVM* vm, int arg, const char** value,
                                    uint32_t* length) {
  CHECK_RUNTIME();
  VALIDATE_ARGC(arg);

  Var val = ARG(arg);
  if (!IS_OBJ_TYPE(val, OBJ_STRING)) {
    ERR_INVALID_ARG_TYPE("String");
    return false;
  }
  String* str = (String*)AS_OBJ(val);
  if (value) *value = str->data;
  if (length) *length = str->length;
  return true;
}

void pkReserveSlots(PKVM* vm, int count) {
  CHECK_RUNTIME();

  int needed = (int)(vm->fiber->ret - vm->fiber->stack) + count;
  vmEnsureStackSize(vm, needed);
}

int pkGetSlotsCount(PKVM* vm) {
  CHECK_RUNTIME();

  return (int)(vm->fiber->sp - vm->fiber->ret);
}

PkVarType pkGetSlotType(PKVM* vm, int index) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  return getVarType(SLOT(index));
}

bool pkGetSlotBool(PKVM* vm, int index) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  Var value = SLOT(index);
  return toBool(value);
}

double pkGetSlotNumber(PKVM* vm, int index) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  Var value = SLOT(index);
  ASSERT(IS_NUM(value), "Slot value wasn't a Number.");
  return AS_NUM(value);
}

const char* pkGetSlotString(PKVM* vm, int index, uint32_t* length) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  Var value = SLOT(index);
  ASSERT(IS_OBJ_TYPE(value, OBJ_STRING), "Slot value wasn't a String.");
  if (length != NULL) *length = ((String*)AS_OBJ(value))->length;
  return ((String*)AS_OBJ(value))->data;
}

PkHandle* pkGetSlotHandle(PKVM* vm, int index) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  return vmNewHandle(vm, SLOT(index));
}

void pkSetSlotNull(PKVM* vm, int index) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_NULL);
}

void pkSetSlotBool(PKVM* vm, int index, bool value) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_BOOL(value));
}

void pkSetSlotNumber(PKVM* vm, int index, double value) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_NUM(value));
}

void pkSetSlotString(PKVM* vm, int index, const char* value) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newString(vm, value)));
}

PK_PUBLIC void pkSetSlotStringLength(PKVM* vm, int index,
                                     const char* value, uint32_t length) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newStringLength(vm, value, length)));
}

void PkSetSlotHandle(PKVM* vm, int index, PkHandle* handle) {
  CHECK_RUNTIME();
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, handle->value);
}

#undef CHECK_RUNTIME
#undef VALIDATE_ARGC
#undef ERR_INVALID_ARG_TYPE
#undef ARG
#undef SLOT
#undef SET_SLOT
#undef ARGC
#undef CHECK_NULL
#undef CHECK_TYPE

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
