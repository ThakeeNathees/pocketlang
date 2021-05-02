/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdio.h>

#include "var.h"
#include "vm.h"

// Public Api /////////////////////////////////////////////////////////////////
Var msVarBool(MSVM* vm, bool value) {
  return VAR_BOOL(value);
}

Var msVarNumber(MSVM* vm, double value) {
  return VAR_NUM(value);
}

Var msVarString(MSVM* vm, const char* value) {
  return VAR_OBJ(newString(vm, value, (uint32_t)strlen(value)));
}

bool msAsBool(MSVM* vm, Var value) {
  return AS_BOOL(value);
}

double msAsNumber(MSVM* vm, Var value) {
  return AS_NUM(value);
}

const char* msAsString(MSVM* vm, Var value) {
  return AS_STRING(value)->data;
}

///////////////////////////////////////////////////////////////////////////////

// Number of maximum digits for to_string buffer.
#define TO_STRING_BUFF_SIZE 128

void varInitObject(Object* self, MSVM* vm, ObjectType type) {
  self->type = type;
  self->is_marked = false;
  self->next = vm->first;
  vm->first = self;
}

void grayObject(Object* self, MSVM* vm) {
  if (self == NULL || self->is_marked) return;
  self->is_marked = true;

  // Add the object to the VM's gray_list so that we can recursively mark
  // it's referenced objects later.
  if (vm->gray_list_count >= vm->gray_list_capacity) {
    vm->gray_list_capacity *= 2;
    vm->gray_list = (Object**)vm->config.realloc_fn(
      vm->gray_list,
      vm->gray_list_capacity * sizeof(Object*),
      vm->config.user_data);
  }

  vm->gray_list[vm->gray_list_count++] = self;
}

void grayValue(Var self, MSVM* vm) {
  if (!IS_OBJ(self)) return;
  grayObject(AS_OBJ(self), vm);
}

void grayVarBuffer(VarBuffer* self, MSVM* vm) {
  for (int i = 0; i < self->count; i++) {
    grayValue(self->data[i], vm);
  }
}

void grayStringBuffer(StringBuffer* self, MSVM* vm) {
  for (int i = 0; i < self->count; i++) {
    grayObject((Object*)self->data[i], vm);
  }
}

void grayFunctionBuffer(FunctionBuffer* self, MSVM* vm) {
  for (int i = 0; i < self->count; i++) {
    grayObject((Object*)self->data[i], vm);
  }
}

void grayNameTable(NameTable* self, MSVM* vm) {
  for (int i = 0; i < self->count; i++) {
    grayObject((Object*)self->data[i], vm);
  }
}

