/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "vm.h"

void* vmRealloc(VM* self, void* memory, size_t old_size, size_t new_size) {

	// Track the total allocated memory of the VM to trigger the GC.
	self->bytes_allocated += new_size - old_size;

	// TODO: If vm->bytes_allocated > some_value -> GC();

	if (new_size == 0) {
		free(memory);
		return NULL;
	}

	return realloc(memory, new_size);
}

void vmPushTempRef(VM* self, Object* obj) {
	ASSERT(obj != NULL, "Cannot reference to NULL.");
	if (self->temp_reference_count < MAX_TEMP_REFERENCE,
		"Too many temp references");
	self->temp_reference[self->temp_reference_count++] = obj;
}

void vmPopTempRef(VM* self) {
	ASSERT(self->temp_reference_count > 0, "Temporary reference is empty to pop.");
	self->temp_reference_count--;
}

