/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

#include "pk_var.h"

#include <math.h>
#include <ctype.h>

#include "pk_utils.h"
#include "pk_vm.h"

/*****************************************************************************/
/* VAR PUBLIC API                                                            */
/*****************************************************************************/

PkVarType pkGetValueType(const PkVar value) {
  __ASSERT(value != NULL, "Given value was NULL.");

  if (IS_NULL(*(const Var*)(value))) return PK_NULL;
  if (IS_BOOL(*(const Var*)(value))) return PK_BOOL;
  if (IS_NUM(*(const Var*)(value))) return PK_NUMBER;

  __ASSERT(IS_OBJ(*(const Var*)(value)),
           "Invalid var pointer. Might be a dangling pointer");

  const Object* obj = AS_OBJ(*(const Var*)(value));
  switch (obj->type) {
    case OBJ_STRING: return PK_STRING;
    case OBJ_LIST:   return PK_LIST;
    case OBJ_MAP:    return PK_MAP;
    case OBJ_RANGE:  return PK_RANGE;
    case OBJ_SCRIPT: return PK_SCRIPT;
    case OBJ_FUNC:   return PK_FUNCTION;
    case OBJ_FIBER:  return PK_FIBER;
    case OBJ_CLASS:  return PK_CLASS;
    case OBJ_INST:   return PK_INST;
  }

  UNREACHABLE();
}

PkHandle* pkNewString(PKVM* vm, const char* value) {
  String* str = newString(vm, value);
  vmPushTempRef(vm, &str->_super); // str
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(str));
  vmPopTempRef(vm); // str
  return handle;
}

PkHandle* pkNewStringLength(PKVM* vm, const char* value, size_t len) {
  String* str = newStringLength(vm, value, (uint32_t)len);
  vmPushTempRef(vm, &str->_super); // str
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(str));
  vmPopTempRef(vm); // str
  return handle;
}

PkHandle* pkNewList(PKVM* vm) {
  List* list = newList(vm, MIN_CAPACITY);
  vmPushTempRef(vm, &list->_super); // list
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(list));
  vmPopTempRef(vm); // list
  return handle;
}

PkHandle* pkNewMap(PKVM* vm) {
  Map* map = newMap(vm);
  vmPushTempRef(vm, &map->_super); // map
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(map));
  vmPopTempRef(vm); // map
  return handle;
}

PkHandle* pkNewFiber(PKVM* vm, PkHandle* fn) {
  __ASSERT(IS_OBJ_TYPE(fn->value, OBJ_FUNC), "Fn should be of type function.");

  Fiber* fiber = newFiber(vm, (Function*)AS_OBJ(fn->value));
  vmPushTempRef(vm, &fiber->_super); // fiber
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(fiber));
  vmPopTempRef(vm); // fiber
  return handle;
}

PkHandle* pkNewInstNative(PKVM* vm, void* data, uint32_t id) {
  Instance* inst = newInstanceNative(vm, data, id);
  vmPushTempRef(vm, &inst->_super); // inst
  PkHandle* handle = vmNewHandle(vm, VAR_OBJ(inst));
  vmPopTempRef(vm); // inst
  return handle;
}

/*****************************************************************************/
/* VAR INTERNALS                                                             */
/*****************************************************************************/

// The maximum percentage of the map entries that can be filled before the map
// is grown. A lower percentage reduce collision which makes looks up faster
// but take more memory.
#define MAP_LOAD_PERCENT 75

// The factor a collection would grow by when it's exceeds the current
// capacity. The new capacity will be calculated by multiplying it's old
// capacity by the GROW_FACTOR.
#define GROW_FACTOR 2

// Buffer implementations.
DEFINE_BUFFER(Uint, uint32_t)
DEFINE_BUFFER(Byte, uint8_t)
DEFINE_BUFFER(Var, Var)
DEFINE_BUFFER(String, String*)
DEFINE_BUFFER(Function, Function*)
DEFINE_BUFFER(Class, Class*)

void pkByteBufferAddString(pkByteBuffer* self, PKVM* vm, const char* str,
                           uint32_t length) {
  pkByteBufferReserve(self, vm, self->count + length);
  for (uint32_t i = 0; i < length; i++) {
    self->data[self->count++] = *(str++);
  }
}

void varInitObject(Object* self, PKVM* vm, ObjectType type) {
  self->type = type;
  self->is_marked = false;
  self->next = vm->first;
  vm->first = self;
}

void markObject(PKVM* vm, Object* self) {
  if (self == NULL || self->is_marked) return;
  self->is_marked = true;

  // Add the object to the VM's working_set so that we can recursively mark
  // its referenced objects later.
  if (vm->working_set_count >= vm->working_set_capacity) {
    vm->working_set_capacity *= 2;
    vm->working_set = (Object**)vm->config.realloc_fn(
                                    vm->working_set,
                                    vm->working_set_capacity * sizeof(Object*),
                                    vm->config.user_data);
  }

  vm->working_set[vm->working_set_count++] = self;
}

void markValue(PKVM* vm, Var self) {
  if (!IS_OBJ(self)) return;
  markObject(vm, AS_OBJ(self));
}

void markVarBuffer(PKVM* vm, pkVarBuffer* self) {
  if (self == NULL) return;
  for (uint32_t i = 0; i < self->count; i++) {
    markValue(vm, self->data[i]);
  }
}

#define MARK_OBJ_BUFFER(m_name)                                   \
  void mark##m_name##Buffer(PKVM* vm, pk##m_name##Buffer* self) { \
    if (self == NULL) return;                                     \
    for (uint32_t i = 0; i < self->count; i++) {                  \
      markObject(vm, &self->data[i]->_super);                     \
    }                                                             \
  }

MARK_OBJ_BUFFER(String)
MARK_OBJ_BUFFER(Function)
MARK_OBJ_BUFFER(Class)

#undef MARK_OBJ_BUFFER

