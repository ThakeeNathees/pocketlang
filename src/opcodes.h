/*
 *  Copyright (c) 2021 Thakee Nathees
 *  Licensed under: MIT License
 */

// Opcodes X macro (http://en.wikipedia.org/wiki/X_Macro) should be included
// in the source where it'll be used. Required to define the following...
//
// #define OPCODE(name, params, stack)
// #include "opcodes.h"
//
// first parameter is the opcode name, 2nd will be the size of the parameter
// in bytes 3 one is how many stack slots it'll take after executing the
// instruction.

// Load the constant at index [arg] from the script's literals.
// params: 2 byte (uint16_t) index value.
OPCODE(CONSTANT, 2, 1)

// Push null on the stack.
OPCODE(PUSH_NULL, 0, 1)

// Push self on the stack. If the runtime don't have self it'll push null.
OPCODE(PUSH_SELF, 0, 1)

// Push true on the stack.
OPCODE(PUSH_TRUE, 0, 1)

// Push false on the stack.
OPCODE(PUSH_FALSE, 0, 1)

// Push a new list to construct from literal.
// param: 2 bytes list size (defalt is 0).
OPCODE(PUSH_LIST, 2, 1)

// Pop the value on the stack the next stack top would be a list. Append the
// value to the list. Used in literal array construction.
OPCODE(LIST_APPEND, 0, -1)

// Push stack local on top of the stack. Locals at 0 to 8 marked explicitly
// since it's performance criticle.
// params: PUSH_LOCAL_N -> 2 bytes (uint16_t) count value.
OPCODE(PUSH_LOCAL_0, 0, 1)
OPCODE(PUSH_LOCAL_1, 0, 1)
OPCODE(PUSH_LOCAL_2, 0, 1)
OPCODE(PUSH_LOCAL_3, 0, 1)
OPCODE(PUSH_LOCAL_4, 0, 1)
OPCODE(PUSH_LOCAL_5, 0, 1)
OPCODE(PUSH_LOCAL_6, 0, 1)
OPCODE(PUSH_LOCAL_7, 0, 1)
OPCODE(PUSH_LOCAL_8, 0, 1)
OPCODE(PUSH_LOCAL_N, 2, 1)

// Store the stack top value to another stack local index and don't pop since
// it's the result of the assignment.
// params: STORE_LOCAL_N -> 2 bytes (uint16_t) count value.
OPCODE(STORE_LOCAL_0, 0, 0)
OPCODE(STORE_LOCAL_1, 0, 0)
OPCODE(STORE_LOCAL_2, 0, 0)
OPCODE(STORE_LOCAL_3, 0, 0)
OPCODE(STORE_LOCAL_4, 0, 0)
OPCODE(STORE_LOCAL_5, 0, 0)
OPCODE(STORE_LOCAL_6, 0, 0)
OPCODE(STORE_LOCAL_7, 0, 0)
OPCODE(STORE_LOCAL_8, 0, 0)
OPCODE(STORE_LOCAL_N, 2, 0)

// Push the script global value on the stack.
// params: 2 bytes (uint16_t) index.
OPCODE(PUSH_GLOBAL, 2, 1)

// Store the stack top value to a global value and don't pop since it's the
// result of the assignment.
// params: 2 bytes (uint16_t) index.
OPCODE(STORE_GLOBAL, 2, 0)

// Push the script's function on the stack. It could later be called. But a
// function can't be stored i.e. can't assign a function with something else.
// params: 2 bytes index.
OPCODE(PUSH_FN, 2, 1)

// Push a built in function.
// params: 2 bytes index of the script.
OPCODE(PUSH_BUILTIN_FN, 2, 1)

// Pop the stack top.
OPCODE(POP, 0, -1)

// Calls a function using stack's top N values as the arguments and once it
// done the stack top should be stored otherwise it'll be disregarded. The
// function should set the 0 th argment to return value. Locals at 0 to 8 
// marked explicitly since it's performance criticle.
// params: n bytes argc.

