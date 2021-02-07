/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Licensed under: MIT License
 */

#ifndef VAR_H
#define VAR_H

/** @file
 * A simple single header dynamic type system library for small dynamic typed
 * languages using a technique called NaN-tagging (optional). The method is 
 * inspired from the wren (https://wren.io/) an awsome language written by the
 * author of "Crafting Interpreters" Bob Nystrom and it's contrbuters. 
 * Reference:
 *     https://github.com/wren-lang/wren/blob/main/src/vm/wren_value.h
 *     https://leonardschuetz.ch/blog/nan-boxing/
 * 
 * The previous implementation was to add a type field to every \ref var
 * and use smart pointers(C++17) to object with custom destructors,
 * which makes the programme in effect for small types such null, bool,
 * int and float.
 */

/** __STDC_LIMIT_MACROS and __STDC_CONSTANT_MACROS are a workaround to
 * allow C++ programs to use stdint.h macros specified in the C99
 * standard that aren't in the C++ standard */
#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include <stdbool.h>
#include <string.h>

#include "types/gen/byte_buffer.h"
#include "types/gen/function_buffer.h"
#include "types/gen/int_buffer.h"
#include "types/gen/var_buffer.h"
#include "types/name_table.h"

// To use dynamic variably-sized struct with a tail array add an array at the
// end of the struct with size \ref DYNAMIC_TAIL_ARRAY. This method was a
// legacy standard called "struct hack".
#if __STDC_VERSION__ >= 199901L
  /** for std >= c99  it's just `arr[]` */
  #define DYNAMIC_TAIL_ARRAY
#else
  #define DYNAMIC_TAIL_ARRAY 0
#endif

// Number of maximum import statements in a script.
#define MAX_IMPORT_SCRIPTS 16

/**
 * The IEEE 754 double precision float bit representation.
 *
 * 1 Sign bit
 * | 11 Exponent bits
 * | |          52 Mantissa (i.e. fraction values) bits
 * | |          |
 * S[Exponent-][Mantissa------------------------------------------]
 *
 * if all bits of the exponent are set it's a NaN ("Not a Number") value.
 *
 *  v~~~~~~~~~~ NaN value
 * -11111111111----------------------------------------------------
 * 
 * We define a our variant \ref var as an unsigned 64 bit integer (we treat it
 * like a bit array) if the exponent bits were not set, just reinterprit it as
 * a IEEE 754 double precision 64 bit number. Other wise we there are a lot of
 * different combination of bits we can use for our custom tagging, this method
 * is called NaN-Tagging.
 *
 * There are two kinds of NaN values "signalling" and "quiet". The first one is
 * intended to halt the execution but the second one is to continue the
 * execution quietly. We get the quiet NaN by setting the highest mentissa bit.
 *
 *             v~Highest mestissa bit
 * -[NaN      ]1---------------------------------------------------
 * 
 * if sign bit set, it's a heap allocated pointer.
 * |             these 2 bits are type tags representing 8 different types
 * |             vv
 * S[NaN      ]1cXX------------------------------------------------
 *              |  ^~~~~~~~ 48 bits to represent the value (51 for pointer)
 *              '- if this (const) bit set, it's a constant.
 * 
 * On a 32-bit machine a pointer size is 32 and on a 64-bit machine actually 48
 * bits are used for pointers. Ta-da, now we have double precision number,
 * primitives, pointers all inside a 64 bit sequence and for numbers it doesn't
 * require any bit mask operations, which means math on the var is now even
 * faster.
 *
 * our custom 2 bits type tagging
 * c00        : NULL
 * c01 ...  0 : UNDEF  (used in unused map keys)
 *     ...  1 : VOID   (void function return void not null)
 *     ... 10 : FALSE
 *     ... 11 : TRUE
 * c10        : INTEGER
 * |
 * '-- c is const bit.
 *
 */

#if VAR_NAN_TAGGING

// Masks and payloads.
#define _MASK_SIGN  ((uint64_t)0x8000000000000000)
#define _MASK_QNAN  ((uint64_t)0x7ffc000000000000)
#define _MASK_TYPE  ((uint64_t)0x0003000000000000)
#define _MASK_CONST ((uint64_t)0x0004000000000000)

#define _MASK_INTEGER (_MASK_QNAN | (uint64_t)0x0002000000000000)
#define _MASK_OBJECT  (_MASK_QNAN | (uint64_t)0x8000000000000000)

#define _PAYLOAD_INTEGER ((uint64_t)0x00000000ffffffff)
#define _PAYLOAD_OBJECT  ((uint64_t)0x0000ffffffffffff)

// Primitive types.
#define VAR_NULL      (_MASK_QNAN | (uint64_t)0x0000000000000000)
#define VAR_UNDEFINED (_MASK_QNAN | (uint64_t)0x0001000000000000)
#define VAR_VOID      (_MASK_QNAN | (uint64_t)0x0001000000000001)
#define VAR_FALSE     (_MASK_QNAN | (uint64_t)0x0001000000000002)
#define VAR_TRUE      (_MASK_QNAN | (uint64_t)0x0001000000000003)

// Encode types.
#define VAR_BOOL(value) ((value)? VAR_TRUE : VAR_FALSE)
#define VAR_INT(value)  (_MASK_INTEGER | (uint32_t)(int32_t)(value))
#define VAR_NUM(value)  (doubleToVar(value))
#define VAR_OBJ(value)  ((Var)(_MASK_OBJECT | (uint64_t)(uintptr_t)(value)))