static void popMarkedObjectsInternal(Object* obj, PKVM* vm) {
  // TODO: trace here.

  switch (obj->type) {
    case OBJ_STRING: {
      vm->bytes_allocated += sizeof(String);
      vm->bytes_allocated += ((size_t)((String*)obj)->length + 1);
    } break;

    case OBJ_LIST: {
      List* list = (List*)obj;
      markVarBuffer(vm, &list->elements);
      vm->bytes_allocated += sizeof(List);
      vm->bytes_allocated += sizeof(Var) * list->elements.capacity;
    } break;

    case OBJ_MAP: {
      Map* map = (Map*)obj;
      for (uint32_t i = 0; i < map->capacity; i++) {
        if (IS_UNDEF(map->entries[i].key)) continue;
        markValue(vm, map->entries[i].key);
        markValue(vm, map->entries[i].value);
      }
      vm->bytes_allocated += sizeof(Map);
      vm->bytes_allocated += sizeof(MapEntry) * map->capacity;
    } break;

    case OBJ_RANGE: {
      vm->bytes_allocated += sizeof(Range);
    } break;

    case OBJ_SCRIPT:
    {
      Script* scr = (Script*)obj;
      vm->bytes_allocated += sizeof(Script);

      markObject(vm, &scr->path->_super);
      markObject(vm, &scr->module->_super);

      markVarBuffer(vm, &scr->globals);
      vm->bytes_allocated += sizeof(Var) * scr->globals.capacity;

      // Integer buffer has no gray call.
      vm->bytes_allocated += sizeof(uint32_t) * scr->global_names.capacity;

      markVarBuffer(vm, &scr->literals);
      vm->bytes_allocated += sizeof(Var) * scr->literals.capacity;

      markFunctionBuffer(vm, &scr->functions);
      vm->bytes_allocated += sizeof(Function*) * scr->functions.capacity;

      markClassBuffer(vm, &scr->classes);
      vm->bytes_allocated += sizeof(Class*) * scr->classes.count;

      markStringBuffer(vm, &scr->names);
      vm->bytes_allocated += sizeof(String*) * scr->names.capacity;

      markObject(vm, &scr->body->_super);
    } break;

    case OBJ_FUNC:
    {
      Function* func = (Function*)obj;
      vm->bytes_allocated += sizeof(Function);

      markObject(vm, &func->owner->_super);

      if (!func->is_native) {
        Fn* fn = func->fn;
        vm->bytes_allocated += sizeof(Fn);

        vm->bytes_allocated += sizeof(uint8_t)* fn->opcodes.capacity;
        vm->bytes_allocated += sizeof(uint32_t) * fn->oplines.capacity;
      }
    } break;

    case OBJ_FIBER:
    {
      Fiber* fiber = (Fiber*)obj;
      vm->bytes_allocated += sizeof(Fiber);

      markObject(vm, &fiber->func->_super);

      // Blacken the stack.
      for (Var* local = fiber->stack; local < fiber->sp; local++) {
        markValue(vm, *local);
      }
      vm->bytes_allocated += sizeof(Var) * fiber->stack_size;

      // Blacken call frames.
      for (int i = 0; i < fiber->frame_count; i++) {
        markObject(vm, (Object*)&fiber->frames[i].fn->_super);
        markObject(vm, &fiber->frames[i].fn->owner->_super);
      }
      vm->bytes_allocated += sizeof(CallFrame) * fiber->frame_capacity;

      markObject(vm, &fiber->caller->_super);
      markObject(vm, &fiber->error->_super);

    } break;

    case OBJ_CLASS:
    {
      Class* type = (Class*)obj;
      vm->bytes_allocated += sizeof(Class);
      markObject(vm, &type->owner->_super);
      markObject(vm, &type->ctor->_super);
      vm->bytes_allocated += sizeof(uint32_t) * type->field_names.capacity;
    } break;

    case OBJ_INST:
    {
      Instance* inst = (Instance*)obj;
      if (!inst->is_native) {
        Inst* ins = inst->ins;
        vm->bytes_allocated += sizeof(Inst);
        vm->bytes_allocated += sizeof(Var*) * ins->fields.capacity;
      }
    } break;
  }
}

void popMarkedObjects(PKVM* vm) {
  while (vm->working_set_count > 0) {
    Object* marked_obj = vm->working_set[--vm->working_set_count];
    popMarkedObjectsInternal(marked_obj, vm);
  }
}

Var doubleToVar(double value) {
#if VAR_NAN_TAGGING
  return utilDoubleToBits(value);
#else
#error TODO:
#endif // VAR_NAN_TAGGING
}

double varToDouble(Var value) {
#if VAR_NAN_TAGGING
  return utilDoubleFromBits(value);
#else
  #error TODO:
#endif // VAR_NAN_TAGGING
}

static String* _allocateString(PKVM* vm, size_t length) {
  String* string = ALLOCATE_DYNAMIC(vm, String, length + 1, char);
  varInitObject(&string->_super, vm, OBJ_STRING);
  string->length = (uint32_t)length;
  string->data[length] = '\0';
  string->capacity = (uint32_t)(length + 1);
  return string;
}

String* newStringLength(PKVM* vm, const char* text, uint32_t length) {

  ASSERT(length == 0 || text != NULL, "Unexpected NULL string.");

  String* string = _allocateString(vm, length);

  if (length != 0 && text != NULL) memcpy(string->data, text, length);
  string->hash = utilHashString(string->data);

  return string;
}

List* newList(PKVM* vm, uint32_t size) {
  List* list = ALLOCATE(vm, List);
  vmPushTempRef(vm, &list->_super);
  varInitObject(&list->_super, vm, OBJ_LIST);
  pkVarBufferInit(&list->elements);
  if (size > 0) {
    pkVarBufferFill(&list->elements, vm, VAR_NULL, size);
    list->elements.count = 0;
  }
  vmPopTempRef(vm);
  return list;
}

Map* newMap(PKVM* vm) {
  Map* map = ALLOCATE(vm, Map);
  varInitObject(&map->_super, vm, OBJ_MAP);
  map->capacity = 0;
  map->count = 0;
  map->entries = NULL;
  return map;
}

Range* newRange(PKVM* vm, double from, double to) {
  Range* range = ALLOCATE(vm, Range);
  varInitObject(&range->_super, vm, OBJ_RANGE);
  range->from = from;
  range->to = to;
  return range;
}

Script* newScript(PKVM* vm, String* name, bool is_core) {
  Script* script = ALLOCATE(vm, Script);
  varInitObject(&script->_super, vm, OBJ_SCRIPT);

  ASSERT(name != NULL && name->length > 0, OOPS);

  script->path = name;
  script->module = NULL;
  script->initialized = is_core;
  script->body = NULL;

  // Core modules has its name as the module name.
  if (is_core) script->module = name;

  pkVarBufferInit(&script->globals);
  pkUintBufferInit(&script->global_names);
  pkVarBufferInit(&script->literals);
  pkFunctionBufferInit(&script->functions);
  pkClassBufferInit(&script->classes);
  pkStringBufferInit(&script->names);

  // Add a implicit main function and the '__file__' global to the module, only
  // if it's not a core module.
  if (!is_core) {
    vmPushTempRef(vm, &script->_super);
    scriptAddMain(vm, script);

    // Add '__file__' variable with it's path as value. If the path starts with
    // '$' It's a special file ($(REPL) or $(TRY)) and don't define __file__.
    if (script->path->data[0] != '$') {
      scriptAddGlobal(vm, script, "__file__", 8, VAR_OBJ(script->path));
    }

    // TODO: Add ARGV as a global.

    vmPopTempRef(vm); // script.
  }

  return script;
}