static void blackenObject(Object* obj, MSVM* vm) {
  // TODO: trace here.

  switch (obj->type) {
    case OBJ_STRING: {
      vm->bytes_allocated += sizeof(String);
      vm->bytes_allocated += (size_t)(((String*)obj)->length + 1);
    } break;

    case OBJ_LIST: {
      List* list = (List*)obj;
      grayVarBuffer(&list->elements, vm);
      vm->bytes_allocated += sizeof(List);
      vm->bytes_allocated += sizeof(Var) * list->elements.capacity;
    } break;

    case OBJ_MAP: {
      TODO;
    } break;

    case OBJ_RANGE: {
      vm->bytes_allocated += sizeof(Range);
    } break;

    case OBJ_SCRIPT:
    {
      Script* script = (Script*)obj;
      vm->bytes_allocated += sizeof(Script);

      const int NT_ELEM_SIZE = sizeof(*script->global_names.data);

      grayVarBuffer(&script->globals, vm);
      vm->bytes_allocated += sizeof(Var) * script->globals.capacity;

      grayNameTable(&script->global_names, vm);
      vm->bytes_allocated += NT_ELEM_SIZE * script->global_names.capacity;

      grayVarBuffer(&script->literals, vm);
      vm->bytes_allocated += sizeof(Var) * script->literals.capacity;

      grayFunctionBuffer(&script->functions, vm);
      vm->bytes_allocated += sizeof(Function*) * script->functions.capacity;

      grayNameTable(&script->function_names, vm);
      vm->bytes_allocated += NT_ELEM_SIZE * script->function_names.capacity;

      grayStringBuffer(&script->names, vm);
      vm->bytes_allocated += sizeof(String*) * script->names.capacity;

      grayObject((Object*)script->body, vm);
    } break;

    case OBJ_FUNC:
    {
      Function* func = (Function*)obj;
      vm->bytes_allocated += sizeof(Function);

      grayObject((Object*)func->owner, vm);

      if (!func->is_native) {
        Fn* fn = func->fn;
        vm->bytes_allocated += sizeof(uint8_t)* fn->opcodes.capacity;
        vm->bytes_allocated += sizeof(int) * fn->oplines.capacity;
      }
    } break;

    case OBJ_FIBER:
    {
      Fiber* fiber = (Fiber*)obj;
      vm->bytes_allocated += sizeof(Fiber);

      grayObject((Object*)fiber->func, vm);

      // Blacken the stack.
      for (Var* local = fiber->stack; local < fiber->sp; local++) {
        grayValue(*local, vm);
      }
      vm->bytes_allocated += sizeof(Var) * fiber->stack_size;

      // Blacken call frames.
      for (int i = 0; i < fiber->frame_count; i++) {
        grayObject((Object*)fiber->frames[i].fn, vm);
        grayObject((Object*)fiber->frames[i].fn->owner, vm);
      }
      vm->bytes_allocated += sizeof(CallFrame) * fiber->frame_capacity;

      grayObject((Object*)fiber->error, vm);

    } break;

    case OBJ_USER:
      TODO;
      break;
  }
}


void blackenObjects(MSVM* vm) {
  while (vm->gray_list_count > 0) {
    // Pop the gray object from the list.
    Object* gray = vm->gray_list[--vm->gray_list_count];
    blackenObject(gray, vm);
  }
}

#if VAR_NAN_TAGGING
// A union to reinterpret a double as raw bits and back.
typedef union {
  uint64_t bits64;
  uint32_t bits32[2];
  double num;
} _DoubleBitsConv;
#endif

Var doubleToVar(double value) {
#if VAR_NAN_TAGGING
  _DoubleBitsConv bits;
  bits.num = value;
  return bits.bits64;
#else
  // TODO:
#endif // VAR_NAN_TAGGING
}

double varToDouble(Var value) {
#if VAR_NAN_TAGGING
  _DoubleBitsConv bits;
  bits.bits64 = value;
  return bits.num;
#else
  // TODO:
#endif // VAR_NAN_TAGGING
}

static String* allocateString(MSVM* vm, size_t length) {
  String* string = ALLOCATE_DYNAMIC(vm, String, length + 1, char);
  varInitObject(&string->_super, vm, OBJ_STRING);
  string->length = (uint32_t)length;
  string->data[length] = '\0';
  return string;
}

String* newString(MSVM* vm, const char* text, uint32_t length) {

  ASSERT(length == 0 || text != NULL, "Unexpected NULL string.");

  String* string = allocateString(vm, length);

  if (length != 0 && text != NULL) memcpy(string->data, text, length);
  return string;
}

List* newList(MSVM* vm, uint32_t size) {
  List* list = ALLOCATE(vm, List);
  varInitObject(&list->_super, vm, OBJ_LIST);
  varBufferInit(&list->elements);
  if (size > 0) {
    varBufferFill(&list->elements, vm, VAR_NULL, size);
    list->elements.count = 0;
  }
  return list;
}

Range* newRange(MSVM* vm, double from, double to) {
  Range* range = ALLOCATE(vm, Range);
  varInitObject(&range->_super, vm, OBJ_RANGE);
  range->from = from;
  range->to = to;
  return range;
}

