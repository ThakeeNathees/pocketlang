/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef PK_CORE_H
#define PK_CORE_H

#ifndef PK_AMALGAMATED
#include "internal.h"
#include "value.h"
#endif

// Literal strings used in various places in pocketlang. For now these are
// defined as macros so that it'll be easier in the future to refactor or
// restructre. The names of the macros are begin with LIST_ and the string.
#define LITS__init      "_init"
#define LITS__str       "_str"
#define LITS__repr      "_repr"

// Functions, methods, classes and  other names which are intrenal / special to
// pocketlang are starts with the following character (ex: @main, @literalFn).
// When importing all (*) from a module, if the name of an entry starts with
// this character it'll be skipped.
#define SPECIAL_NAME_CHAR '@'

// Name of the implicit function for a module. When a module is parsed all of
// it's statements are wrapped around an implicit function with this name.
#define IMPLICIT_MAIN_NAME "@main"

// Name of a literal function. All literal function will have the same name but
// they're uniquely identified by their index in the script's function buffer.
#define LITERAL_FN_NAME "@func"

// Name of a constructor function.
#define CTOR_NAME LITS__init

// Getter/Setter method names used by the native instance to get/ set value.
// Script instance's values doesn't support methods but they use vanila
// '.attrib', '.attrib=' operators.
#define GETTER_NAME "@getter"
#define SETTER_NAME "@setter"

// Initialize core language, builtin function and core libs.
void initializeCore(PKVM* vm);

// Initialize a module. If the script has path, it'll define __file__ global
// as an absolute path of the module. [path] will be the normalized absolute
// path of the module. If the module's path is NULL, it's name is used.
//
// Also define __name__ as the name of the module, assuming all the modules
// have name excpet for main which. for main the name will be defined as
// '__main__' just like python.
void initializeModule(PKVM* vm, Module* module, bool is_main);

// Create a new module with the given [name] and returns as a Module*.
// This is function is a wrapper around `newModule()` function to create
// native modules for pocket core and public native api.
Module* newModuleInternal(PKVM* vm, const char* name);

// Adds a function to the module with the give properties and add the function
// to the module's globals variables.
void moduleAddFunctionInternal(PKVM* vm, Module* module,
                               const char* name, pkNativeFn fptr,
                               int arity, const char* docstring);

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

// This method is called just before constructing a type to initialize self
// and after that the constructor will be called. For builtin types this
// function will return VAR_NULL and the constructor will override self to
// it's instance (because for some classes we cannot create without argument
// example Fiber(fn), Range(from, to) etc). If the class cannot be
// instanciated (ex: Class 'Module') it'll set an error and return VAR_NULL.
// For other classes the return value will be an Instance.
Var preConstructSelf(PKVM* vm, Class* cls);

// Returns the class of the [instance].
Class* getClass(PKVM* vm, Var instance);

// Returns the method (closure) in the instance [self]. If it's not an method
// but just an attribute the [is_method] pointer will be set to false and
// returns the value.
// If the method / attribute not found, it'll set a runtime error on the VM.
Var getMethod(PKVM* vm, Var self, String* name, bool* is_method);

// Returns the method (closure) from the instance's super class. If the method
// doesn't exists, it'll set an error on the VM.
Closure* getSuperMethod(PKVM* vm, Var self, String* name);

// Unlike getMethod this will not set error and will not try to get attribute
// with the same name. It'll return true if the method exists on [self], false
// otherwise and if the [method] argument is not NULL, method will be set.
bool hasMethod(PKVM* vm, Var self, String* name, Closure** method);

// Returns the string value of the variable, a wrapper of toString() function
// but for instances it'll try to calll "_to_string" function and on error
// it'll return NULL.
// if parameter [repr] is true it'll return repr string of the value and for
// instances it'll call "_repr()" method.
// Note that if _str method does not exists it'll use _repr method for to
// string.
String* varToString(PKVM* vm, Var self, bool repr);

Var varPositive(PKVM* vm, Var v); // Returns +v.
Var varNegative(PKVM* vm, Var v); // Returns -v.
Var varNot(PKVM* vm, Var v);      // Returns !v.
Var varBitNot(PKVM* vm, Var v);   // Returns ~v.

Var varAdd(PKVM* vm, Var v1, Var v2, bool inplace);       // Returns v1 + v2.
Var varSubtract(PKVM* vm, Var v1, Var v2, bool inplace);  // Returns v1 - v2.
Var varMultiply(PKVM* vm, Var v1, Var v2, bool inplace);  // Returns v1 * v2.
Var varDivide(PKVM* vm, Var v1, Var v2, bool inplace);    // Returns v1 / v2.
Var varExponent(PKVM* vm, Var v1, Var v2, bool inplace);  // Returns v1 ** v2.
Var varModulo(PKVM* vm, Var v1, Var v2, bool inplace);    // Returns v1 % v2.

Var varBitAnd(PKVM* vm, Var v1, Var v2, bool inplace);    // Returns v1 & v2.
Var varBitOr(PKVM* vm, Var v1, Var v2, bool inplace);     // Returns v1 | v2.
Var varBitXor(PKVM* vm, Var v1, Var v2, bool inplace);    // Returns v1 ^ v2.
Var varBitLshift(PKVM* vm, Var v1, Var v2, bool inplace); // Returns v1 << v2.
Var varBitRshift(PKVM* vm, Var v1, Var v2, bool inplace); // Returns v1 >> v2.

Var varEqals(PKVM* vm, Var v1, Var v2);       // Returns v1 == v2.
Var varGreater(PKVM* vm, Var v1, Var v2);     // Returns v1 > v2.
Var varLesser(PKVM* vm, Var v1, Var v2);      // Returns v1 < v2.

Var varOpRange(PKVM* vm, Var v1, Var v2);     // Returns v1 .. v2.

// Returns [elem] in [container]. Sets an error if the [container] is not an
// iterable.
bool varContains(PKVM* vm, Var elem, Var container);

// Returns [inst] is [type]. Sets an error if the [type] is not a class.
bool varIsType(PKVM* vm, Var inst, Var type);

// Returns the attribute named [attrib] on the variable [on].
Var varGetAttrib(PKVM* vm, Var on, String* attrib);

// Set the attribute named [attrib] on the variable [on] with the given
// [value].
void varSetAttrib(PKVM* vm, Var on, String* name, Var value);

// Returns the subscript value (ie. on[key]).
Var varGetSubscript(PKVM* vm, Var on, Var key);

// Set subscript [value] with the [key] (ie. on[key] = value).
void varsetSubscript(PKVM* vm, Var on, Var key, Var value);

#endif // PK_CORE_H