Function* newFunction(PKVM* vm, const char* name, int length, Script* owner,
                      bool is_native, const char* docstring) {

  Function* func = ALLOCATE(vm, Function);
  varInitObject(&func->_super, vm, OBJ_FUNC);

  vmPushTempRef(vm, &func->_super); // func

  if (owner == NULL) {
    ASSERT(is_native, OOPS);
    func->name = name;
    func->owner = NULL;
    func->is_native = is_native;

  } else {
    pkFunctionBufferWrite(&owner->functions, vm, func);
    uint32_t name_index = scriptAddName(owner, vm, name, length);

    func->name = owner->names.data[name_index]->data;
    func->owner = owner;
    func->arity = -2; // -1 means variadic args.
    func->is_native = is_native;
  }

  if (is_native) {
    func->native = NULL;

  } else {
    Fn* fn = ALLOCATE(vm, Fn);
    pkByteBufferInit(&fn->opcodes);
    pkUintBufferInit(&fn->oplines);
    fn->stack_size = 0;
    func->fn = fn;
  }

  // Both native and script (TODO:) functions support docstring.
  func->docstring = docstring;

  vmPopTempRef(vm); // func
  return func;
}

Fiber* newFiber(PKVM* vm, Function* fn) {
  Fiber* fiber = ALLOCATE(vm, Fiber);
  memset(fiber, 0, sizeof(Fiber));
  varInitObject(&fiber->_super, vm, OBJ_FIBER);

  fiber->state = FIBER_NEW;
  fiber->func = fn;

  if (fn->is_native) {
    // For native functions, we're only using stack for parameters,
    // there won't be any locals or temps (which are belongs to the
    // native "C" stack).
    int stack_size = utilPowerOf2Ceil(fn->arity + 1);
    fiber->stack = ALLOCATE_ARRAY(vm, Var, stack_size);
    fiber->stack_size = stack_size;
    fiber->ret = fiber->stack;
    fiber->sp = fiber->stack + 1;

  } else {
    // Allocate stack.
    int stack_size = utilPowerOf2Ceil(fn->fn->stack_size + 1);
    if (stack_size < MIN_STACK_SIZE) stack_size = MIN_STACK_SIZE;
    fiber->stack = ALLOCATE_ARRAY(vm, Var, stack_size);
    fiber->stack_size = stack_size;
    fiber->ret = fiber->stack;
    fiber->sp = fiber->stack + 1;

    // Allocate call frames.
    fiber->frame_capacity = INITIAL_CALL_FRAMES;
    fiber->frames = ALLOCATE_ARRAY(vm, CallFrame, fiber->frame_capacity);
    fiber->frame_count = 1;

    // Initialize the first frame.
    fiber->frames[0].fn = fn;
    fiber->frames[0].ip = fn->fn->opcodes.data;
    fiber->frames[0].rbp = fiber->ret;
  }

  // Initialize the return value to null (doesn't really have to do that here
  // but if we're trying to debut it may crash when dumping the return value).
  *fiber->ret = VAR_NULL;

  return fiber;
}

Class* newClass(PKVM* vm, Script* scr, const char* name, uint32_t length) {
  Class* type = ALLOCATE(vm, Class);
  varInitObject(&type->_super, vm, OBJ_CLASS);

  vmPushTempRef(vm, &type->_super); // type.

  pkClassBufferWrite(&scr->classes, vm, type);
  type->owner = scr;
  type->name = scriptAddName(scr, vm, name, length);
  pkUintBufferInit(&type->field_names);

  // Can't use '$' in string format. (TODO)
  String* ty_name = scr->names.data[type->name];
  String* dollar = newStringLength(vm, "$", 1);
  vmPushTempRef(vm, &dollar->_super); // dollar
  String* ctor_name = stringFormat(vm, "@(Ctor:@)", dollar, ty_name);
  vmPopTempRef(vm); // dollar

  // Constructor.
  vmPushTempRef(vm, &ctor_name->_super); // ctor_name
  type->ctor = newFunction(vm, ctor_name->data, ctor_name->length,
                           scr, false, NULL);
  vmPopTempRef(vm); // ctor_name

  vmPopTempRef(vm); // type.
  return type;
}

Instance* newInstance(PKVM* vm, Class* ty, bool initialize) {

  Instance* inst = ALLOCATE(vm, Instance);
  varInitObject(&inst->_super, vm, OBJ_INST);

  vmPushTempRef(vm, &inst->_super); // inst.

  ASSERT(ty->name < ty->owner->names.count, OOPS);
  inst->name = ty->owner->names.data[ty->name]->data;
  inst->is_native = false;

  Inst* ins = ALLOCATE(vm, Inst);
  inst->ins = ins;
  ins->type = ty;
  pkVarBufferInit(&ins->fields);

  if (initialize && ty->field_names.count != 0) {
    pkVarBufferFill(&ins->fields, vm, VAR_NULL, ty->field_names.count);
  }

  vmPopTempRef(vm); // inst.

  return inst;
}

Instance* newInstanceNative(PKVM* vm, void* data, uint32_t id) {
  Instance* inst = ALLOCATE(vm, Instance);
  varInitObject(&inst->_super, vm, OBJ_INST);
  inst->is_native = true;
  inst->native_id = id;

  if (vm->config.inst_name_fn != NULL) {
    inst->name = vm->config.inst_name_fn(id);
  } else {
    inst->name = "$(?)";
  }

  inst->native = data;
  return inst;
}

List* rangeAsList(PKVM* vm, Range* self) {
  List* list;
  if (self->from < self->to) {
    list = newList(vm, (uint32_t)(self->to - self->from));
    for (double i = self->from; i < self->to; i++) {
      pkVarBufferWrite(&list->elements, vm, VAR_NUM(i));
    }
    return list;

  } else {
    list = newList(vm, 0);
  }

  return list;
}

String* stringLower(PKVM* vm, String* self) {
  // If the string itself is already lower, don't allocate new string.
  uint32_t index = 0;
  for (const char* c = self->data; *c != '\0'; c++, index++) {
    if (isupper(*c)) {

      // It contain upper case letters, allocate new lower case string .
      String* lower = newStringLength(vm, self->data, self->length);

      // Start where the first upper case letter found.
      char* _c = lower->data + (c - self->data);
      for (; *_c != '\0'; _c++) *_c = (char)tolower(*_c);

      // Since the string is modified re-hash it.
      lower->hash = utilHashString(lower->data);
      return lower;
    }
  }
  // If we reached here the string itself is lower, return it.
  return self;
}

String* stringUpper(PKVM* vm, String* self) {
  // If the string itself is already upper don't allocate new string.
  uint32_t index = 0;
  for (const char* c = self->data; *c != '\0'; c++, index++) {
    if (islower(*c)) {
      // It contain lower case letters, allocate new upper case string .
      String* upper = newStringLength(vm, self->data, self->length);

      // Start where the first lower case letter found.
      char* _c = upper->data + (c - self->data);
      for (; *_c != '\0'; _c++) *_c = (char)toupper(*_c);

      // Since the string is modified re-hash it.
      upper->hash = utilHashString(upper->data);
      return upper;
    }
  }
  // If we reached here the string itself is lower, return it.
  return self;
}

