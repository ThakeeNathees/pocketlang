/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

// This file contains all the pocketlang public function implementations.

#include "include/pocketlang.h"

#include "pk_core.h"
#include "pk_compiler.h"
#include "pk_utils.h"
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

#if TRACE_MEMORY
  void* ptr = vm->config.realloc_fn(NULL, size, vm->config.user_data);
  printf("[alloc string] alloc   : %p = %+li bytes\n", ptr, (long) size);
  return (char*) ptr;
#else
  return (char*) vm->config.realloc_fn(NULL, size, vm->config.user_data);
#endif
}

void pkDeAllocString(PKVM* vm, char* str) {
  ASSERT(vm->config.realloc_fn != NULL, "PKVM's allocator was NULL.");
#if TRACE_MEMORY
  printf("[alloc string] dealloc : %p\n", str);
#endif
  vm->config.realloc_fn(str, 0, vm->config.user_data);
}

PkConfiguration pkNewConfiguration() {
  PkConfiguration config;
  config.realloc_fn = defaultRealloc;

  config.stdout_write = stdoutWrite;
  config.stderr_write = stderrWrite;
  config.stdin_read = stdinRead;

  config.resolve_path_fn = NULL;
  config.load_script_fn = loadScript;

  config.use_ansi_color = false;
  config.user_data = NULL;

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

  DEALLOCATE(vm, vm, PKVM);
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
    pkDeAllocString(vm, resolved_);
    module->path = script_path;
    vmPopTempRef(vm); // script_path.

    initializeScript(vm, module);

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
      pkDeAllocString(vm, source);
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
  Module* _module = (Module*)AS_OBJ(module->value);

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
      pkDeAllocString(vm, line);
      break;
    }

    // If the line is empty, we don't have to compile it.
    if (isStringEmpty(line)) {
      if (need_more_lines) ASSERT(lines.count != 0, OOPS);
      pkDeAllocString(vm, line);
      continue;
    }

    // Add the line to the lines buffer.
    if (lines.count != 0) pkByteBufferWrite(&lines, vm, '\n');
    pkByteBufferAddString(&lines, vm, line, (uint32_t) line_length);
    pkDeAllocString(vm, line);
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
    result = vmRunFunction(vm, _main, 0, NULL, NULL);

  } while (!done);

  pkReleaseHandle(vm, module);

  return result;
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
    c = (char)fgetc(stdin);
    if (c == '\n') break;
    pkByteBufferWrite(&buff, vm, (uint8_t)c);
  } while (c != EOF);
  pkByteBufferWrite(&buff, vm, '\0');

  char* str = pkAllocString(vm, buff.count);
  memcpy(str, buff.data, buff.count);
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
  char* buff = pkAllocString(vm, file_size + 1);
  ASSERT(buff != NULL, "pkAllocString failed.");

  clearerr(file);
  size_t read = fread(buff, sizeof(char), file_size, file);
  ASSERT(read <= file_size, "fread() failed.");
  buff[read] = '\0';
  fclose(file);

  return buff;
}
