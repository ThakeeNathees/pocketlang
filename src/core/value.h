/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_VALUE_H
#define PK_VALUE_H

#ifndef PK_AMALGAMATED
#include "buffers.h"
#include "internal.h"
#endif

/**
 * A simple dynamic type system library for small dynamic typed languages using
 * a technique called NaN-tagging (optional). The method is inspired from the
 * wren (https://wren.io/) an awesome language written by Bob Nystrom the
 * author of "Crafting Interpreters" and it's contrbuters.
 * Reference:
 *     https://github.com/wren-lang/wren/blob/main/src/vm/wren_value.h
 *     https://leonardschuetz.ch/blog/nan-boxing/
 *
 * The previous implementation was to add a type field to every var and use
 * smart pointers(C++17) to object with custom destructors, which makes the
 * programme inefficient for small types such null, bool, int and float.
 */

// There are 2 main implemenation of Var's internal representation. First one
// is NaN-tagging, and the second one is union-tagging. (read below for more).
#if VAR_NAN_TAGGING
  typedef uint64_t Var;
#else
  typedef struct Var Var;
#endif

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
 * like a bit array) if the exponent bits were not set, just reinterpret it as
 * a IEEE 754 double precision 64 bit number. Other wise we there are a lot of
 * different combination of bits we can use for our custom tagging, this method
 * is called NaN-Tagging.
 *
 * There are two kinds of NaN values "signalling" and "quiet". The first one is
 * intended to halt the execution but the second one is to continue the
 * execution quietly. We get the quiet NaN by setting the highest mantissa bit.
 *
 *             v~Highest mantissa bit
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
#define VAR_OBJ(value) /* [value] is an instance of Object */ \
  ((Var)(_MASK_OBJECT | (uint64_t)(uintptr_t)(&value->_super)))

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

// Evaluate to true if the var is an object and type of [obj_type].
#define IS_OBJ_TYPE(var, obj_type) \
  (IS_OBJ(var) && (AS_OBJ(var)->type == (obj_type)))

// Check if the 2 pocket strings are equal.
#define IS_STR_EQ(s1, s2)          \
 (((s1)->hash == (s2)->hash) &&    \
 ((s1)->length == (s2)->length) && \
 (memcmp((const void*)(s1)->data, (const void*)(s2)->data, (s1)->length) == 0))

// Compare pocket string with C string.
#define IS_CSTR_EQ(str, cstr, len)  \
 (((str)->length == len) &&         \
  (memcmp((const void*)(str)->data, (const void*)(cstr), len) == 0))

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
  VAR_INT,       //< Only 32bit integers (for consistence with Nan-Tagging).
  VAR_FLOAT,     //< Floats are stored as (64bit) double.

  VAR_OBJECT,    //< Base type for all \ref var_Object types.
} VarType;

struct Var {
  VarType type;
  union {
    bool _bool;
    int _int;
    double _float;
    Object* _obj;
  };
};

#endif // VAR_NAN_TAGGING

// Type definition of pocketlang heap allocated types.
typedef struct Object Object;
typedef struct String String;
typedef struct List List;
typedef struct Map Map;
typedef struct Range Range;
typedef struct Module Module;
typedef struct Function Function;
typedef struct Closure Closure;
typedef struct MethodBind MethodBind;
typedef struct Upvalue Upvalue;
typedef struct Fiber Fiber;
typedef struct Class Class;
typedef struct Instance Instance;

// Declaration of buffer objects of different types.
DECLARE_BUFFER(Uint, uint32_t)
DECLARE_BUFFER(Byte, uint8_t)
DECLARE_BUFFER(Var, Var)
DECLARE_BUFFER(String, String*)
DECLARE_BUFFER(Closure, Closure*)

// Add all the characters to the buffer, byte buffer can also be used as a
// buffer to write string (like a string stream). Note that this will not
// add a null byte '\0' at the end.
void pkByteBufferAddString(pkByteBuffer* self, PKVM* vm, const char* str,
                           uint32_t length);