String* stringStrip(PKVM* vm, String* self) {

  // Implementation:
  //
  // "     a string with leading and trailing white space    "
  //  ^start >>                                       << end^
  //
  // These 'start' and 'end' pointers will move respectively right and left
  // while it's a white space and return an allocated string from 'start' with
  // length of (end - start + 1). For already trimmed string it'll not allocate
  // a new string, instead returns the same string provided.

  const char* start = self->data;
  while (*start && isspace(*start)) start++;

  // If we reached the end of the string, it's all white space, return
  // an empty string.
  if (*start == '\0') {
    return newStringLength(vm, NULL, 0);
  }

  const char* end = self->data + self->length - 1;
  while (isspace(*end)) end--;

  // If the string is already trimmed, return the same string.
  if (start == self->data && end == self->data + self->length - 1) {
    return self;
  }

  return newStringLength(vm, start, (uint32_t)(end - start + 1));
}

String* stringFormat(PKVM* vm, const char* fmt, ...) {
  va_list arg_list;

  // Calculate the total length of the resulting string. This is required to
  // determine the final string size to allocate.
  va_start(arg_list, fmt);
  size_t total_length = 0;
  for (const char* c = fmt; *c != '\0'; c++) {
    switch (*c) {
      case '$':
        total_length += strlen(va_arg(arg_list, const char*));
        break;

      case '@':
        total_length += va_arg(arg_list, String*)->length;
        break;

      default:
        total_length++;
    }
  }
  va_end(arg_list);

  // Now build the new string.
  String* result = _allocateString(vm, total_length);
  va_start(arg_list, fmt);
  char* buff = result->data;
  for (const char* c = fmt; *c != '\0'; c++) {
    switch (*c) {
      case '$':
      {
        const char* string = va_arg(arg_list, const char*);
        size_t length = strlen(string);
        memcpy(buff, string, length);
        buff += length;
      } break;

      case '@':
      {
        String* string = va_arg(arg_list, String*);
        memcpy(buff, string->data, string->length);
        buff += string->length;
      } break;

      default:
      {
        *buff++ = *c;
      } break;
    }
  }
  va_end(arg_list);

  result->hash = utilHashString(result->data);
  return result;
}

String* stringJoin(PKVM* vm, String* str1, String* str2) {

  // Optimize end case.
  if (str1->length == 0) return str2;
  if (str2->length == 0) return str1;

  size_t length = (size_t)str1->length + (size_t)str2->length;
  String* string = _allocateString(vm, length);

  memcpy(string->data, str1->data, str1->length);
  memcpy(string->data + str1->length, str2->data, str2->length);
  // Null byte already existed. From _allocateString.

  string->hash = utilHashString(string->data);
  return string;
}

void listInsert(PKVM* vm, List* self, uint32_t index, Var value) {

  // Add an empty slot at the end of the buffer.
  if (IS_OBJ(value)) vmPushTempRef(vm, AS_OBJ(value));
  pkVarBufferWrite(&self->elements, vm, VAR_NULL);
  if (IS_OBJ(value)) vmPopTempRef(vm);

  // Shift the existing elements down.
  for (uint32_t i = self->elements.count - 1; i > index; i--) {
    self->elements.data[i] = self->elements.data[i - 1];
  }

  // Insert the new element.
  self->elements.data[index] = value;
}

Var listRemoveAt(PKVM* vm, List* self, uint32_t index) {
  Var removed = self->elements.data[index];
  if (IS_OBJ(removed)) vmPushTempRef(vm, AS_OBJ(removed));

  // Shift the rest of the elements up.
  for (uint32_t i = index; i < self->elements.count - 1; i++) {
    self->elements.data[i] = self->elements.data[i + 1];
  }

  // Shrink the size if it's too much excess.
  if (self->elements.capacity / GROW_FACTOR >= self->elements.count) {
    self->elements.data = (Var*)vmRealloc(vm, self->elements.data,
      sizeof(Var) * self->elements.capacity,
      sizeof(Var) * self->elements.capacity / GROW_FACTOR);
    self->elements.capacity /= GROW_FACTOR;
  }

  if (IS_OBJ(removed)) vmPopTempRef(vm);

  self->elements.count--;
  return removed;
}

List* listJoin(PKVM* vm, List* l1, List* l2) {

  // Optimize end case.
  if (l1->elements.count == 0) return l2;
  if (l2->elements.count == 0) return l1;

  uint32_t size = l1->elements.count + l2->elements.count;
  List* list = newList(vm, size);

  vmPushTempRef(vm, &list->_super);
  pkVarBufferConcat(&list->elements, vm, &l1->elements);
  pkVarBufferConcat(&list->elements, vm, &l2->elements);
  vmPopTempRef(vm);

  return list;
}

// Return a hash value for the object.
static uint32_t _hashObject(Object* obj) {

  ASSERT(isObjectHashable(obj->type),
         "Check if it's hashable before calling this method.");

  switch (obj->type) {

    case OBJ_STRING:
      return ((String*)obj)->hash;

    case OBJ_LIST:
    case OBJ_MAP:
      goto L_unhashable;

    case OBJ_RANGE:
    {
      Range* range = (Range*)obj;
      return utilHashNumber(range->from) ^ utilHashNumber(range->to);
    }

    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_CLASS:
    case OBJ_INST:
      TODO;
      UNREACHABLE();

    default:
    L_unhashable:
      UNREACHABLE();
      break;
  }

  UNREACHABLE();
}

uint32_t varHashValue(Var v) {
  if (IS_OBJ(v)) return _hashObject(AS_OBJ(v));

#if VAR_NAN_TAGGING
  return utilHashBits(v);
#else
  #error TODO:
#endif
}

// Find the entry with the [key]. Returns true if found and set [result] to
// point to the entry, return false otherwise and points [result] to where
// the entry should be inserted.
static bool _mapFindEntry(Map* self, Var key, MapEntry** result) {

  // An empty map won't contain the key.
  if (self->capacity == 0) return false;

  // The [start_index] is where the entry supposed to be if there wasn't any
  // collision occurred. It'll be the start index for the linear probing.
  uint32_t start_index = varHashValue(key) % self->capacity;
  uint32_t index = start_index;

  // Keep track of the first tombstone after the [start_index] if we don't find
  // the key anywhere. The tombstone would be the entry at where we will have
  // to insert the key/value pair.
  MapEntry* tombstone = NULL;

  do {
    MapEntry* entry = &self->entries[index];

    if (IS_UNDEF(entry->key)) {
      ASSERT(IS_BOOL(entry->value), OOPS);

      if (IS_TRUE(entry->value)) {

        // We've found a tombstone, if we haven't found one [tombstone] should
        // be updated. We still need to keep search for if the key exists.
        if (tombstone == NULL) tombstone = entry;

      } else {
        // We've found a new empty slot and the key isn't found. If we've
        // found a tombstone along the sequence we could use that entry
        // otherwise the entry at the current index.

        *result = (tombstone != NULL) ? tombstone : entry;
        return false;
      }

    } else if (isValuesEqual(entry->key, key)) {
      // We've found the key.
      *result = entry;
      return true;
    }

    index = (index + 1) % self->capacity;

  } while (index != start_index);

  // If we reach here means the map is filled with tombstone. Set the first
  // tombstone as result for the next insertion and return false.
  ASSERT(tombstone != NULL, OOPS);
  *result = tombstone;
  return false;
}

