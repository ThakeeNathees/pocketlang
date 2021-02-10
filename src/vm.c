/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "vm.h"

#include "core.h"
#include "utils.h"

#define HAS_ERROR() (vm->error != NULL)

void* vmRealloc(MSVM* self, void* memory, size_t old_size, size_t new_size) {

	// Track the total allocated memory of the VM to trigger the GC.
	// if vmRealloc is called for freeing the old_size would be 0 since 
	// deallocated bytes are traced by garbage collector.
	self->bytes_allocated += new_size - old_size;

	// TODO: If vm->bytes_allocated > some_value -> GC();

	if (new_size == 0) {
		free(memory);
		return NULL;
	}

	return realloc(memory, new_size);
}

void vmInit(MSVM* self, MSConfiguration* config) {
	memset(self, 0, sizeof(MSVM));
	self->config = *config;
}

void vmPushTempRef(MSVM* self, Object* obj) {
	ASSERT(obj != NULL, "Cannot reference to NULL.");
	if (self->temp_reference_count < MAX_TEMP_REFERENCE,
		"Too many temp references");
	self->temp_reference[self->temp_reference_count++] = obj;
}

void vmPopTempRef(MSVM* self) {
	ASSERT(self->temp_reference_count > 0, "Temporary reference is empty to pop.");
	self->temp_reference_count--;
}

/******************************************************************************
 * RUNTIME                                                                    *
 *****************************************************************************/

// TODO: A function for quick debug. REMOVE.
void _printStackTop(MSVM* vm) {
	if (vm->sp != vm->stack) {
		Var v = *(vm->sp - 1);
		double n = AS_NUM(v);
		printf("%f\n", n);
	}
}

void msSetRuntimeError(MSVM* vm, const char* format, ...) {
	vm->error = newString(vm, "TODO:", 5);
	ASSERT(false, "TODO:");
}

void vmReportError(MSVM* vm) {
	ASSERT(HAS_ERROR(), "runtimeError() should be called after an error.");
	ASSERT(false, "TODO: create debug.h");
}

MSInterpretResult msInterpret(MSVM* vm, const char* file) {
	Script* script = compileSource(vm, file);
	// TODO: add script to vm's scripts array.
	return vmRunScript(vm, script);
}

