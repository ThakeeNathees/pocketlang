/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <math.h>

#ifndef PK_AMALGAMATED
#include "libs.h"
#include "../core/value.h"
#include "../core/vm.h"
#endif

DEF(_typesHashable,
  "types.hashable(value:Var) -> Bool",
  "Returns true if the [value] is hashable.") {

  // Get argument 1 directly.
  ASSERT(vm->fiber != NULL, OOPS);
  ASSERT(1 < pkGetSlotsCount(vm), OOPS);
  Var value = vm->fiber->ret[1];

  if (!IS_OBJ(value)) pkSetSlotBool(vm, 0, true);
  else pkSetSlotBool(vm, 0, isObjectHashable(AS_OBJ(value)->type));
}

DEF(_typesHash,
  "types.hash(value:Var) -> Number",
  "Returns the hash of the [value]") {

  // Get argument 1 directly.
  ASSERT(vm->fiber != NULL, OOPS);
  ASSERT(1 < pkGetSlotsCount(vm), OOPS);
  Var value = vm->fiber->ret[1];

  if (IS_OBJ(value) && !isObjectHashable(AS_OBJ(value)->type)) {
    pkSetRuntimeErrorFmt(vm, "Type '%s' is not hashable.", varTypeName(value));
    return;
  }

  pkSetSlotNumber(vm, 0, varHashValue(value));
}

/*****************************************************************************/
/* BYTE BUFFER                                                               */
/*****************************************************************************/

static void* _bytebuffNew(PKVM* vm) {
  pkByteBuffer* self = pkRealloc(vm, NULL, sizeof(pkByteBuffer));
  pkByteBufferInit(self);
  return self;
}

static void _bytebuffDelete(PKVM* vm, void* buff) {
  pkRealloc(vm, buff, 0);
}

DEF(_bytebuffReserve,
  "types.ByteBuffer.reserve(count:Number) -> Null",
  "Reserve [count] number of bytes internally. This is use full if the final "
  "size of the buffer is known beforehand to avoid reduce the number of "
  "re-allocations.") {
  double size;
  if (!pkValidateSlotNumber(vm, 1, &size)) return;

  pkByteBuffer* self = pkGetSelf(vm);
  pkByteBufferReserve(self, vm, (size_t) size);
}

// buff.fill(data, count)
DEF(_bytebuffFill,
  "types.ByteBuffer.fill(value:Number) -> Null",
  "Fill the buffer with the given byte value. Note that the value must be in "
  "between 0 and 0xff inclusive.") {
  uint32_t n;
  if (!pkValidateSlotInteger(vm, 1, &n)) return;
  if (n < 0x00 || n > 0xff) {
    pkSetRuntimeErrorFmt(vm, "Expected integer in range "
      "0x00 to 0xff, got %i.", n);
    return;
  }

  double count;
  if (!pkValidateSlotNumber(vm, 1, &count)) return;

  pkByteBuffer* self = pkGetSelf(vm);
  pkByteBufferFill(self, vm, (uint8_t) n, (int) count);
}

DEF(_bytebuffClear,
  "types.ByteBuffer.clear() -> Null",
  "Clear the buffer values.") {
  // TODO: Should I also zero or reduce the capacity?
  pkByteBuffer* self = pkGetSelf(vm);
  self->count = 0;
}