// Add formated string to the byte buffer.
void pkByteBufferAddStringFmt(pkByteBuffer* self, PKVM* vm,
                              const char* fmt, ...);

// Type enums of the pocketlang heap allocated types.
typedef enum {
  OBJ_STRING = 0,
  OBJ_LIST,
  OBJ_MAP,
  OBJ_RANGE,
  OBJ_MODULE,
  OBJ_FUNC,
  OBJ_CLOSURE,
  OBJ_METHOD_BIND,
  OBJ_UPVALUE,
  OBJ_FIBER,
  OBJ_CLASS,
  OBJ_INST, // OBJ_INST should be the last element of this enums (don't move).
} ObjectType;

// Base struct for all heap allocated objects.
struct Object {
  ObjectType type;  //< Type of the object in \ref var_Object_Type.
  bool is_marked;   //< Marked when garbage collection's marking phase.
  Object* next;     //< Next object in the heap allocated link list.
};

struct String {
  Object _super;

  uint32_t hash;      //< 32 bit hash value of the string.
  uint32_t length;    //< Length of the string in \ref data.
  uint32_t capacity;  //< Size of allocated \ref data.
  char data[DYNAMIC_TAIL_ARRAY];
};

struct List {
  Object _super;

  pkVarBuffer elements; //< Elements of the array.
};

