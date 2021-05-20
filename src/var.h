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

#include "common.h"
#include "buffers.h"

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
#define IS_UNDEF(value) ((value) == VAR_UNDEFINED)
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
#define AS_ARRAY(value)   ((List*)AS_OBJ(value))
#define AS_MAP(value)     ((Map*)AS_OBJ(value))
#define AS_RANGE(value)   ((Range*)AS_OBJ(value))

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
  OBJ_LIST,
  OBJ_MAP,
  OBJ_RANGE,

  OBJ_SCRIPT,
  OBJ_FUNC,

  OBJ_FIBER,

  // TODO: remove OBJ_USER and implement handlers for that.
  OBJ_USER,
} ObjectType;

// This will terminate compiler (because of 1/0 evaluvated) if ObjectType max
// is not [count]. Use this to ensure every time switching ObjectType will
// cover all object types.
#if DEBUG
  #define CHECK_MISSING_OBJ_TYPE(count) (1/ ((int)(!(count ^ OBJ_USER))) )
#else
  #define CHECK_MISSING_OBJ_TYPE(count) do {} while (false)
#endif

// Base struct for all heap allocated objects.
struct Object {
  ObjectType type;  //< Type of the object in \ref var_Object_Type.
  bool is_marked;   //< Marked when garbage collection's marking phase.
  //Class* is;      //< The class the object IS. // No OOP in PK (yet?).

  Object* next;     //< Next object in the heap allocated link list.
};

struct String {
  Object _super;

  uint32_t hash;      //< Hash value of the string.
  uint32_t length;    //< Length of the string in \ref data.
  uint32_t capacity;  //< Size of allocated \ref data.
  char data[DYNAMIC_TAIL_ARRAY];
};

struct List {
  Object _super;

  VarBuffer elements; //< Elements of the array.
};

typedef struct {
  // If the key is VAR_UNDEFINED it's an empty slot and if the value is false
  // the entry is new and available, if true it's a tumbstone - the entry
  // previously used but then deleted.

  Var key;   //< The entry's key or VAR_UNDEFINED of the entry is not in use.
  Var value; //< The entry's value.
} MapEntry;

struct Map {
  Object _super;

  uint32_t capacity; //< Allocated entry's count.
  uint32_t count;    //< Number of entries in the map.
  MapEntry* entries; //< Pointer to the contiguous array.
};

struct Range {
  Object _super;

  double from; //< Beggining of the range inclusive.
  double to;   //< End of the range exclusive.
};

struct Script {
  Object _super;

  // For core libraries the module and the path are same and points to the
  // same String objects.
  String* moudle; //< Module name of the script.
  String* path;   //< Path of the script.

  /*
  names:     ["v1", "fn1", "v2", "fn2", ...]
               0     1      2     3

  fn_names:  [      1,         3 ] <-- function name
                    0          1   <-- it's index

  functions: [      fn1,       fn2 ]
                    0          1
  */
  VarBuffer globals;         //< Script level global variables.
  UintBuffer global_names;   //< Name map to index in globals.
  VarBuffer literals;        //< Script literal constant values.
  FunctionBuffer functions;  //< Script level functions.
  UintBuffer function_names; //< Name map to index in functions.
  StringBuffer names;        //< Name literals, attribute names, etc.

  Function* body;            //< Script body is an anonymous function.
};

// Script function pointer.
typedef struct {
  ByteBuffer opcodes;  //< Buffer of opcodes.
  UintBuffer oplines;  //< Line number of opcodes for debug (1 based).
  int stack_size;      //< Maximum size of stack required.
} Fn;

struct Function {
  Object _super;

  const char* name;    //< Name in the script [owner] or C literal.
  Script* owner;       //< Owner script of the function.
  int arity;           //< Number of argument the function expects.

  bool is_native;      //< True if Native function.
  union {
    pkNativeFn native;  //< Native function pointer.
    Fn* fn;             //< Script function pointer.
  };
};

typedef struct {
  uint8_t* ip;  //< Pointer to the next instruction byte code.
  Function* fn; //< Function of the frame.
  Var* rbp;     //< Stack base pointer. (%rbp)
} CallFrame;

struct Fiber {
  Object _super;

  // The root function of the fiber. (For script it'll be the script's implicit
  // body function).
  Function* func;

  // The stack of the execution holding locals and temps. A heap will be
  // allocated and grow as needed.
  Var* stack;

  // The stack pointer (%rsp) pointing to the stack top.
  Var* sp;

  // The stack base pointer of the current frame. It'll be updated before
  // calling a native function. (`fiber->ret` === `curr_call_frame->rbp`). And
  // also updated if the stack is reallocated (that's when it's about to get
  // overflowed.
  Var* ret;

  // Size of the allocated stack.
  int stack_size;

  // Heap allocated array of call frames will grow as needed.
  CallFrame* frames;

