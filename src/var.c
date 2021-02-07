/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "var.h"
#include "vm.h"

void varInitObject(Object* self, VM* vm, ObjectType type) {
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

static inline double varToDouble(Var value) {
#if VAR_NAN_TAGGING
	_DoubleBitsConv bits;
	bits.bits64 = value;
	return bits.num;
#else
	// TODO:
#endif // VAR_NAN_TAGGING
}

String* newString(VM* vm, const char* text, uint32_t length) {

	ASSERT(length == 0 || text != NULL, "Unexpected NULL string.");

	String* string = ALLOCATE_DYNAMIC(vm, String, length + 1, char);
	varInitObject(&string->_super, vm, OBJ_STRING);
	string->length = length;

	if (length != 0) memcpy(string->data, text, length);
	string->data[length] = '\0';
	return string;
}

Script* newScript(VM* vm) {
	Script* script = ALLOCATE(vm, Script);
	varInitObject(&script->_super, vm, OBJ_SCRIPT);

	varBufferInit(&script->globals);
	nameTableInit(&script->global_names);

	functionBufferInit(&script->functions);
	nameTableInit(&script->function_names);
	
	return script;
}

Function* newFunction(VM* vm, const char* name, Script* owner,
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
