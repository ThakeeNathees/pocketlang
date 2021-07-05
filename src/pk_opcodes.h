/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

// Opcodes X macro (http://en.wikipedia.org/wiki/X_Macro) should be included
// in the source where it'll be used. Required to define the following...
//
// #define OPCODE(name, params, stack)
// #include "opcodes.h"
//
// Parameters:
//  (1) Opcode name.
//  (2) Size of parameter in bytes.
//  (3) How many stack slots it (parameter) will take after executing the
//      instruction.

// Load the constant at index [arg] from the script's literals.
// params: 2 byte (uint16_t) index value.
OPCODE(PUSH_CONSTANT, 2, 1)

// Push null on the stack.
OPCODE(PUSH_NULL, 0, 1)

// Push number 0 on the stack.
OPCODE(PUSH_0, 0, 1)

// Push true on the stack.
OPCODE(PUSH_TRUE, 0, 1)

// Push false on the stack.
OPCODE(PUSH_FALSE, 0, 1)

// Swap the top 2 stack values.
OPCODE(SWAP, 0, 0)

// Push a new list to construct from literal.
// param: 2 bytes list size (defalt is 0).
OPCODE(PUSH_LIST, 2, 1)

// Push a new map to construct from literal.
OPCODE(PUSH_MAP, 0, 1)

// Push a new instance to the stack.
// param: 1 byte index.
OPCODE(PUSH_INSTANCE, 1, 1)

// Pop the value on the stack the next stack top would be a list. Append the
// value to the list. Used in literal array construction.
OPCODE(LIST_APPEND, 0, -1)

// Pop the top 2 values from the stack, the next stack top would be the map.
// Insert the key value pairs to the map. Used in literal map construction.
OPCODE(MAP_INSERT, 0, -2)

// Pop the value on the stack, the next stack top would be an instance. Append
// the value to the instance. Used in instance construction.
OPCODE(INST_APPEND, 0, -1)

// Push stack local on top of the stack. Locals at 0 to 8 marked explicitly
// since it's performance critical.
// params: PUSH_LOCAL_N -> 1 byte count value.
OPCODE(PUSH_LOCAL_0, 0, 1)
OPCODE(PUSH_LOCAL_1, 0, 1)
OPCODE(PUSH_LOCAL_2, 0, 1)
OPCODE(PUSH_LOCAL_3, 0, 1)
OPCODE(PUSH_LOCAL_4, 0, 1)
OPCODE(PUSH_LOCAL_5, 0, 1)
OPCODE(PUSH_LOCAL_6, 0, 1)
OPCODE(PUSH_LOCAL_7, 0, 1)
OPCODE(PUSH_LOCAL_8, 0, 1)
OPCODE(PUSH_LOCAL_N, 1, 1)

// Store the stack top value to another stack local index and don't pop since
// it's the result of the assignment.
// params: STORE_LOCAL_N -> 1 byte count value.
OPCODE(STORE_LOCAL_0, 0, 0)
OPCODE(STORE_LOCAL_1, 0, 0)
OPCODE(STORE_LOCAL_2, 0, 0)
OPCODE(STORE_LOCAL_3, 0, 0)
OPCODE(STORE_LOCAL_4, 0, 0)
OPCODE(STORE_LOCAL_5, 0, 0)
OPCODE(STORE_LOCAL_6, 0, 0)
OPCODE(STORE_LOCAL_7, 0, 0)
OPCODE(STORE_LOCAL_8, 0, 0)
OPCODE(STORE_LOCAL_N, 1, 0)

// Push the script global value on the stack.
// params: 1 byte index.
OPCODE(PUSH_GLOBAL, 1, 1)

// Store the stack top value to a global value and don't pop since it's the
// result of the assignment.
// params: 1 byte index.
OPCODE(STORE_GLOBAL, 1, 0)

// Push the script's function on the stack. It could later be called. But a
// function can't be stored i.e. can't assign a function with something else.
// params: 1 byte index.
OPCODE(PUSH_FN, 1, 1)

// Push the script's type on the stack.
// params: 1 byte index
OPCODE(PUSH_TYPE, 1, 1)

// Push a built in function.
// params: 1 bytes index.
OPCODE(PUSH_BUILTIN_FN, 1, 1)

// Pop the stack top.
OPCODE(POP, 0, -1)

// Push the pre-compiled module at the index (from opcode) on the stack, and
// initialize the module (ie. run the main function) if it's not initialized
// already.
// params: 2 byte name index.
OPCODE(IMPORT, 2, 1)

// Calls a function using stack's top N values as the arguments and once it
// done the stack top should be stored otherwise it'll be disregarded. The
// function should set the 0 th argment to return value.
// params: 1 byte argc.
OPCODE(CALL, 1, -0) //< Stack size will calculated at compile time.

// Moves the [n] arguments and the function at the stack top to the current
// frame's base for the tail call (the caller's frame it taken by the callee)
// to perform the call, instead of creating a new call frame. This will
// optimize memory from O(n) to O(1) and prevent stackoverflow, and for larger
// recursive function it'll be much faster since, we're not allocating new
// stack slots and call frames.
// The return value will be placed at the same slot where it's caller will
// place once the function is called (rbp). So we're jumping from a deep nested
// calls to the initial caller directly once the function is done.
// params: 1 byte argc.
OPCODE(TAIL_CALL, 1, -0) //< Stack size will calculated at compile time.

// Starts the iteration and test the sequence if it's iterable, before the
// iteration instead of checking it everytime.
OPCODE(ITER_TEST, 0, 0)

// The stack top will be iteration value, next one is iterator (integer) and
// next would be the container. It'll update those values but not push or pop
// any values. We need to ensure that stack state at the point.
// param: 1 byte iterate type (will be set by ITER_TEST at runtime).
// param: 2 bytes jump offset if the iteration should stop.
OPCODE(ITER, 3, 0)

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

// It'll keep the instance on the stack and push the attribute on the stack.
// param: 2 byte attrib name index.
OPCODE(GET_ATTRIB_KEEP, 2, 1)

// Pop var and value update the attribute push result.
// param: 2 byte attrib name index.
OPCODE(SET_ATTRIB, 2, -1)

// Pop var, key, get value and push the result.
OPCODE(GET_SUBSCRIPT, 0, -1)

// Get subscript to perform assignment operation before store it, so it won't
// pop the var and the key. (ex: map[key] += value).
OPCODE(GET_SUBSCRIPT_KEEP, 0, 1)

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

OPCODE(EQEQ, 0, -1)
OPCODE(NOTEQ, 0, -1)
OPCODE(LT, 0, -1)
OPCODE(LTEQ, 0, -1)
OPCODE(GT, 0, -1)
OPCODE(GTEQ, 0, -1)

OPCODE(RANGE_IN, 0, -1) //< Pop 2 integer make range push.
OPCODE(RANGE_EX, 0, -1) //< Pop 2 integer make range push.
OPCODE(IN, 0, -1)

// Print the repr string of the value at the stack top, used in REPL mode.
// This will not pop the value.
OPCODE(REPL_PRINT, 0, 0)

// A sudo instruction which will never be called. A function's last opcode
// used for debugging.
OPCODE(END, 0, 0)