  // Capacity of the frames array.
  int frame_capacity;

  // Number of frame entry in frames.
  int frame_count;

  // Caller of this fiber if it has one, NULL otherwise.
  Fiber* caller;

  // Runtime error initially NULL, heap allocated.
  String* error;
};

// Methods ////////////////////////////////////////////////////////////////////

// Initialize the object with it's default value.
void varInitObject(Object* self, PKVM* vm, ObjectType type);

// Mark the reachable objects at the mark-and-sweep phase of the garbage
// collection.
void grayObject(PKVM* vm, Object* self);

// Mark the reachable values at the mark-and-sweep phase of the garbage
// collection.
void grayValue(PKVM* vm, Var self);

// Mark the elements of the buffer as reachable at the mark-and-sweep pahse of
// the garbage collection.
void grayVarBuffer(PKVM* vm, VarBuffer* self);

// Mark the elements of the buffer as reachable at the mark-and-sweep pahse of
// the garbage collection.
void grayStringBuffer(PKVM* vm, StringBuffer* self);

// Mark the elements of the buffer as reachable at the mark-and-sweep pahse of
// the garbage collection.
void grayFunctionBuffer(PKVM* vm, FunctionBuffer* self);

// Pop objects from the gray list and add it's referenced objects to the
// working list to traverse and update the vm's [bytes_allocated] value.
void blackenObjects(PKVM* vm);

// Instead use VAR_NUM(value) and AS_NUM(value)
Var doubleToVar(double value);
double varToDouble(Var value);

// Allocate new String object and return String*.
String* newStringLength(PKVM* vm, const char* text, uint32_t length);
static inline String* newString(PKVM* vm, const char* text) {
  return newStringLength(vm, text, (uint32_t)strlen(text));
}

// Allocate new List and return List*.
List* newList(PKVM* vm, uint32_t size);

// Allocate new Map and return Map*.
Map* newMap(PKVM* vm);

// Allocate new Range object and return Range*.
Range* newRange(PKVM* vm, double from, double to);

// Allocate new Script object and return Script*.
Script* newScript(PKVM* vm, String* path);

// Allocate new Function object and return Function*. Parameter [name] should
// be the name in the Script's nametable. If the [owner] is NULL the function
// would be builtin function. For builtin function arity and the native
// function pointer would be initialized after calling this function.
Function* newFunction(PKVM* vm, const char* name, int length, Script* owner,
                      bool is_native);

// Allocate new Fiber object and return Fiber*.
Fiber* newFiber(PKVM* vm);

// Insert [value] to the list at [index] and shift down the rest of the
// elements.
void listInsert(PKVM* vm, List* self, uint32_t index, Var value);

// Remove and return element at [index].
Var listRemoveAt(PKVM* vm, List* self, uint32_t index);

// Returns the value for the [key] in the mape. If key not exists return
// VAR_UNDEFINED.
Var mapGet(Map* self, Var key);

// Add the [key], [value] entry to the map.
void mapSet(PKVM* vm, Map* self, Var key, Var value);

// Remove all the entries from the map.
void mapClear(PKVM* vm, Map* self);

// Remove the [key] from the map. If the key exists return it's value
// otherwise return VAR_NULL.
Var mapRemoveKey(PKVM* vm, Map* self, Var key);

// Release all the object owned by the [self] including itself.
void freeObject(PKVM* vm, Object* self);

// Utility functions //////////////////////////////////////////////////////////

// Returns the type name of the var [v].
const char* varTypeName(Var v);

// Returns true if both variables are the same (ie v1 is v2).
bool isValuesSame(Var v1, Var v2);

// Returns true if both variables are equal (ie v1 == v2).
bool isValuesEqual(Var v1, Var v2);

// Return the hash valur of the variable. (variable should be hashable).
uint32_t varHashValue(Var v);

// Return true if the object type is hashable.
bool isObjectHashable(ObjectType type);

// Returns the string version of the value. Note: pass false as [_recursive]
// It's for internal use (or may be I could make a wrapper around).
String* toString(PKVM* vm, Var v, bool _recursive);

// Returns the truthy value of the var.
bool toBool(Var v);

// Creates a new string from the arguments. This is intented to used internal
// usage and it has 2 formated characters (just like wren does).
// $ - a C string
// @ - a String object
String* stringFormat(PKVM* vm, const char* fmt, ...);

// Add the name (string literal) to the string buffer if not already exists and
// return it's index in the buffer.
uint32_t scriptAddName(Script* self, PKVM* vm, const char* name,
                       uint32_t length);

// Search for the function name in the script and return it's index in it's
// [functions] buffer. If not found returns -1.
int scriptSearchFunc(Script* script, const char* name, uint32_t length);

// Search for the global variable name in the script and return it's index in
// it's [globals] buffer. If not found returns -1.
int scriptSearchGlobals(Script* script, const char* name, uint32_t length);

#endif // VAR_H
