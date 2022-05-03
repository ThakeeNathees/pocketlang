/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

// This file contains all the pocketlang public function implementations.

#include "include/pocketlang.h"

#include <stddef.h>
#include <stdio.h>

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

// The default allocator that will be used to initialize the PKVM's
// configuration if the host doesn't provided any allocators for us.
static void* defaultRealloc(void* memory, size_t new_size, void* _);

static void stderrWrite(PKVM* vm, const char* text);
static void stdoutWrite(PKVM* vm, const char* text);
static char* stdinRead(PKVM* vm);
static char* loadScript(PKVM* vm, const char* path);

char* pkAllocString(PKVM* vm, size_t size) {
  ASSERT(vm->config.realloc_fn != NULL, "PKVM's allocator was NULL.");
  return (char*) vm->config.realloc_fn(NULL, size, vm->config.user_data);
}

#define STR_TO_RAWSTR(str) \
  (_RawString*) ((((uint8_t*) (str)) - offsetof(_RawString, ptr)))

void pkDeAllocString(PKVM* vm, char* str) {
  ASSERT(vm->config.realloc_fn != NULL, "PKVM's allocator was NULL.");
  vm->config.realloc_fn(str, 0, vm->config.user_data);
}

PkConfiguration pkNewConfiguration() {
  PkConfiguration config;
  config.realloc_fn = defaultRealloc;

  config.stdout_write = stdoutWrite;
  config.stderr_write = stderrWrite;
  config.stdin_read = stdinRead;

  config.load_script_fn = loadScript;
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

  // TODO:
  // Check if the method name is valid, and validate argc for special
  // methods (like "@getter", "@call", "+", "-", etc).

  Class* class_ = (Class*)AS_OBJ(cls->value);

  Function* fn = newFunction(vm, name, (int)strlen(name),
                             class_->owner, true, NULL, NULL);
  fn->arity = arity;
  fn->native = fptr;

  // No need to push the function to temp references of the VM
  // since it's written to the constant pool of the module and the module
  // won't be garbage collected (class handle has reference to the module).

  Closure* method = newClosure(vm, fn);

  if (strcmp(name, CTOR_NAME) == 0) {
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

  Fiber* fiber = newFiber(vm, module->body);
  vmPushTempRef(vm, &fiber->_super); // fiber.
  vmPrepareFiber(vm, fiber, 0, NULL /* TODO: get argv from user. */);
  vmPopTempRef(vm); // fiber.

  return vmRunFiber(vm, fiber);
}

PK_PUBLIC PkResult pkRunFunction(PKVM* vm, PkHandle* fn,
                                 int argc, int argv_slot, int ret_slot) {
  CHECK_HANDLE_TYPE(fn, OBJ_CLOSURE);
  Closure* closure = (Closure*)AS_OBJ(fn->value);

  ASSERT(argc >= 0, "argc cannot be negative.");
  Var* argv = NULL;

  if (argc != 0) {
    for (int i = 0; i < argc; i++) {
      VALIDATE_SLOT_INDEX(argv_slot + i);
    }
    argv = &SLOT(argv_slot);
  }

  Var* ret = NULL;
  if (ret_slot >= 0) {
    VALIDATE_SLOT_INDEX(ret_slot);
    ret = &SLOT(ret_slot);
  }

  return vmRunFunction(vm, closure, argc, argv, ret);
}

/*****************************************************************************/
/* RUNTIME                                                                   */
/*****************************************************************************/

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
  CHECK_RUNTIME();
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

static void stderrWrite(PKVM* vm, const char* text) {
  fprintf(stderr, "%s", text);
}

static void stdoutWrite(PKVM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

static char* stdinRead(PKVM* vm) {

  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  char c;
  do {
    c = (char)fgetc(stdin);
    if (c == '\n') break;
    pkByteBufferWrite(&buff, vm, (uint8_t)c);
  } while (c != EOF);
  pkByteBufferWrite(&buff, vm, '\0');

  // FIXME:
  // Not sure if this is proper or even safer way to do this, but for now we're
  // casting the internal allocated struct (_RawString) to realloc the string.
  char* str = pkAllocString(vm, buff.count);
  memcpy(str, buff.data, buff.count);
  return str;
}

static char* loadScript(PKVM* vm, const char* path) {

  // FIXME:
  // This is temproary to migrate native interface here and simplify.
  // Fix the function body and implement it properly.

  // Open the file. In windows ftell isn't reliable since it'll read \r\n as
  // a single character in read mode ("r") thus we need to get the file size
  // by opening it as a binary file first and reopen it to read the text.
  FILE* file = fopen(path, "rb");
  if (file == NULL) return NULL;

  // Get the source length.
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);
  fclose(file);

  file = fopen(path, "r");

  char* str = pkAllocString(vm, file_size);
  ASSERT(str != NULL, "PKVM string allocation failed.");

  // Using read instead of file_size is because "\r\n" is read as '\n' in
  // windows.
  size_t read = fread(str, sizeof(char), file_size, file);
  str[read] = '\0';
  fclose(file);

  return str;
}