// TODO: may be later.
//OPCODE(CALL_0, 0,  0) //< Push null call null will be the return value.
//OPCODE(CALL_1, 0, -1) //< Push null and arg1. arg1 will be popped.
//OPCODE(CALL_2, 0, -2) //< And so on.
//OPCODE(CALL_3, 0, -3)
//OPCODE(CALL_4, 0, -4)
//OPCODE(CALL_5, 0, -5)
//OPCODE(CALL_6, 0, -6)
//OPCODE(CALL_7, 0, -7)
//OPCODE(CALL_8, 0, -8)
OPCODE(CALL, 2, -0) //< Will calculated at compile time.

// The stack top will be iteration value, next one is iterator (integer) and
// next would be the container. It'll update those values but not push or pop
// any values. We need to ensure that stack state at the point.
// param: 2 bytes jump offset if the iteration should stop.
OPCODE(ITER, 2, 0)

// The address offset to jump to. It'll add the offset to ip.
// param: 2 bytes jump address offset.
OPCODE(JUMP, 2, 0)

// The address offset to jump to. It'll SUBTRACT the offset to ip.
// param: 2 bytes jump address offset.
OPCODE(LOOP, 2, 0)

// Pop the stack top value and if it's true jump.
// param: 2 bytes jump address.
OPCODE(JUMP_IF, 2, -1)

// Pop the stack top value and if it's false jump.
// param: 2 bytes jump address.
OPCODE(JUMP_IF_NOT, 2, -1)

// Pop the stack top value and store it to the current stack frame's 0 index.
// Then it'll pop the current stack frame.
OPCODE(RETURN, 0, -1)

// Pop var get attribute push the value.
// param: 2 byte attrib name index.
OPCODE(GET_ATTRIB, 2, 0)

// Get attribute to perform assignment operation before store it, so don't
// pop the var.
// param: 2 byte attrib name index.
OPCODE(GET_ATTRIB_AOP, 2, 1)

// Pop var and value update the attribute push result.
// param: 2 byte attrib name index.
OPCODE(SET_ATTRIB, 2, -1)

// Pop var, key, get value and push the result.
OPCODE(GET_SUBSCRIPT, 0, -1)

// Get subscript to perform assignment operation before store it, so it won't 
// pop the var and the key.
OPCODE(GET_SUBSCRIPT_AOP, 0, 1)

// Pop var, key, value set and push value back.
OPCODE(SET_SUBSCRIPT, 0, -2)

// Pop unary operand and push value.
OPCODE(NEGATIVE, 0, 0) //< Negative number value.
OPCODE(NOT, 0, 0)      //< boolean not.
OPCODE(BIT_NOT, 0, 0)  //< bitwise not.

// Pop binary operands and push value.
OPCODE(ADD, 0, -1)
OPCODE(SUBTRACT, 0, -1)
OPCODE(MULTIPLY, 0, -1)
OPCODE(DIVIDE, 0, -1)
OPCODE(MOD, 0, -1)

OPCODE(BIT_AND, 0, -1)
OPCODE(BIT_OR, 0, -1)
OPCODE(BIT_XOR, 0, -1)
OPCODE(BIT_LSHIFT, 0, -1)
OPCODE(BIT_RSHIFT, 0, -1)

OPCODE(AND, 0, -1)
OPCODE(OR, 0, -1)
OPCODE(EQEQ, 0, -1)
OPCODE(NOTEQ, 0, -1)
OPCODE(LT, 0, -1)
OPCODE(LTEQ, 0, -1)
OPCODE(GT, 0, -1)
OPCODE(GTEQ, 0, -1)

OPCODE(RANGE, 0, -1) //< Pop 2 integer make range push.
OPCODE(IN, 0, -1)

// TODO: literal list, map

// A sudo instruction which will never be called. A function's last opcode
// used for debugging.
OPCODE(END, 0, 0)
