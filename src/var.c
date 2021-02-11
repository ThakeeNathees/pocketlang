/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Licensed under: MIT License
 */

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
  func->arity = -1;

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

String* toString(MSVM* vm, Var v) {

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
        return newString(vm, ((String*)obj)->data, ((String*)obj)->length);
        break;

      case OBJ_ARRAY:    TODO;
      case OBJ_MAP:      TODO;
      case OBJ_RANGE:    TODO;
      case OBJ_SCRIPT:   TODO;
      case OBJ_FUNC:     TODO;
      case OBJ_INSTANCE: TODO;
      case OBJ_USER:     TODO;
        break;
    }

  }
  UNREACHABLE();
  return NULL;
}