// Add the key, value pair to the entries array of the map. Returns true if
// the entry added for the first time and false for replaced value.
static bool _mapInsertEntry(Map* self, Var key, Var value) {

  ASSERT(self->capacity != 0, "Should ensure the capacity before inserting.");

  MapEntry* result;
  if (_mapFindEntry(self, key, &result)) {
    // Key already found, just replace the value.
    result->value = value;
    return false;
  } else {
    result->key = key;
    result->value = value;
    return true;
  }
}

// Resize the map's size to the given [capacity].
static void _mapResize(PKVM* vm, Map* self, uint32_t capacity) {

  MapEntry* old_entries = self->entries;
  uint32_t old_capacity = self->capacity;

  self->entries = ALLOCATE_ARRAY(vm, MapEntry, capacity);
  self->capacity = capacity;
  for (uint32_t i = 0; i < capacity; i++) {
    self->entries[i].key = VAR_UNDEFINED;
    self->entries[i].value = VAR_FALSE;
  }

  // Insert the old entries to the new entries.
  for (uint32_t i = 0; i < old_capacity; i++) {
    // Skip the empty entries or tombstones.
    if (IS_UNDEF(old_entries[i].key)) continue;

    _mapInsertEntry(self, old_entries[i].key, old_entries[i].value);
  }

  DEALLOCATE(vm, old_entries);
}

Var mapGet(Map* self, Var key) {
  MapEntry* entry;
  if (_mapFindEntry(self, key, &entry)) return entry->value;
  return VAR_UNDEFINED;
}

void mapSet(PKVM* vm, Map* self, Var key, Var value) {

  // If map is about to fill, resize it first.
  if (self->count + 1 > self->capacity * MAP_LOAD_PERCENT / 100) {
    uint32_t capacity = self->capacity * GROW_FACTOR;
    if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;
    _mapResize(vm, self, capacity);
  }

  if (_mapInsertEntry(self, key, value)) {
    self->count++; //< A new key added.
  }
}

void mapClear(PKVM* vm, Map* self) {
  DEALLOCATE(vm, self->entries);
  self->entries = NULL;
  self->capacity = 0;
  self->count = 0;
}

Var mapRemoveKey(PKVM* vm, Map* self, Var key) {
  MapEntry* entry;
  if (!_mapFindEntry(self, key, &entry)) return VAR_NULL;

  // Set the key as VAR_UNDEFINED to mark is as an available slow and set it's
  // value to VAR_TRUE for tombstone.
  Var value = entry->value;
  entry->key = VAR_UNDEFINED;
  entry->value = VAR_TRUE;

  self->count--;

  if (IS_OBJ(value)) vmPushTempRef(vm, AS_OBJ(value));

  if (self->count == 0) {
    // Clear the map if it's empty.
    mapClear(vm, self);

 } else if ((self->capacity > MIN_CAPACITY) &&
             (self->capacity / (GROW_FACTOR * GROW_FACTOR)) >
             ((self->count * 100) / MAP_LOAD_PERCENT)) {

    // We grow the map when it's filled 75% (MAP_LOAD_PERCENT) by 2
    // (GROW_FACTOR) but we're not shrink the map when it's half filled (ie.
    // half of the capacity is 75%). Instead we wait till it'll become 1/4 is
    // filled (1/4 = 1/(GROW_FACTOR*GROW_FACTOR)) to minimize the
    // reallocations and which is more faster.

    uint32_t capacity = self->capacity / (GROW_FACTOR * GROW_FACTOR);
    if (capacity < MIN_CAPACITY) capacity = MIN_CAPACITY;

    _mapResize(vm, self, capacity);
  }

  if (IS_OBJ(value)) vmPopTempRef(vm);

  return value;
}

bool fiberHasError(Fiber* fiber) {
  return fiber->error != NULL;
}

void freeObject(PKVM* vm, Object* self) {
  // TODO: Debug trace memory here.

  // First clean the object's references, but we're not recursively
  // deallocating them because they're not marked and will be cleaned later.
  // Example: List's `elements` is VarBuffer that contain a heap allocated
  // array of `var*` which will be cleaned below but the actual `var` elements
  // will won't be freed here instead they haven't marked at all, and will be
  // removed at the sweeping phase of the garbage collection.
  switch (self->type) {
    case OBJ_STRING:
      break;

    case OBJ_LIST:
      pkVarBufferClear(&(((List*)self)->elements), vm);
      break;

    case OBJ_MAP:
      DEALLOCATE(vm, ((Map*)self)->entries);
      break;

    case OBJ_RANGE:
      break;

    case OBJ_SCRIPT: {
      Script* scr = (Script*)self;
      pkVarBufferClear(&scr->globals, vm);
      pkUintBufferClear(&scr->global_names, vm);
      pkVarBufferClear(&scr->literals, vm);
      pkFunctionBufferClear(&scr->functions, vm);
      pkClassBufferClear(&scr->classes, vm);
      pkStringBufferClear(&scr->names, vm);
    } break;

    case OBJ_FUNC: {
      Function* func = (Function*)self;
      if (!func->is_native) {
        pkByteBufferClear(&func->fn->opcodes, vm);
        pkUintBufferClear(&func->fn->oplines, vm);
        DEALLOCATE(vm, func->fn);
      }
    } break;

    case OBJ_FIBER: {
      Fiber* fiber = (Fiber*)self;
      DEALLOCATE(vm, fiber->stack);
      DEALLOCATE(vm, fiber->frames);
    } break;

    case OBJ_CLASS: {
      Class* type = (Class*)self;
      pkUintBufferClear(&type->field_names, vm);
    } break;

    case OBJ_INST:
    {
      Instance* inst = (Instance*)self;

      if (inst->is_native) {
        if (vm->config.inst_free_fn != NULL) {
          // TODO: Allow user to set error when freeing the object.
          vm->config.inst_free_fn(vm, inst->native, inst->native_id);
        }

      } else {
        Inst* ins = inst->ins;
        pkVarBufferClear(&ins->fields, vm);
        DEALLOCATE(vm, ins);
      }

      break;
    }
  }

  DEALLOCATE(vm, self);
}

