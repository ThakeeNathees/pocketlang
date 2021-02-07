/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

 /** A template header to emulate C++ template and every occurence of
  * $name$ will be replaced by the name of the buffer and $type$ will be
  * replaced by the element type of the buffer (by a pre compile script) */

// Replace the following line with "$name$_buffer.h"
#include "buffer.template.h"
#include "../utils.h"
#include "../vm.h"

void $name_l$BufferInit($name$Buffer* self) {
	self->data = NULL;
	self->count = 0;
	self->capacity = 0;
}

void $name_l$BufferClear($name$Buffer* self, VM* vm) {
	vmRealloc(vm, self->data, self->capacity * sizeof($type$), 0);
	self->data = NULL;
	self->count = 0;
	self->capacity = 0;
}

void $name_l$BufferFill($name$Buffer* self, VM* vm, $type$ data, int count) {
	
	if (self->capacity < self->count + count) {
		int capacity = utilPowerOf2Ceil((int)self->count + count);
		self->data = ($type$*)vmRealloc(vm, self->data,
			self->capacity * sizeof($type$), capacity * sizeof($type$));
		self->capacity = capacity;
	}

	for (int i = 0; i < count; i++) {
		self->data[self->count++] = data;
	}
}

void $name_l$BufferWrite($name$Buffer* self, VM* vm, $type$ data) {
	$name_l$BufferFill(self, vm, data, 1);
}