Script* newScript(MSVM* vm) {
  Script* script = ALLOCATE(vm, Script);
  varInitObject(&script->_super, vm, OBJ_SCRIPT);

  script->name = NULL;
  // TODO: script->path = NULL;

  varBufferInit(&script->globals);
  nameTableInit(&script->global_names);
  varBufferInit(&script->literals);
  functionBufferInit(&script->functions);
  nameTableInit(&script->function_names);
  stringBufferInit(&script->names);

  vmPushTempRef(vm, &script->_super);
  const char* fn_name = "@(ScriptLevel)";
  script->body = newFunction(vm, fn_name, (int)strlen(fn_name), script, false);
  vmPopTempRef(vm);

  return script;
}

Function* newFunction(MSVM* vm, const char* name, int length, Script* owner,
                      bool is_native) {

  Function* func = ALLOCATE(vm, Function);
  varInitObject(&func->_super, vm, OBJ_FUNC);

  if (owner == NULL) {
    ASSERT(is_native, OOPS);
    func->name = name;
    func->owner = NULL;
    func->is_native = is_native;

  } else {
    // Add the name in the script's function buffer.
    String* name_ptr;
    vmPushTempRef(vm, &func->_super);
    functionBufferWrite(&owner->functions, vm, func);
    nameTableAdd(&owner->function_names, vm, name, length, &name_ptr);
    vmPopTempRef(vm);
  
    func->name = name_ptr->data;
    func->owner = owner;
    func->arity = -2; // -1 means variadic args.
    func->is_native = is_native;
  }


  if (is_native) {
    func->native = NULL;
  } else {
    Fn* fn = ALLOCATE(vm, Fn);

    byteBufferInit(&fn->opcodes);
    intBufferInit(&fn->oplines);
    fn->stack_size = 0;
    func->fn = fn;
  }
  return func;
}

Fiber* newFiber(MSVM* vm) {
  Fiber* fiber = ALLOCATE(vm, Fiber);
  memset(fiber, 0, sizeof(Fiber));
  varInitObject(&fiber->_super, vm, OBJ_FIBER);
  return fiber;
}

void freeObject(MSVM* vm, Object* obj) {
  // TODO: Debug trace memory here.

  // First clean the object's referencs, but we're not recursively doallocating
  // them because they're not marked and will be cleaned later.
  // Example: List's `elements` is VarBuffer that contain a heap allocated
  // array of `var*` which will be cleaned below but the actual `var` elements
  // will won't be freed here instead they havent marked at all, and will be
  // removed at the sweeping phase of the garbage collection.
  switch (obj->type) {
    case OBJ_STRING:
      break;

    case OBJ_LIST:
      varBufferClear(&(((List*)obj)->elements), vm);
      break;

    case OBJ_MAP:
      TODO;
      break;

    case OBJ_RANGE:
      break;

    case OBJ_SCRIPT: {
      Script* scr = (Script*)obj;
      varBufferClear(&scr->globals, vm);
      nameTableClear(&scr->global_names, vm);
      varBufferClear(&scr->literals, vm);
      functionBufferClear(&scr->functions, vm);
      nameTableClear(&scr->function_names, vm);
      stringBufferClear(&scr->names, vm);

    } break;

    case OBJ_FUNC:
    {
      Function* func = (Function*)obj;
      if (!func->is_native) {
        byteBufferClear(&func->fn->opcodes, vm);
        intBufferClear(&func->fn->oplines, vm);
      }
    } break;

    case OBJ_FIBER:
    {
      Fiber* fiber = (Fiber*)obj;
      DEALLOCATE(vm, fiber->stack);
      DEALLOCATE(vm, fiber->frames);
    } break;

    case OBJ_USER:
      break;
  }

  DEALLOCATE(vm, obj);
}

// Utility functions //////////////////////////////////////////////////////////