uint32_t scriptAddName(Script* self, PKVM* vm, const char* name,
  uint32_t length) {

  for (uint32_t i = 0; i < self->names.count; i++) {
    String* _name = self->names.data[i];
    if (_name->length == length && strncmp(_name->data, name, length) == 0) {
      // Name already exists in the buffer.
      return i;
    }
  }

  // If we reach here the name doesn't exists in the buffer, so add it and
  // return the index.
  String* new_name = newStringLength(vm, name, length);
  vmPushTempRef(vm, &new_name->_super);
  pkStringBufferWrite(&self->names, vm, new_name);
  vmPopTempRef(vm);
  return self->names.count - 1;
}

int scriptGetClass(Script* script, const char* name, uint32_t length) {
  for (uint32_t i = 0; i < script->classes.count; i++) {
    uint32_t name_ind = script->classes.data[i]->name;
    ASSERT(name_ind < script->names.count, OOPS);
    String* ty_name = script->names.data[name_ind];
    if (ty_name->length == length &&
        strncmp(ty_name->data, name, length) == 0) {
      return (int)i;
    }
  }
  return -1;
}

int scriptGetFunc(Script* script, const char* name, uint32_t length) {
  for (uint32_t i = 0; i < script->functions.count; i++) {
    const char* fn_name = script->functions.data[i]->name;
    uint32_t fn_length = (uint32_t)strlen(fn_name);
    if (fn_length == length && strncmp(fn_name, name, length) == 0) {
      return (int)i;
    }
  }
  return -1;
}

int scriptGetGlobals(Script* script, const char* name, uint32_t length) {
  for (uint32_t i = 0; i < script->global_names.count; i++) {
    uint32_t name_index = script->global_names.data[i];
    String* g_name = script->names.data[name_index];
    if (g_name->length == length && strncmp(g_name->data, name, length) == 0) {
      return (int)i;
    }
  }
  return -1;
}

uint32_t scriptAddGlobal(PKVM* vm, Script* script,
                    const char* name, uint32_t length,
                    Var value) {

  // If already exists update the value.
  int var_ind = scriptGetGlobals(script, name, length);
  if (var_ind != -1) {
    ASSERT(var_ind < (int)script->globals.count, OOPS);
    script->globals.data[var_ind] = value;
    return var_ind;
  }

  // If we're reached here that means we don't already have a variable with
  // that name, create new one and set the value.
  uint32_t name_ind = scriptAddName(script, vm, name, length);
  pkUintBufferWrite(&script->global_names, vm, name_ind);
  pkVarBufferWrite(&script->globals, vm, value);
  return script->globals.count - 1;
}

void scriptAddMain(PKVM* vm, Script* script) {
  ASSERT(script->body == NULL, OOPS);

  const char* fn_name = PK_IMPLICIT_MAIN_NAME;
  script->body = newFunction(vm, fn_name, (int)strlen(fn_name),
                             script, false, NULL/*TODO*/);
  script->body->arity = 0;
  script->initialized = false;
}

bool instGetAttrib(PKVM* vm, Instance* inst, String* attrib, Var* value) {
  ASSERT(inst != NULL, OOPS);
  ASSERT(attrib != NULL, OOPS);
  ASSERT(value != NULL, OOPS);

  // This function should only be called at runtime.
  ASSERT(vm->fiber != NULL, OOPS);

  if (inst->is_native) {

    if (vm->config.inst_get_attrib_fn) {
      // Temproarly change the fiber's "return address" to points to the
      // below var 'val' so that the users can use 'pkReturn...()' function
      // to return the attribute as well.
      Var* temp = vm->fiber->ret;
      Var val = VAR_UNDEFINED;

      vm->fiber->ret = &val;
      PkStringPtr attr = { attrib->data, NULL, NULL,
                           attrib->length, attrib->hash };
      vm->config.inst_get_attrib_fn(vm, inst->native, inst->native_id, attr);
      vm->fiber->ret = temp;

      if (IS_UNDEF(val)) {

        // FIXME: add a list of attribute overrides.
        if (IS_CSTR_EQ(attrib, "as_string", 9,
          CHECK_HASH("as_string", 0xbdef4147))) {
          *value = VAR_OBJ(toRepr(vm, VAR_OBJ(inst)));
          return true;
        }

        // If we reached here, the native instance don't have the attribute
        // and no overriden attributes found, return false to indicate that the
        // attribute doesn't exists.
        return false;
      }

      // Attribute [val] updated by the hosting application.
      *value = val;
      return true;
    }

    // If the hosting application doesn't provided a getter function, we treat
    // it as if the instance don't has the attribute.
    return false;

  } else {

    // TODO: Optimize this with binary search.
    Class* ty = inst->ins->type;
    for (uint32_t i = 0; i < ty->field_names.count; i++) {
      ASSERT_INDEX(i, ty->field_names.count);
      ASSERT_INDEX(ty->field_names.data[i], ty->owner->names.count);
      String* f_name = ty->owner->names.data[ty->field_names.data[i]];
      if (IS_STR_EQ(f_name, attrib)) {
        *value = inst->ins->fields.data[i];
        return true;
      }
    }

    // Couldn't find the attribute in it's type class, return false.
    return false;
  }

  UNREACHABLE();
}

bool instSetAttrib(PKVM* vm, Instance* inst, String* attrib, Var value) {

  if (inst->is_native) {

    if (vm->config.inst_set_attrib_fn) {
      // Temproarly change the fiber's "return address" to points to the
      // below var 'ret' so that the users can use 'pkGetArg...()' function
      // to validate and get the attribute.
      Var* temp = vm->fiber->ret;
      Var attrib_ptr = value;

      vm->fiber->ret = &attrib_ptr;
      PkStringPtr attr = { attrib->data, NULL, NULL,
                           attrib->length, attrib->hash };
      bool exists = vm->config.inst_set_attrib_fn(vm, inst->native,
                                                  inst->native_id, attr);
      vm->fiber->ret = temp;

      // If the type is incompatible there'll be an error by now, return false
      // and the user of this function has to check VM_HAS_ERROR() as well.
      if (VM_HAS_ERROR(vm)) return false;

      // If the attribute exists on the native type, the host application would
      // returned true by now, return it.
      return exists;
    }

    // If the host application doesn't provided a setter we treat it as it
    // doesn't has the attribute.
    return false;

  } else {

    // TODO: Optimize this with binary search.
    Class* ty = inst->ins->type;
    for (uint32_t i = 0; i < ty->field_names.count; i++) {
      ASSERT_INDEX(i, ty->field_names.count);
      ASSERT_INDEX(ty->field_names.data[i], ty->owner->names.count);
      String* f_name = ty->owner->names.data[ty->field_names.data[i]];
      if (f_name->hash == attrib->hash &&
        f_name->length == attrib->length &&
        memcmp(f_name->data, attrib->data, attrib->length) == 0) {
        inst->ins->fields.data[i] = value;
        return true;
      }
    }

    // Couldn't find the attribute in it's type class, return false.
    return false;
  }

  UNREACHABLE();
}