// Returns the length of bytes were written.
DEF(_bytebuffWrite,
  "types.ByteBuffer.write(data:Number|String) -> Null",
  "Writes the data to the buffer. If the [data] is a number that should be in "
  "between 0 and 0xff inclusively. If the [data] is a string all the bytes "
  "of the string will be written to the buffer.") {
  pkByteBuffer* self = pkGetSelf(vm);

  PkVarType type = pkGetSlotType(vm, 1);

  switch (type) {
    case PK_BOOL:
      pkByteBufferWrite(self, vm, pkGetSlotBool(vm, 1) ? 1 : 0);
      pkSetSlotNumber(vm, 0, 1);
      return;

    case PK_NUMBER: {
      uint32_t i;
      if (!pkValidateSlotInteger(vm, 1, &i)) return;
      if (i < 0x00 || i > 0xff) {
        pkSetRuntimeErrorFmt(vm, "Expected integer in range "
                                 "0x00 to 0xff, got %i.", i);
        return;
      }

      pkByteBufferWrite(self, vm, (uint8_t) i);
      pkSetSlotNumber(vm, 0, 1);
      return;
    }

    case PK_STRING: {
      uint32_t length;
      const char* str = pkGetSlotString(vm, 1, &length);
      pkByteBufferAddString(self, vm, str, length);
      pkSetSlotNumber(vm, 0, (double) length);
      return;
    }

    // TODO:
    case PK_LIST: {
    }

    default:
      break;
  }

  //< Pocketlang internal function.
  const char* name = getPkVarTypeName(type);
  pkSetRuntimeErrorFmt(vm, "Object %s cannot be written to ByteBuffer.", name);

}

DEF(_bytebuffSubscriptGet,
  "types.ByteBuffer.[](index:Number)", "") {
  double index;
  if (!pkValidateSlotNumber(vm, 1, &index)) return;
  if (floor(index) != index) {
    pkSetRuntimeError(vm, "Expected an integer but got number.");
    return;
  }

  pkByteBuffer* self = pkGetSelf(vm);

  if (index < 0 || index >= self->count) {
    pkSetRuntimeError(vm, "Index out of bound");
    return;
  }

  pkSetSlotNumber(vm, 0, self->data[(uint32_t)index]);

}

DEF(_bytebuffSubscriptSet,
  "types.ByteBuffer.[]=(index:Number, value:Number)", "") {
  double index, value;
  if (!pkValidateSlotNumber(vm, 1, &index)) return;
  if (!pkValidateSlotNumber(vm, 2, &value)) return;

  if (floor(index) != index) {
    pkSetRuntimeError(vm, "Expected an integer but got float.");
    return;
  }
  if (floor(value) != value) {
    pkSetRuntimeError(vm, "Expected an integer but got float.");
    return;
  }

  pkByteBuffer* self = pkGetSelf(vm);

  if (index < 0 || index >= self->count) {
    pkSetRuntimeError(vm, "Index out of bound");
    return;
  }

  if (value < 0x00 || value > 0xff) {
    pkSetRuntimeError(vm, "Value should be in range 0x00 to 0xff.");
    return;
  }

  self->data[(uint32_t) index] = (uint8_t) value;

}

DEF(_bytebuffString,
  "types.ByteBuffer.string() -> String",
  "Returns the buffered values as String.") {
  pkByteBuffer* self = pkGetSelf(vm);
  pkSetSlotStringLength(vm, 0, self->data, self->count);
}

DEF(_bytebuffCount,
  "types.ByteBuffer.count() -> Number",
  "Returns the number of bytes that have written to the buffer.") {
  pkByteBuffer* self = pkGetSelf(vm);
  pkSetSlotNumber(vm, 0, self->count);
}

/*****************************************************************************/
/* VECTOR                                                                    */
/*****************************************************************************/

typedef struct {
  double x, y, z;
} Vector;

static void* _vectorNew(PKVM* vm) {
  Vector* vec = pkRealloc(vm, NULL, sizeof(Vector));
  memset(vec, 0, sizeof(Vector));
  return vec;
}

static void _vectorDelete(PKVM* vm, void* vec) {
  pkRealloc(vm, vec, 0);
}

DEF(_vectorInit,
  "types.Vector._init()", "") {
  int argc = pkGetArgc(vm);
  if (!pkCheckArgcRange(vm, argc, 0, 3)) return;

  double x, y, z;
  Vector* vec = pkGetSelf(vm);

  if (argc == 0) return;
  if (argc >= 1) {
    if (!pkValidateSlotNumber(vm, 1, &x)) return;
    vec->x = x;
  }

  if (argc >= 2) {
    if (!pkValidateSlotNumber(vm, 2, &y)) return;
    vec->y = y;
  }

  if (argc == 3) {
    if (!pkValidateSlotNumber(vm, 3, &z)) return;
    vec->z = z;
  }

}