const char* varTypeName(Var v) {
  if (IS_NULL(v)) return "null";
  if (IS_BOOL(v)) return "bool";
  if (IS_NUM(v)) return "number";

  ASSERT(IS_OBJ(v), OOPS);
  Object* obj = AS_OBJ(v);
  switch (obj->type) {
    case OBJ_STRING:  return "String";
    case OBJ_LIST:    return "List";
    case OBJ_MAP:     return "Map";
    case OBJ_RANGE:   return "Range";
    case OBJ_SCRIPT:  return "Script";
    case OBJ_FUNC:    return "Func";
    case OBJ_USER:    return "UserObj";
    default:
      UNREACHABLE();
  }
}

bool isVauesSame(Var v1, Var v2) {
#if VAR_NAN_TAGGING
  // Bit representation of each values are unique so just compare the bits.
  return v1 == v2;
#else
#error TODO:
#endif
}

String* toString(MSVM* vm, Var v, bool recursive) {

  if (IS_NULL(v)) {
    return newString(vm, "null", 4);

  } else if (IS_BOOL(v)) {
    if (AS_BOOL(v)) {
      return newString(vm, "true", 4);
    } else {
      return newString(vm, "false", 5);
    }

  } else if (IS_NUM(v)) {
    char buff[TO_STRING_BUFF_SIZE];
    int length = sprintf(buff, "%.14g", AS_NUM(v));
    ASSERT(length < TO_STRING_BUFF_SIZE, "Buffer overflowed.");
    return newString(vm, buff, length);

  } else if (IS_OBJ(v)) {
    Object* obj = AS_OBJ(v);
    switch (obj->type) {
      case OBJ_STRING:
      {
        // If recursive return with quotes (ex: [42, "hello", 0..10])
        if (!recursive)
          return newString(vm, ((String*)obj)->data, ((String*)obj)->length);
        TODO; //< Add quotes around the string.
        break;
      }

      case OBJ_LIST:   return newString(vm, "[Array]",   7); // TODO;
      case OBJ_MAP:    return newString(vm, "[Map]",     5); // TODO;
      case OBJ_RANGE:  return newString(vm, "[Range]",   7); // TODO;
      case OBJ_SCRIPT: return newString(vm, "[Script]",  8); // TODO;
      case OBJ_FUNC: {
        const char* name = ((Function*)obj)->name;
        int length = (int)strlen(name); // TODO: Assert length.
        char buff[TO_STRING_BUFF_SIZE];
        memcpy(buff, "[Func:", 6);
        memcpy(buff + 6, name, length);
        buff[6 + length] = ']';
        return newString(vm, buff, 6 + length + 1);
      }
      case OBJ_USER:   return newString(vm, "[UserObj]", 9); // TODO;
        break;
    }

  }
  UNREACHABLE();
  return NULL;
}

bool toBool(Var v) {
  if (IS_BOOL(v)) return AS_BOOL(v);
  if (IS_NULL(v)) return false;
  if (IS_NUM(v)) {
    return AS_NUM(v) != 0;
  }

  ASSERT(IS_OBJ(v), OOPS);
  Object* o = AS_OBJ(v);
  switch (o->type) {
    case OBJ_STRING: return ((String*)o)->length != 0;

    case OBJ_LIST:   return ((List*)o)->elements.count != 0;
    case OBJ_MAP:    TODO;
    case OBJ_RANGE:  TODO; // Nout sure.
    case OBJ_SCRIPT: // [[FALLTHROUGH]]
    case OBJ_FUNC:
    case OBJ_USER:
      return true;
    default:
      UNREACHABLE();
  }

  return true;
}

Var stringFormat(MSVM* vm, const char* fmt, ...) {
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
        total_length += AS_STRING(va_arg(arg_list, Var))->length;
        break;

      default:
        total_length++;
    }
  }
  va_end(arg_list);

  // Now build the new string.
  String* result = allocateString(vm, total_length);
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
        String* string = AS_STRING(va_arg(arg_list, Var));
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

  return VAR_OBJ(result);
}