/*****************************************************************************/
/* UTILITY FUNCTIONS                                                         */
/*****************************************************************************/

const char* getPkVarTypeName(PkVarType type) {
  switch (type) {
    case PK_NULL:     return "Null";
    case PK_BOOL:     return "Bool";
    case PK_NUMBER:   return "Number";
    case PK_STRING:   return "String";
    case PK_LIST:     return "List";
    case PK_MAP:      return "Map";
    case PK_RANGE:    return "Range";
    case PK_SCRIPT:   return "Script";
    case PK_FUNCTION: return "Function";
    case PK_FIBER:    return "Fiber";
    case PK_CLASS:    return "Class";
    case PK_INST:     return "Inst";
  }

  UNREACHABLE();
}

const char* getObjectTypeName(ObjectType type) {
  switch (type) {
    case OBJ_STRING:  return "String";
    case OBJ_LIST:    return "List";
    case OBJ_MAP:     return "Map";
    case OBJ_RANGE:   return "Range";
    case OBJ_SCRIPT:  return "Script";
    case OBJ_FUNC:    return "Func";
    case OBJ_FIBER:   return "Fiber";
    case OBJ_CLASS:   return "Class";
    case OBJ_INST:    return "Inst";
  }
  UNREACHABLE();
}

const char* varTypeName(Var v) {
  if (IS_NULL(v)) return "Null";
  if (IS_BOOL(v)) return "Bool";
  if (IS_NUM(v))  return "Number";

  ASSERT(IS_OBJ(v), OOPS);
  Object* obj = AS_OBJ(v);
  return getObjectTypeName(obj->type);
}

bool isValuesSame(Var v1, Var v2) {
#if VAR_NAN_TAGGING
  // Bit representation of each values are unique so just compare the bits.
  return v1 == v2;
#else
  #error TODO:
#endif
}

bool isValuesEqual(Var v1, Var v2) {
  if (isValuesSame(v1, v2)) return true;

  // If we reach here only heap allocated objects could be compared.
  if (!IS_OBJ(v1) || !IS_OBJ(v2)) return false;

  Object* o1 = AS_OBJ(v1), *o2 = AS_OBJ(v2);
  if (o1->type != o2->type) return false;

  switch (o1->type) {
    case OBJ_RANGE:
      return ((Range*)o1)->from == ((Range*)o2)->from &&
             ((Range*)o1)->to   == ((Range*)o2)->to;

    case OBJ_STRING: {
      String* s1 = (String*)o1, *s2 = (String*)o2;
      return s1->hash == s2->hash &&
             s1->length == s2->length &&
             memcmp(s1->data, s2->data, s1->length) == 0;
    }

    case OBJ_LIST: {
      /*
      * l1 = []; list_append(l1, l1) # [[...]]
      * l2 = []; list_append(l2, l2) # [[...]]
      * l1 == l2 ## This will cause a stack overflow but not handling that
      *          ## (in python too).
      */
      List *l1 = (List*)o1, *l2 = (List*)o2;
      if (l1->elements.count != l2->elements.count) return false;
      Var* _v1 = l1->elements.data;
      Var* _v2 = l2->elements.data;
      for (uint32_t i = 0; i < l1->elements.count; i++) {
        if (!isValuesEqual(*_v1, *_v2)) return false;
        _v1++, _v2++;
      }
      return true;
    }

    default:
      return false;
  }
}

bool isObjectHashable(ObjectType type) {
  // Only list and map are un-hashable.
  return type != OBJ_LIST && type != OBJ_MAP;
}

// This will prevent recursive list/map from crash when calling to_string, by
// checking if the current sequence is in the outer sequence linked list.
struct OuterSequence {
  struct OuterSequence* outer;
  // If false it'll be map. If we have multiple sequence we should use an enum
  // here but we only ever support list and map as builtin sequence (so bool).
  bool is_list;
  union {
    const List* list;
    const Map* map;
  };
};
typedef struct OuterSequence OuterSequence;

