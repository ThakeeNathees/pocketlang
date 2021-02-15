/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "name_table.h"
#include "../var.h"
#include "../vm.h"

void nameTableInit(NameTable* self) {
  stringBufferInit(self);
}

void nameTableClear(NameTable* self, MSVM* vm) {
  stringBufferClear(self, vm);
}

int nameTableAdd(NameTable* self, MSVM* vm, const char* name, size_t length,
                 String** ptr) {
  String* string = newString(vm, name, (uint32_t)length);

  vmPushTempRef(vm, &string->_super);
  stringBufferWrite(self, vm, string);
  vmPopTempRef(vm);

  int index = (int)(self->count - 1);
  if (ptr != NULL) *ptr = self->data[index];
  return index;
}

const char* nameTableGet(NameTable* self, int index) {
  ASSERT(0 <= index && index < self->count, "Index out of bounds.");
  return self->data[index]->data;
}

int nameTableFind(NameTable* self, const char* name, size_t length) {

  for (int i = 0; i < self->count; i++) {
    if (self->data[i]->length == length &&
      strncmp(self->data[i]->data, name, length) == 0) {
      return i;
    }
  }
  return -1;
}
