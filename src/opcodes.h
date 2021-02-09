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
OPCODE(PUSH_NULL, 0, 0)

// Push true on the stack.
OPCODE(PUSH_TRUE, 0, 0)

// Push false on the stack.
OPCODE(PUSH_FALSE, 0, 0)

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

// Pop the stack top value and store to another stack local index.
// params: STORE_LOCAL_N -> 2 bytes (uint16_t) count value.
OPCODE(STORE_LOCAL_0, 0, -1)
OPCODE(STORE_LOCAL_1, 0, -1)
OPCODE(STORE_LOCAL_2, 0, -1)
OPCODE(STORE_LOCAL_3, 0, -1)
OPCODE(STORE_LOCAL_4, 0, -1)
OPCODE(STORE_LOCAL_5, 0, -1)
OPCODE(STORE_LOCAL_6, 0, -1)
OPCODE(STORE_LOCAL_7, 0, -1)
OPCODE(STORE_LOCAL_8, 0, -1)
OPCODE(STORE_LOCAL_N, 2, -1)

// Push the script global value on the stack.
// params: 2 bytes (uint16_t) index.
OPCODE(PUSH_GLOBAL, 2, 1)

// Pop and store the value to script's global.
// params: 2 bytes (uint16_t) index.
OPCODE(STORE_GLOBAL, 2, -1)

// Push imported script's global value on the stack.
// params: 4 bytes script ID and 2 bytes index.
OPCODE(PUSH_GLOBAL_EXT, 6, 1)

// Pop and store the value to imported script's global.
// params: 4 bytes script ID and 2 bytes index.
OPCODE(STORE_GLOBAL_EXT, 6, -1)

// Push the script's function on the stack. It could later be called. But a
// function can't be stored i.e. can't assign a function with something else.
// params: 2 bytes index.
OPCODE(PUSH_FN, 2, 1)

// Push an imported script's function.
// params: 4 bytes script ID and 2 bytes index.
OPCODE(PUSH_FN_EXT, 6, 1)

// Pop the stack top.
OPCODE(POP, 0, -1)

// Calls a function using stack's top N values as the arguments and once it
// done the stack top should be stored otherwise it'll be disregarded. The
// function should set the 0 th argment to return value. Locals at 0 to 8 
// marked explicitly since it's performance criticle.
// params: CALL_0..8 -> 2 bytes index. _N -> 2 bytes index and 2 bytes count.
OPCODE(CALL_0, 2,  1) //< Return value will be pushed.
OPCODE(CALL_1, 2,  0) //< 1st argument poped and return value pushed.
OPCODE(CALL_2, 2, -1) //< 2 args will be popped and return value pushed.
OPCODE(CALL_3, 2, -2)
OPCODE(CALL_4, 2, -3)
OPCODE(CALL_5, 2, -4)
OPCODE(CALL_6, 2, -5)
OPCODE(CALL_7, 2, -6)
OPCODE(CALL_8, 2, -7)
OPCODE(CALL_N, 4, -0) //< Will calculated at compile time.

// Call a function from an imported script.
// params: 4 bytes script ID and 2 bytes index. _N -> +2 bytes for argc.
OPCODE(CALL_EXT_0, 6,  1)
OPCODE(CALL_EXT_1, 6,  0)
OPCODE(CALL_EXT_2, 6, -1)
OPCODE(CALL_EXT_3, 6, -2)
OPCODE(CALL_EXT_4, 6, -3)
OPCODE(CALL_EXT_5, 6, -4)
OPCODE(CALL_EXT_6, 6, -5)
OPCODE(CALL_EXT_7, 6, -6)
OPCODE(CALL_EXT_8, 6, -7)
OPCODE(CALL_EXT_N, 8, -0) //< Will calculated at compile time.

// The address to jump to. It'll set the ip to the address it should jump to
// and the address is absolute not an offset from ip's current value.
// param: 2 bytes jump address.
OPCODE(JUMP, 2, 0)

// Pop the stack top value and if it's true jump.
// param: 2 bytes jump address.
OPCODE(JUMP_NOT, 2, -1)

// Pop the stack top value and if it's false jump.
// param: 2 bytes jump address.
OPCODE(JUMP_IF_NOT, 2, -1)

// Pop the stack top value and store it to the current stack frame's 0 index.
// Then it'll pop the current stack frame.
OPCODE(RETURN, 0, -1)

// Pop var get attribute push the value.
// param: 2 byte attrib name index.
OPCODE(GET_ATTRIB, 2, 0)

// Pop var and value update the attribute push result.
// param: 2 byte attrib name index.
OPCODE(SET_ATTRIB, 2, -1)

// Pop var, key, get value and push the result.
OPCODE(GET_SUBSCRIPT, 0, -1)

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
OPCODE(IS, 0, -1)
OPCODE(IN, 0, -1)

// TODO: literal list, map

// A sudo instruction which will never be called. A function's last opcode
// used for debugging.
OPCODE(END, 0, 0)