static void _toStringInternal(PKVM* vm, const Var v, pkByteBuffer* buff,
                              OuterSequence* outer, bool repr) {
  ASSERT(outer == NULL || repr, OOPS);

  if (IS_NULL(v)) {
    pkByteBufferAddString(buff, vm, "null", 4);
    return;

  } else if (IS_BOOL(v)) {
    if (AS_BOOL(v)) pkByteBufferAddString(buff, vm, "true", 4);
    else pkByteBufferAddString(buff, vm, "false", 5);
    return;

  } else if (IS_NUM(v)) {
    double value = AS_NUM(v);

    if (isnan(value)) {
      pkByteBufferAddString(buff, vm, "nan", 3);

    } else if (isinf(value)) {
      if (value > 0.0) {
        pkByteBufferAddString(buff, vm, "+inf", 4);
      } else {
        pkByteBufferAddString(buff, vm, "-inf", 4);
      }

    } else {
      char num_buff[STR_DBL_BUFF_SIZE];
      int length = sprintf(num_buff, DOUBLE_FMT, AS_NUM(v));
      pkByteBufferAddString(buff, vm, num_buff, length);
    }

    return;

  } else if (IS_OBJ(v)) {

    const Object* obj = AS_OBJ(v);
    switch (obj->type) {

      case OBJ_STRING:
      {
        const String* str = (const String*)obj;
        if (outer == NULL && !repr) {
          pkByteBufferAddString(buff, vm, str->data, str->length);
          return;
        } else {
          // If recursive return with quotes (ex: [42, "hello", 0..10]).
          pkByteBufferWrite(buff, vm, '"');
          for (const char* c = str->data; *c != '\0'; c++) {
            switch (*c) {
              case '"': pkByteBufferAddString(buff, vm, "\\\"", 2); break;
              case '\\': pkByteBufferAddString(buff, vm, "\\\\", 2); break;
              case '\n': pkByteBufferAddString(buff, vm, "\\n", 2); break;
              case '\r': pkByteBufferAddString(buff, vm, "\\r", 2); break;
              case '\t': pkByteBufferAddString(buff, vm, "\\t", 2); break;

              default:
                pkByteBufferWrite(buff, vm, *c);
                break;
            }
          }
          pkByteBufferWrite(buff, vm, '"');
          return;
        }
        UNREACHABLE();
      }

      case OBJ_LIST:
      {
        const List* list = (const List*)obj;
        if (list->elements.count == 0) {
          pkByteBufferAddString(buff, vm, "[]", 2);
          return;
        }

        // Check if the list is recursive.
        OuterSequence* seq = outer;
        while (seq != NULL) {
          if (seq->is_list && seq->list == list) {
            pkByteBufferAddString(buff, vm, "[...]", 5);
            return;
          }
          seq = seq->outer;
        }
        OuterSequence seq_list;
        seq_list.outer = outer; seq_list.is_list = true; seq_list.list = list;

        pkByteBufferWrite(buff, vm, '[');
        for (uint32_t i = 0; i < list->elements.count; i++) {
          if (i != 0) pkByteBufferAddString(buff, vm, ", ", 2);
          _toStringInternal(vm, list->elements.data[i], buff, &seq_list, true);
        }
        pkByteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_MAP:
      {
        const Map* map = (const Map*)obj;
        if (map->entries == NULL) {
          pkByteBufferAddString(buff, vm, "{}", 2);
          return;
        }

        // Check if the map is recursive.
        OuterSequence* seq = outer;
        while (seq != NULL) {
          if (!seq->is_list && seq->map == map) {
            pkByteBufferAddString(buff, vm, "{...}", 5);
            return;
          }
          seq = seq->outer;
        }
        OuterSequence seq_map;
        seq_map.outer = outer; seq_map.is_list = false; seq_map.map = map;

        pkByteBufferWrite(buff, vm, '{');
        uint32_t i = 0;     // Index of the current entry to iterate.
        bool _first = true; // For first element no ',' required.
        do {

          // Get the next valid key index.
          bool _done = false;
          while (IS_UNDEF(map->entries[i].key)) {
            if (++i >= map->capacity) {
              _done = true;
              break;
            }
          }
          if (_done) break;

          if (!_first) pkByteBufferAddString(buff, vm, ", ", 2);

          _toStringInternal(vm, map->entries[i].key, buff, &seq_map, true);
          pkByteBufferWrite(buff, vm, ':');
          _toStringInternal(vm, map->entries[i].value, buff, &seq_map, true);

          i++; _first = false;
        } while (i < map->capacity);

        pkByteBufferWrite(buff, vm, '}');
        return;
      }

      case OBJ_RANGE:
      {
        const Range* range = (const Range*)obj;

        char buff_from[STR_DBL_BUFF_SIZE];
        const int len_from = snprintf(buff_from, sizeof(buff_from),
                                      DOUBLE_FMT, range->from);
        char buff_to[STR_DBL_BUFF_SIZE];
        const int len_to = snprintf(buff_to, sizeof(buff_to),
                                    DOUBLE_FMT, range->to);

        pkByteBufferAddString(buff, vm, "[Range:", 7);
        pkByteBufferAddString(buff, vm, buff_from, len_from);
        pkByteBufferAddString(buff, vm, "...", 3);
        pkByteBufferAddString(buff, vm, buff_to, len_to);
        pkByteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_SCRIPT: {
        const Script* scr = (const Script*)obj;
        pkByteBufferAddString(buff, vm, "[Module:", 8);
        if (scr->module != NULL) {
          pkByteBufferAddString(buff, vm, scr->module->data,
                              scr->module->length);
        } else {
          pkByteBufferWrite(buff, vm, '"');
          pkByteBufferAddString(buff, vm, scr->path->data, scr->path->length);
          pkByteBufferWrite(buff, vm, '"');
        }
        pkByteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_FUNC: {
        const Function* fn = (const Function*)obj;
        pkByteBufferAddString(buff, vm, "[Func:", 6);
        pkByteBufferAddString(buff, vm, fn->name, (uint32_t)strlen(fn->name));
        pkByteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_FIBER: {
        const Fiber* fb = (const Fiber*)obj;
        pkByteBufferAddString(buff, vm, "[Fiber:", 7);
        pkByteBufferAddString(buff, vm, fb->func->name,
                            (uint32_t)strlen(fb->func->name));
        pkByteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_CLASS: {
        const Class* ty = (const Class*)obj;
        pkByteBufferAddString(buff, vm, "[Class:", 7);
        String* ty_name = ty->owner->names.data[ty->name];
        pkByteBufferAddString(buff, vm, ty_name->data, ty_name->length);
        pkByteBufferWrite(buff, vm, ']');
        return;
      }

      case OBJ_INST:
      {
        const Instance* inst = (const Instance*)obj;
        pkByteBufferWrite(buff, vm, '[');
        pkByteBufferAddString(buff, vm, inst->name,
                              (uint32_t)strlen(inst->name));
        pkByteBufferWrite(buff, vm, ':');

        if (!inst->is_native) {
          const Class* ty = inst->ins->type;
          const Inst* ins = inst->ins;
          ASSERT(ins->fields.count == ty->field_names.count, OOPS);

          for (uint32_t i = 0; i < ty->field_names.count; i++) {
            if (i != 0) pkByteBufferWrite(buff, vm, ',');

            pkByteBufferWrite(buff, vm, ' ');
            String* f_name = ty->owner->names.data[ty->field_names.data[i]];
            pkByteBufferAddString(buff, vm, f_name->data, f_name->length);
            pkByteBufferWrite(buff, vm, '=');
            _toStringInternal(vm, ins->fields.data[i], buff, outer, repr);
          }
        } else {

          char buff_addr[STR_HEX_BUFF_SIZE];
          char* ptr = (char*)buff_addr;
          (*ptr++) = '0'; (*ptr++) = 'x';
          const int len = snprintf(ptr, sizeof(buff_addr) - 2,
                                "%08x", (unsigned int)(uintptr_t)inst->native);
          pkByteBufferAddString(buff, vm, buff_addr, (uint32_t)len);
        }

        pkByteBufferWrite(buff, vm, ']');
        return;
      }
    }

  }
  UNREACHABLE();
  return;
}

String* toString(PKVM* vm, const Var value) {

  // If it's already a string don't allocate a new string.
  if (IS_OBJ_TYPE(value, OBJ_STRING)) {
    return (String*)AS_OBJ(value);
  }

  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  _toStringInternal(vm, value, &buff, NULL, false);
  String* ret = newStringLength(vm, (const char*)buff.data, buff.count);
  pkByteBufferClear(&buff, vm);
  return ret;
}

String* toRepr(PKVM* vm, const Var value) {
  pkByteBuffer buff;
  pkByteBufferInit(&buff);
  _toStringInternal(vm, value, &buff, NULL, true);
  String* ret = newStringLength(vm, (const char*)buff.data, buff.count);
  pkByteBufferClear(&buff, vm);
  return ret;
}

bool toBool(Var v) {

  if (IS_BOOL(v)) return AS_BOOL(v);
  if (IS_NULL(v)) return false;
  if (IS_NUM(v)) return AS_NUM(v) != 0;

  ASSERT(IS_OBJ(v), OOPS);
  Object* o = AS_OBJ(v);
  switch (o->type) {
    case OBJ_STRING: return ((String*)o)->length != 0;
    case OBJ_LIST:   return ((List*)o)->elements.count != 0;
    case OBJ_MAP:    return ((Map*)o)->count != 0;
    case OBJ_RANGE: // [[FALLTHROUGH]]
    case OBJ_SCRIPT:
    case OBJ_FUNC:
    case OBJ_FIBER:
    case OBJ_CLASS:
    case OBJ_INST:
      return true;
  }

  UNREACHABLE();
}
