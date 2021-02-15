/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include <stdio.h>

#include "var.h"
#include "vm.h"

 // Number of maximum digits for to_string buffer.
#define TO_STRING_BUFF_SIZE 128

void varInitObject(Object* self, MSVM* vm, ObjectType type) {
  self->type = type;
  self->next = vm->first;
  vm->first = self;
  // TODO: set isGray = false;
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

String* newString(MSVM* vm, const char* text, uint32_t length) {

  ASSERT(length == 0 || text != NULL, "Unexpected NULL string.");

  String* string = ALLOCATE_DYNAMIC(vm, String, length + 1, char);
  varInitObject(&string->_super, vm, OBJ_STRING);
  string->length = length;

  if (length != 0) memcpy(string->data, text, length);
  string->data[length] = '\0';
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

  varBufferInit(&script->globals);
  nameTableInit(&script->global_names);

  varBufferInit(&script->literals);

  vmPushTempRef(vm, &script->_super);
  script->body = newFunction(vm, "@(ScriptLevel)", script, false);
  vmPopTempRef(vm);

  functionBufferInit(&script->functions);
  nameTableInit(&script->function_names);

  stringBufferInit(&script->names);

  return script;
}

Function* newFunction(MSVM* vm, const char* name, Script* owner,
                      bool is_native) {

  Function* func = ALLOCATE(vm, Function);
  varInitObject(&func->_super, vm, OBJ_FUNC);

  func->name = name;
  func->owner = owner;
  func->arity = -2; // -1 means variadic args.

  func->is_native = is_native;

  if (is_native) {
    func->native = NULL;
  } else {
    vmPushTempRef(vm, &func->_super);
    Fn* fn = ALLOCATE(vm, Fn);
    vmPopTempRef(vm);

    byteBufferInit(&fn->opcodes);
    intBufferInit(&fn->oplines);
    fn->stack_size = 0;
    func->fn = fn;
  }
  return func;
}

// Utility functions //////////////////////////////////////////////////////////

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
        memcpy(buff, "[func:", 6);
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