// Const casting.
#define ADD_CONST(value)    ((value) | _MASK_CONST)
#define REMOVE_CONST(value) ((value) & ~_MASK_CONST)

// Check types.
#define IS_CONST(value) ((value & _MASK_CONST) == _MASK_CONST)
#define IS_NULL(value)  ((value) == VAR_NULL)
#define IS_UNDEF(value) ((value) == VAR_UNDEF)
#define IS_FALSE(value) ((value) == VAR_FALSE)
#define IS_TRUE(value)  ((value) == VAR_TRUE)
#define IS_BOOL(value)  (IS_TRUE(value) || IS_FALSE(value))
#define IS_INT(value)   ((value & _MASK_INTEGER) == _MASK_INTEGER)
#define IS_NUM(value)   ((value & _MASK_QNAN) != _MASK_QNAN)
#define IS_OBJ(value)   ((value & _MASK_OBJECT) == _MASK_OBJECT)

// Decode types.
#define AS_BOOL(value) ((value) == VAR_TRUE)
#define AS_INT(value)  ((int32_t)((value) & _PAYLOAD_INTEGER))
#define AS_NUM(value)  (varToDouble(value))
#define AS_OBJ(value)  ((Object*)(value & _PAYLOAD_OBJECT))

#define AS_STRING(value)  ((String*)AS_OBJ(value))
#define AS_CSTRING(value) (AS_STRING(value)->data)
#define AS_ARRAY(value)   ((Array*)AS_OBJ(value))
#define AS_MAP(value)     ((Map*)AS_OBJ(value))
#define AS_RANGE(value)   ((Range*)AS_OBJ(value))

typedef uint64_t Var;

#else

// TODO: Union tagging implementation of all the above macros ignore macros 
//       starts with an underscore.


typedef enum {
	VAR_UNDEFINED, //< Internal type for exceptions.
	VAR_NULL,      //< Null pointer type.
	VAR_BOOL,      //< Yin and yang of software.
	VAR_INT,       //< Only 32bit integers (to consistance with Nan-Tagging).
	VAR_FLOAT,     //< Floats are stored as (64bit) double.

	VAR_OBJECT,    //< Base type for all \ref var_Object types.
} VarType;

typedef struct {
	VarType type;
	union {
		bool _bool;
		int _int;
		double _float;
		Object* _obj;
	};
} var;

#endif // VAR_NAN_TAGGING

typedef enum /* ObjectType */ {
	OBJ_STRING,
	OBJ_ARRAY,
	OBJ_MAP,
	OBJ_RANGE,

	OBJ_SCRIPT,
	OBJ_FUNC,
	OBJ_INSTANCE,

	OBJ_USER,
} ObjectType;

// Base struct for all heap allocated objects.
struct Object {
	ObjectType type;  //< Type of the object in \ref var_Object_Type.
	//Class* is;      //< The class the object IS. // No OOP in MS.

	Object* next;     //< Next object in the heap allocated link list.
};

struct String {
	Object _super;

	uint32_t length;    //< Length of the string in \ref data.
	uint32_t capacity;  //< Size of allocated \ref data.
	char data[DYNAMIC_TAIL_ARRAY];
};

struct Array {
	Object _super;

	VarBuffer elements; //< Elements of the array.
};

// TODO: struct Map here.

struct Range {
	Object _super;

	double from; //< Beggining of the range inclusive.
	double to;   //< End of the range exclusive.
};

struct Script {
	Object _super;

	ID imports[MAX_IMPORT_SCRIPTS]; //< Imported script IDs.
	int import_count;               //< Number of import in imports.

	VarBuffer globals;         //< Script level global variables.
	NameTable global_names;    //< Name map to index in globals.

	FunctionBuffer functions;  //< Script level functions.
	NameTable function_names;  //< Name map to index in functions.

	// TODO: literal constants as Map.
};

// To maintain simpilicity I won't implement object oriantation in MiniScript.
//struct Class {
//	Object _super;
//
//	Class* _base_class;
//	String* name;
//};

// Script function pointer.
typedef struct {
	ByteBuffer opcodes;  //< Buffer of opcodes.
	IntBuffer oplines;   //< Line number of opcodes for debug (1 based).
	int stack_size;      //< Maximum size of stack required.
} Fn;

struct Function {
	Object _super;

	const char* name;    //< Name in the script [owner].
	Script* owner;       //< Owner script of the function.
	int arity;           //< Number of argument the function expects.

	bool is_native;      //< True if Native function.
	union {
		MiniScriptNativeFn native;  //< Native function pointer.
		Fn* fn;                     //< Script function pointer.
	};
};

// Methods.

void varInitObject(Object* self, VM* vm, ObjectType type);

// Instead use VAR_NUM(value) and AS_NUM(value)
Var doubleToVar(double value);
double varToDouble(Var value);

// Allocate new String object and return String*.
String* newString(VM* vm, const char* text, uint32_t length);

// Allocate new Script object and return Script*.
Script* newScript(VM* vm);

// Allocate new Function object and return Function*. Parameter [name] should
// be the name in the Script's nametable.
Function* newFunction(VM* vm, const char* name, Script* owner, bool is_native);


#endif // VAR_H
