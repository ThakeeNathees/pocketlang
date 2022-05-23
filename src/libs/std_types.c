/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <math.h>

#ifndef PK_AMALGAMATED
#include "libs.h"
#endif

/*****************************************************************************/
/* BYTE BUFFER                                                               */
/*****************************************************************************/

#ifndef PK_AMALGAMATED
#include "../core/value.h"
#endif

void* _bytebuffNew(PKVM* vm) {
  pkByteBuffer* self = pkRealloc(vm, NULL, sizeof(pkByteBuffer));
  pkByteBufferInit(self);
  return self;
}

void _bytebuffDelete(PKVM* vm, void* buff) {
  pkRealloc(vm, buff, 0);
}

void _bytebuffReserve(PKVM* vm) {
  double size;
  if (!pkValidateSlotNumber(vm, 1, &size)) return;

  pkByteBuffer* self = pkGetSelf(vm);
  pkByteBufferReserve(self, vm, (size_t) size);
}

// buff.fill(data, count)
void _bytebuffFill(PKVM* vm) {
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

void _bytebuffClear(PKVM* vm) {
  // TODO: Should I also zero or reduce the capacity?
  pkByteBuffer* self = pkGetSelf(vm);
  self->count = 0;
}

// Returns the length of bytes were written.
void _bytebuffWrite(PKVM* vm) {
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

void _bytebuffSubscriptGet(PKVM* vm) {
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

void _bytebuffSubscriptSet(PKVM* vm) {
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

void _bytebuffString(PKVM* vm) {
  pkByteBuffer* self = pkGetSelf(vm);
  pkSetSlotStringLength(vm, 0, self->data, self->count);
}

void _bytebuffCount(PKVM* vm) {
  pkByteBuffer* self = pkGetSelf(vm);
  pkSetSlotNumber(vm, 0, self->count);
}

/*****************************************************************************/
/* VECTOR                                                                    */
/*****************************************************************************/

typedef struct {
  double x, y, z;
} Vector;

void* _vectorNew(PKVM* vm) {
  Vector* vec = pkRealloc(vm, NULL, sizeof(Vector));
  memset(vec, 0, sizeof(Vector));
  return vec;
}

void _vectorDelete(PKVM* vm, void* vec) {
  pkRealloc(vm, vec, 0);
}

void _vectorInit(PKVM* vm) {
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

void _vectorGetter(PKVM* vm) {
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

void _vectorSetter(PKVM* vm) {
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

void _vectorRepr(PKVM* vm) {
  Vector* vec = pkGetSelf(vm);
  pkSetSlotStringFmt(vm, 0, "[%g, %g, %g]", vec->x, vec->y, vec->z);
}

/*****************************************************************************/
/* REGISTER TYPES                                                            */
/*****************************************************************************/

void registerModuleTypes(PKVM* vm) {
  PkHandle* types = pkNewModule(vm, "types");

  PkHandle* cls_byte_buffer = pkNewClass(vm, "ByteBuffer", NULL, types,
                                         _bytebuffNew, _bytebuffDelete);

  pkClassAddMethod(vm, cls_byte_buffer, "[]",      _bytebuffSubscriptGet, 1);
  pkClassAddMethod(vm, cls_byte_buffer, "[]=",     _bytebuffSubscriptSet, 2);
  pkClassAddMethod(vm, cls_byte_buffer, "reserve", _bytebuffReserve, 1);
  pkClassAddMethod(vm, cls_byte_buffer, "fill",    _bytebuffFill, 2);
  pkClassAddMethod(vm, cls_byte_buffer, "clear",   _bytebuffClear, 0);
  pkClassAddMethod(vm, cls_byte_buffer, "write",   _bytebuffWrite, 1);
  pkClassAddMethod(vm, cls_byte_buffer, "string",  _bytebuffString, 0);
  pkClassAddMethod(vm, cls_byte_buffer, "count",   _bytebuffCount, 0);
  pkReleaseHandle(vm, cls_byte_buffer);

  // TODO: add move mthods.
  PkHandle* cls_vector = pkNewClass(vm, "Vector", NULL, types,
                                    _vectorNew, _vectorDelete);
  pkClassAddMethod(vm, cls_vector, "_init", _vectorInit, -1);
  pkClassAddMethod(vm, cls_vector, "@getter", _vectorGetter, 1);
  pkClassAddMethod(vm, cls_vector, "@setter", _vectorSetter, 2);
  pkClassAddMethod(vm, cls_vector, "_repr", _vectorRepr, 0);
  pkReleaseHandle(vm, cls_vector);

  pkRegisterModule(vm, types);
  pkReleaseHandle(vm, types);
}