typedef struct {
  // If the key is VAR_UNDEFINED it's an empty slot and if the value is false
  // the entry is new and available, if true it's a tombstone - the entry
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

// Module in pocketlang is a collection of globals, functions, classes and top
// level statements, they can be imported in other modules generally a
// pocketlang script will compiled to a module.
struct Module {
  Object _super;

  // The [name] is the module name defined with either 'module' statement
  // in the script or the provided name for native modules when creating.
  // For core modules the name and the path are same and will points to the
  // same String objects. For modules compiled from a script the path will
  // be it's resolved path (could be absolute path but thats depend on the
  // path resolver).
  String* name;
  String* path;

  // The constant pool of the module, which contains literal values like
  // numbers, strings, and functions which are considered constants to
  // a moduel as well as classes.
  pkVarBuffer constants;

  // Globals is an array of global variables of the module. All the names
  // (including global variables) are stored in the constant pool of the
  // module. The (i)th global variable's names is located at index (j)
  // in the constant pool where j = global_names[i].
  pkVarBuffer globals;
  pkUintBuffer global_names;

  // Top level statements of a module are compiled to an implicit function
  // body which will be executed if it's imported for the first time.
  Closure* body;

  // If the [initialized] boolean is false, the body function of the module
  // will be executed when it's first imported and the 'initialized' boolean
  // will be set to true. If a module doesn't have globals, We can safely set
  // it to true to prevent from running the above body function, if it has one.
  bool initialized;

#ifndef PK_NO_DL
  // If the module loaded from a native extension, this handle will reference
  // to the platform dependent handle of that module and will be released
  // when the module garbage collected.
  void* handle;
#endif
};

// A struct contain opcodes and other information of a compiled function.
typedef struct {
  pkByteBuffer opcodes;  //< Buffer of opcodes.
  pkUintBuffer oplines;  //< Line number of opcodes for debug (1 based).
  int stack_size;        //< Maximum size of stack required.
} Fn;

struct Function {
  Object _super;

  // The module that owns this function. Since built in functions doesn't
  // belongs to a module it'll be NULL for them.
  Module* owner;

  // FIXME:
  // Because of the builtin function cannot have modules, we cannot reference
  // the name of a function with a index which points to the name entry in the
  // owner module's names buffer.
  //
  // The [name] is the name of the function which the function won't have a
  // reference to that (to prevent it from garbage collected), it's either
  // a C literal string or a name entry in the owner modules names buffer.
  // Either way it's guranteed to be alive till the function is alive.
  //
  // For embedding pocketlang the user must ensure the name exists till the
  // function is alive, and it's recomented to use literal C string for that.
  const char* name;

  // Number of argument the function expects. If the arity is -1 that means
  // the function has a variadic number of parameters. When a function is
  // constructed the value for arity is set to -2 to indicate that it hasn't
  // initialized.
  int arity;

  // A function can be either a function or a method, and if it's a method, it
  // cannot directly invoked without an instance.
  bool is_method;

  // Number of upvalues it uses, we're defining it here (and not in object Fn)
  // is prevent checking is_native everytime (which might be a bit faster).
  int upvalue_count;

  // Docstring of the function. Could be either a C string literal or a string
  // entry in it's owner module's constant pool.
  const char* docstring;

  // Function can be either native C function pointers or compiled pocket
  // functions.
  bool is_native;
  union {
    pkNativeFn native; //< Native function pointer.
    Fn* fn;            //< Pocket function pointer.
  };
};

// Closure are the first class citizen callables which wraps around a function
// [fn] which will be invoked each time the closure is called. In contrary to
// functions, closures have lexical scoping support via Upvalues. Consider the
// following function 'foo'
//
//     def foo()
//       bar = "bar"
//       return func()  ##< We'll be using the name 'baz' to identify this.
//         print(bar)
//       end
//     end
//
// The inner literal function 'baz' need variable named 'bar', It's only exists
// as long as the function 'foo' is active. Once the function 'foo' is returned
// all of it's local variables (including 'bar') will be popped and 'baz' will
// becom in-accessible.
//
// This is where closure and upvalues comes into picture. A closure will use
// upvalues to hold a reference of the variable ('bar') and when the variable
// ran out of it's scope / popped from stack, the upvalue will make it's own
// copy of that variable to make sure that a closure referenceing the variable
// via this upvalue has still access to the variable.
struct Closure {
  Object _super;

  Function* fn;
  Upvalue* upvalues[DYNAMIC_TAIL_ARRAY];
};

// Method bounds are first class callable of methods. That are bound to an
// instace which will be used as the self when the underlying method invoked.
// If the vallue [instance] is VAR_UNDEFINED it's unbound and cannot be
// called.
struct MethodBind {
  Object _super;

  Closure* method;
  Var instance;
};

// In addition to locals (which lives on the stack), a closure has upvalues.
// When a closure is created, an array of upvalues will be created for every
// closures, They works as a bridge between a closure and it's non-local
// variable. When the variable is still on the stack, upvalue has a state of
// 'open' and will points to the variable on the stack. When the variable is
// popped from the stack, the upvalue will be changed to the state 'closed'
// it'll make a copy of that variable inside of it and the pointer will points
// to the copyied (closed) variable.
//
//       |       |   .----------------v
//       |       |   | .-------------------.
//       |       |   '-| u1 closed | (qux) |
//       |       |     '-------------------'
//       |       | <- stack top
//       |  baz  |     .------------------.
//       |  bar  | <---| u2 open   | null |
//       |  foo  |     '------------------'
//       '-------'
//         stack
//
struct Upvalue {
  Object _super;

  // The pointer which points to the non-local variable, once the variable is
  // out of scope the [ptr] will points to the below value [closed].
  Var* ptr;

  // The copyied value of the non-local.
  Var closed;

  // To prevent multiple upvalues created for a single variable we keep track
  // of all the open upvalues on a linked list. Once we need an upvalue and if
  // it's already exists on the chain we re-use it, otherwise a new upvalue
  // instance will be created (here [next] is the next upvalue on the chain).
  Upvalue* next;
};

typedef struct {
  const uint8_t* ip;      //< Pointer to the next instruction byte code.
  const Closure* closure; //< Closure of the frame.
  Var* rbp;               //< Stack base pointer. (%rbp)
  Var self;               //< Self reference of the current method.
} CallFrame;

typedef enum {
  FIBER_NEW,     //< Fiber haven't started yet.
  FIBER_RUNNING, //< Fiber is currently running.
  FIBER_YIELDED, //< Yielded fiber, can be resumed.
  FIBER_DONE,    //< Fiber finished and cannot be resumed.
} FiberState;

struct Fiber {
  Object _super;

  FiberState state;

  // The root closure of the fiber.
  Closure* closure;

  // The stack of the execution holding locals and temps. A heap will be
  // allocated and grow as needed.
  Var* stack;
  int stack_size; //< Capacity of the allocated stack.

  // The stack pointer (%rsp) pointing to the stack top.
  Var* sp;

  // Heap allocated array of call frames will grow as needed.
  CallFrame* frames;
  int frame_capacity; //< Capacity of the frames array.
  int frame_count; //< Number of frame entry in frames.

  // All the open upvalues will form a linked list in the fiber and the
  // upvalues are sorted in the same order their locals in the stack. The
  // bellow pointer is the head of those upvalues near the stack top.
  Upvalue* open_upvalues;

  // The stack base pointer of the current frame. It'll be updated before
  // calling a native function. (`fiber->ret` === `curr_call_frame->rbp`).
  // Return value of the callee fiber will be passed and return value of the
  // the function that started the fiber will also be set.
  Var* ret;

  // The self pointer to of the current method. It'll be updated before
  // calling a native method. (Because native methods doesn't have a call
  // frame we're doing it this way). Also updated just before calling a
  // script method, and will be captured by the next allocated callframe
  // and reset to VAR_UNDEFINED.
  Var self;

  // [caller] is the caller of the fiber that was created by invoking the
  // pocket concurency model. Where [native] is the native fiber which
  // started this fiber. If a native function wants to call pocket function
  // it needs to create a new fiber, so we keep track of both.
  Fiber *caller, *native;

  // Runtime error initially NULL, heap allocated.
  String* error;
};

struct Class {
  Object _super;

  // The base class of this class.
  Class* super_class;

  // The module that owns this class.
  Module* owner;

  // Name of the class.
  String* name;

  // Docstring of the class. Could be either a C string literal or a string
  // entry in it's owner module's constant pool.
  const char* docstring;

  // For builtin type it'll be it's enum (ex: PK_STRING, PK_NUMBER, ...) for
  // every other classes it'll be PK_INSTANCE to indicate that it's not a
  // builtin type's class.
  PkVarType class_of;

  Closure* ctor; //< The constructor function.

  // A buffer of methods of the class.
  pkClosureBuffer methods;

  // Static attributes of the class.
  Map* static_attribs;

  // Allocater and de-allocator functions for native types.
  // For script/ builtin types it'll be NULL.
  pkNewInstanceFn new_fn;
  pkDeleteInstanceFn delete_fn;

};

typedef struct {
  Class* type;        //< Class this instance belongs to.
  pkVarBuffer fields; //< Var buffer of the instance.
} Inst;

struct Instance {
  Object _super;

  Class* cls; //< Class of the instance.

  // If the instance is native, the [native] pointer points to the user data
  // (generally a heap allocated struct of that type) that contains it's
  // attributes. We'll use it to access an attribute first with setters and
  // getters and if the attribute not exists we'll continue search in the
  // bellow attribs map.
  void* native;

  // Dynamic attributes of an instance.
  Map* attribs;
};

/*****************************************************************************/
/* "CONSTRUCTORS"                                                            */
/*****************************************************************************/

void varInitObject(Object* self, PKVM* vm, ObjectType type);

String* newStringLength(PKVM* vm, const char* text, uint32_t length);

String* newStringVaArgs(PKVM* vm, const char* fmt, va_list args);

// An inline function/macro implementation of newString(). Set below 0 to 1, to
// make the implementation a static inline function, it's totally okey to
// define a function inside a header as long as it's static (but not a fan).
#if 0 // Function implementation.
  // Allocate new string using the cstring [text].
  static inline String* newString(PKVM* vm, const char* text) {
    uint32_t length = (text == NULL) ? 0 : (uint32_t)strlen(text);
    return newStringLength(vm, text, length);
  }
#else // Macro implementation.
  // Allocate new string using the cstring [text].
  #define newString(vm, text) \
    (newStringLength(vm, text, (!(text)) ? 0 : (uint32_t)strlen(text)))
#endif

List* newList(PKVM* vm, uint32_t size);

Map* newMap(PKVM* vm);

Range* newRange(PKVM* vm, double from, double to);

Module* newModule(PKVM* vm);

Closure* newClosure(PKVM* vm, Function* fn);

MethodBind* newMethodBind(PKVM* vm, Closure* method);

Upvalue* newUpvalue(PKVM* vm, Var* value);

Fiber* newFiber(PKVM* vm, Closure* closure);

// FIXME:
// The docstring should be allocated and stored in the module's constants
// as a string if it's not a native function. (native function's docs are
// C string liteals).
//
// Allocate a new function and return it.
Function* newFunction(PKVM* vm, const char* name, int length,
                      Module* owner,
                      bool is_native, const char* docstring,
                      int* fn_index);

// If the module is not NULL, the name and the class object will be added to
// the module's constant pool. The class will be added to the modules global
// as well.
Class* newClass(PKVM* vm, const char* name, int length,
                Class* super, Module* module,
                const char* docstring, int* cls_index);

// Allocate new instance with of the base [type].
Instance* newInstance(PKVM* vm, Class* cls);

/*****************************************************************************/
/* METHODS                                                                   */
/*****************************************************************************/

// Mark the reachable objects at the mark-and-sweep phase of the garbage
// collection.
void markObject(PKVM* vm, Object* self);

// Mark the reachable values at the mark-and-sweep phase of the garbage
// collection.
void markValue(PKVM* vm, Var self);

// Mark the elements of the buffer as reachable at the mark-and-sweep phase of
// the garbage collection.
void markVarBuffer(PKVM* vm, pkVarBuffer* self);

// Pop the marked objects from the working set of the VM and add it's
// referenced objects to the working set, continue traversing and mark
// all the reachable objects.
void popMarkedObjects(PKVM* vm);

// Returns a number list from the range. starts with range.from and ends with
// (range.to - 1) increase by 1. Note that if the range is reversed
// (ie. range.from > range.to) It'll return an empty list ([]).
List* rangeAsList(PKVM* vm, Range* self);

// Returns a lower case version of the given string. If the string is
// already lower it'll return the same string.
String* stringLower(PKVM* vm, String* self);

// Returns a upper case version of the given string. If the string is
// already upper it'll return the same string.
String* stringUpper(PKVM* vm, String* self);

// Returns string with the leading and trailing white spaces are trimmed.
// If the string is already trimmed it'll return the same string.
String* stringStrip(PKVM* vm, String* self);

// Replace the [count] occurence of the [old] string with [new_] string. If
// [count] == -1. It'll replace all the occurences. If nothing is replaced
// the original string will be returned.
String* stringReplace(PKVM* vm, String* self,
                      String* old, String* new_, int count);

// Split the string into a list of string separated by [sep]. String [sep] must
// not be of length 0 otherwise an assertion will fail.
List* stringSplit(PKVM* vm, String* self, String* sep);

// Creates a new string from the arguments. This is intended for internal
// usage and it has 2 formated characters (just like wren does).
// $ - a C string
// @ - a String object
String* stringFormat(PKVM* vm, const char* fmt, ...);

// Create a new string by joining the 2 given string and return the result.
// Which would be faster than using "@@" format.
String* stringJoin(PKVM* vm, String* str1, String* str2);

// An inline function/macro implementation of listAppend(). Set below 0 to 1,
// to make the implementation a static inline function, it's totally okey to
// define a function inside a header as long as it's static (but not a fan).
#if 0 // Function implementation.
  static inline void listAppend(PKVM* vm, List* self, Var value) {
    pkVarBufferWrite(&self->elements, vm, value);
  }
#else // Macro implementation.
  #define listAppend(vm, self, value) \
    pkVarBufferWrite(&self->elements, vm, value)
#endif

// Insert [value] to the list at [index] and shift down the rest of the
// elements.
void listInsert(PKVM* vm, List* self, uint32_t index, Var value);

// Remove and return element at [index].
Var listRemoveAt(PKVM* vm, List* self, uint32_t index);

// Remove all the elements of the list.
void listClear(PKVM* vm, List* self);

// Create a new list by joining the 2 given list and return the result.
List* listAdd(PKVM* vm, List* l1, List* l2);

// Returns the value for the [key] in the map. If key not exists return
// VAR_UNDEFINED.
Var mapGet(Map* self, Var key);

// Add the [key], [value] entry to the map.
void mapSet(PKVM* vm, Map* self, Var key, Var value);

// Remove all the entries from the map.
void mapClear(PKVM* vm, Map* self);

// Remove the [key] from the map. If the key exists return it's value
// otherwise return VAR_UNDEFINED.
Var mapRemoveKey(PKVM* vm, Map* self, Var key);

// Returns true if the fiber has error, and if it has any the fiber cannot be
// resumed anymore.
bool fiberHasError(Fiber* fiber);

// Add a constant [value] to the [module] if it doesn't already present in the
// constant buffer and return it's index.
uint32_t moduleAddConstant(PKVM* vm, Module* module, Var value);

// Add a string literal to the module's constant buffer if not already exists
// and return it. If the [index] isn't NULL, the index of the string will be
// written on it.
String* moduleAddString(Module* module, PKVM* vm, const char* name,
                        uint32_t length, int* index);

// Returns a string at the index of the module, if the index is invalid or the
// constant at the index is not a string, it'll return NULL. (however if the
// index is negative i'll fail an assertion).
String* moduleGetStringAt(Module* module, int index);

// Set the global [value] to the [module] and return its index. If the global
// [name] already exists it'll update otherwise one will be created.
uint32_t moduleSetGlobal(PKVM* vm, Module* module,
                         const char* name, uint32_t length,
                         Var value);

// Search for the [name] in the module's globals and return it's index.
// If not found it'll return -1.
int moduleGetGlobalIndex(Module* module, const char* name, uint32_t length);

// This will allocate a new implicit main function for the module and assign to
// the module's body attribute. And the attribute initialized will be set to
// false. Note that the body of the module should be NULL before calling this
// function.
void moduleAddMain(PKVM* vm, Module* module);

// Release all the object owned by the [self] including itself.
void freeObject(PKVM* vm, Object* self);

/*****************************************************************************/
/* UTILITY FUNCTIONS                                                         */
/*****************************************************************************/

// Internal method behind VAR_NUM(value) don't use it directly.
Var doubleToVar(double value);

// Internal method behind AS_NUM(value) don't use it directly.
double varToDouble(Var value);

// Returns the PkVarType of the object type.
PkVarType getObjPkVarType(ObjectType type);

// Returns the ObjectType of the PkVar type.
ObjectType getPkVarObjType(PkVarType type);

// Returns the type name of the PkVarType enum value.
const char* getPkVarTypeName(PkVarType type);

// Returns the type name of the ObjectType enum value.
const char* getObjectTypeName(ObjectType type);

// Returns the type name of the var [v]. If [v] is an instance of a class
// the return pointer will be the class name string's data, which would be
// dangling if [v] is garbage collected.
const char* varTypeName(Var v);

// Returns the PkVarType of the first class varaible [v].
PkVarType getVarType(Var v);

// Returns true if both variables are the same (ie v1 is v2).
bool isValuesSame(Var v1, Var v2);

// Returns true if both variables are equal (ie v1 == v2).
bool isValuesEqual(Var v1, Var v2);

// Return the hash value of the variable. (variable should be hashable).
uint32_t varHashValue(Var v);

// Return true if the object type is hashable.
bool isObjectHashable(ObjectType type);

// Returns the string version of the [value].
String* toString(PKVM* vm, const Var value);

// Returns the representation version of the [value], similar to python's
// __repr__() method.
String* toRepr(PKVM * vm, const Var value);

// Returns the truthy value of the var.
bool toBool(Var v);

#endif // PK_VAR_H