DEF(_vectorGetter,
  "types.Vector.@getter()", "") {
  const char* name; uint32_t length;
  if (!pkValidateSlotString(vm, 1, &name, &length)) return;

  Vector* vec = pkGetSelf(vm);
  if (length == 1) {
    if (*name == 'x') {
      pkSetSlotNumber(vm, 0, vec->x);
      return;
    } else if (*name == 'y') {
      pkSetSlotNumber(vm, 0, vec->y);
      return;
    } else if (*name == 'z') {
      pkSetSlotNumber(vm, 0, vec->z);
      return;
    }
  }
}

DEF(_vectorSetter,
  "types.Vector.@setter()", "") {
  const char* name; uint32_t length;
  if (!pkValidateSlotString(vm, 1, &name, &length)) return;

  Vector* vec = pkGetSelf(vm);

  if (length == 1) {
    if (*name == 'x') {
      double val;
      if (!pkValidateSlotNumber(vm, 2, &val)) return;
      vec->x = val; return;
    } else if (*name == 'y') {
      double val;
      if (!pkValidateSlotNumber(vm, 2, &val)) return;
      vec->y = val; return;
    } else if (*name == 'z') {
      double val;
      if (!pkValidateSlotNumber(vm, 2, &val)) return;
      vec->z = val; return;
    }
  }
}

DEF(_vectorRepr,
  "types.Vector._repr()", "") {
  Vector* vec = pkGetSelf(vm);
  pkSetSlotStringFmt(vm, 0, "[%g, %g, %g]", vec->x, vec->y, vec->z);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleTypes(PKVM* vm) {
  PkHandle* types = pkNewModule(vm, "types");

  REGISTER_FN(types, "hashable", _typesHashable, 1);
  REGISTER_FN(types, "hash", _typesHash, 1);

  PkHandle* cls_byte_buffer = pkNewClass(vm, "ByteBuffer", NULL, types,
                                         _bytebuffNew, _bytebuffDelete,
  "A simple dynamically allocated byte buffer type. This can be used for "
  "constructing larger strings without allocating and adding smaller "
  "intermeidate strings.");

  ADD_METHOD(cls_byte_buffer, "[]",      _bytebuffSubscriptGet, 1);
  ADD_METHOD(cls_byte_buffer, "[]=",     _bytebuffSubscriptSet, 2);
  ADD_METHOD(cls_byte_buffer, "reserve", _bytebuffReserve, 1);
  ADD_METHOD(cls_byte_buffer, "fill",    _bytebuffFill, 2);
  ADD_METHOD(cls_byte_buffer, "clear",   _bytebuffClear, 0);
  ADD_METHOD(cls_byte_buffer, "write",   _bytebuffWrite, 1);
  ADD_METHOD(cls_byte_buffer, "string",  _bytebuffString, 0);
  ADD_METHOD(cls_byte_buffer, "count",   _bytebuffCount, 0);

  pkReleaseHandle(vm, cls_byte_buffer);

  // TODO: add move mthods.
  PkHandle* cls_vector = pkNewClass(vm, "Vector", NULL, types,
                                    _vectorNew, _vectorDelete,
  "A simple vector type contains x, y, and z components.");

  ADD_METHOD(cls_vector, "_init", _vectorInit, -1);
  ADD_METHOD(cls_vector, "@getter", _vectorGetter, 1);
  ADD_METHOD(cls_vector, "@setter", _vectorSetter, 2);
  ADD_METHOD(cls_vector, "_repr", _vectorRepr, 0);

  pkReleaseHandle(vm, cls_vector);

  pkRegisterModule(vm, types);
  pkReleaseHandle(vm, types);
}