MSInterpretResult vmRunScript(MSVM* vm, Script* _script) {

	uint8_t* ip;
	CallFrame* frame;

#define PUSH(value) (*vm->sp++ = (value))
#define POP()       (*(--vm->sp))
#define DROP()      (--vm->sp)
#define PEEK()      (vm->sp - 1)
#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip+=2, (uint16_t)((ip[-2] << 8) | ip[-1]))

// Check if any runtime error exists and if so returns RESULT_RUNTIME_ERROR.
#define CHECK_ERROR()                    \
	do {                                 \
		if (HAS_ERROR()) {               \
			msRuntimeError(vm);          \
			return RESULT_RUNTIME_ERROR; \
		}                                \
	} while (false)

#ifdef OPCODE
  #error "OPCODE" should not be deifined here.
#endif

#define DEBUG_INSTRUCTION()

#define SWITCH(code)                   \
	L_vm_main_loop:                    \
	DEBUG_INSTRUCTION();               \
	switch (code = (Opcode)READ_BYTE())
#define OPCODE(code) case OP_##code
#define DISPATCH()   goto L_vm_main_loop

	Script* script = _script; //< Currently executing script.
	ip = script->body->fn->opcodes.data;

	int stack_size = utilPowerOf2Ceil(script->body->fn->stack_size + 1);
	vm->stack_size = stack_size;
	vm->stack = ALLOCATE_ARRAY(vm, Var, vm->stack_size);
	vm->sp = vm->stack;

	vm->frames_size = 4; // TODO: Magic number.
	vm->frames = ALLOCATE_ARRAY(vm, CallFrame, vm->frames_size);
	frame = vm->frames;

	Opcode instruction;
	SWITCH(instruction) {

		OPCODE(CONSTANT):
			PUSH(script->literals.data[READ_SHORT()]);
			DISPATCH();

		OPCODE(PUSH_NULL):
			PUSH(VAR_NULL);
			DISPATCH();

		OPCODE(PUSH_TRUE):
			PUSH(VAR_TRUE);
			DISPATCH();

		OPCODE(PUSH_FALSE):
			PUSH(VAR_FALSE);
			DISPATCH();

		OPCODE(PUSH_LOCAL_0):
		OPCODE(PUSH_LOCAL_1):
		OPCODE(PUSH_LOCAL_2):
		OPCODE(PUSH_LOCAL_3):
		OPCODE(PUSH_LOCAL_4):
		OPCODE(PUSH_LOCAL_5):
		OPCODE(PUSH_LOCAL_6):
		OPCODE(PUSH_LOCAL_7):
		OPCODE(PUSH_LOCAL_8):
		OPCODE(PUSH_LOCAL_N):
			ASSERT(false, "TODO:");

		OPCODE(STORE_LOCAL_0):
		OPCODE(STORE_LOCAL_1):
		OPCODE(STORE_LOCAL_2):
		OPCODE(STORE_LOCAL_3):
		OPCODE(STORE_LOCAL_4):
		OPCODE(STORE_LOCAL_5):
		OPCODE(STORE_LOCAL_6):
		OPCODE(STORE_LOCAL_7):
		OPCODE(STORE_LOCAL_8):
		OPCODE(STORE_LOCAL_N):
			ASSERT(false, "TODO:");

		OPCODE(PUSH_GLOBAL):
		OPCODE(STORE_GLOBAL):
		OPCODE(PUSH_GLOBAL_EXT):
		OPCODE(STORE_GLOBAL_EXT):
		OPCODE(PUSH_FN):
		OPCODE(PUSH_FN_EXT):

		OPCODE(POP):
			DROP();
			DISPATCH();

		OPCODE(CALL_0):
		OPCODE(CALL_1):
		OPCODE(CALL_2):
		OPCODE(CALL_3):
		OPCODE(CALL_4):
		OPCODE(CALL_5):
		OPCODE(CALL_6):
		OPCODE(CALL_7):
		OPCODE(CALL_8):
		OPCODE(CALL_N):
		OPCODE(CALL_EXT_0):
		OPCODE(CALL_EXT_1):
		OPCODE(CALL_EXT_2):
		OPCODE(CALL_EXT_3):
		OPCODE(CALL_EXT_4):
		OPCODE(CALL_EXT_5):
		OPCODE(CALL_EXT_6):
		OPCODE(CALL_EXT_7):
		OPCODE(CALL_EXT_8):
		OPCODE(CALL_EXT_N):
		OPCODE(JUMP):
		OPCODE(JUMP_NOT):
		OPCODE(JUMP_IF_NOT):
		OPCODE(RETURN):
		OPCODE(GET_ATTRIB):
		OPCODE(SET_ATTRIB):
		OPCODE(GET_SUBSCRIPT):
		OPCODE(SET_SUBSCRIPT):
		OPCODE(NEGATIVE):
		OPCODE(NOT):
		OPCODE(BIT_NOT):
			ASSERT(false, "TODO:");

		OPCODE(ADD):
			PUSH(varAdd(vm, POP(), POP()));
			DISPATCH();

		OPCODE(SUBTRACT):
			PUSH(varSubtract(vm, POP(), POP()));
			DISPATCH();

		OPCODE(MULTIPLY):
			PUSH(varMultiply(vm, POP(), POP()));
			DISPATCH();

		OPCODE(DIVIDE):
			PUSH(varDivide(vm, POP(), POP()));
			DISPATCH();

		OPCODE(MOD):
		OPCODE(BIT_AND):
		OPCODE(BIT_OR):
		OPCODE(BIT_XOR):
		OPCODE(BIT_LSHIFT):
		OPCODE(BIT_RSHIFT):
		OPCODE(AND):
		OPCODE(OR):
		OPCODE(EQEQ):
		OPCODE(NOTEQ):
		OPCODE(LT):
		OPCODE(LTEQ):
		OPCODE(GT):
		OPCODE(GTEQ):
		OPCODE(RANGE):
		OPCODE(IN):
		OPCODE(END):
			break;

	}

	return RESULT_SUCCESS;
}
