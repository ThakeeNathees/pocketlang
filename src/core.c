/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#include "core.h"
#include "vm.h"

// Validators /////////////////////////////////////////////////////////////////

// Check if a numeric value bool/number and set [value].
bool isNumeric(Var var, double* value) {
	if (IS_BOOL(var)) {
		*value = AS_BOOL(var);
		return true;
	}
	if (IS_NUM(var)) {
		*value = AS_NUM(var);
		return true;
	}
	return false;
}

// Check if [var] is bool/number. if not set error and return false.
bool validateNumeric(MSVM* vm, Var var, double* value, const char* arg) {
	if (isNumeric(var, value)) return true;
	msSetRuntimeError(vm, "%s must be a numeric value.", arg);
	return false;
}

// Operators //////////////////////////////////////////////////////////////////

Var varAdd(MSVM* vm, Var v1, Var v2) {
	
	double d1, d2;
	if (isNumeric(v1, &d1)) {
		if (validateNumeric(vm, v2, &d2, "Right operand")) {
			return VAR_NUM(d1 + d2);
		}
		return VAR_NULL;
	}

	// TODO: string addition/ array addition etc.
	return VAR_NULL;
}

Var varSubtract(MSVM* vm, Var v1, Var v2) {
	return VAR_NULL;
}

Var varMultiply(MSVM* vm, Var v1, Var v2) {

	double d1, d2;
	if (isNumeric(v1, &d1)) {
		if (validateNumeric(vm, v2, &d2, "Right operand")) {
			return VAR_NUM(d1 * d2);
		}
		return VAR_NULL;
	}

	return VAR_NULL;
}

Var varDivide(MSVM* vm, Var v1, Var v2) {
	return VAR_NULL;
}