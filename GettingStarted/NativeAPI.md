# Native API

This guide covers how to integrate the PocketLang VM into your C application. The PocketLang VM can be embedded in any C99 compatible application with minimal setup. This document covers everything from basic VM usage to creating custom native types and dynamic library extensions.

## Getting Started

To integrate PocketLang VM in your project:

**Option 1: Using Make (Recommended)**

Run `make` or `make release` to build the executable and static library. This will generate:
- `build/Release/include/pocketlang.h` (header file)
- `build/Release/lib/libpocketlang.a` (static library)
- `build/Release/bin/pocket` (executable)

Then link against the library and include the header file in your project.

**Option 2: Manual Integration**

1. Add the `src/` directory to your project
2. Add all C files in `src/core/` and `src/libs/` directories with your source files
3. Add `src/include` to the include path
4. Compile

The examples in this document can be found in the `tests/native/` directory of the repository.

## Basic VM Usage

The simplest way to use PocketLang is to create a VM instance and run some code. Here's a minimal example:

```c
#include <pocketlang.h>

int main(int argc, char** argv) {
  // Create a new pocket VM.
  PKVM* vm = pkNewVM(NULL);
  
  // Run a string.
  pkRunString(vm, "print('hello world')");
  
  // Run from file.
  pkRunFile(vm, "script.pk");
  
  // Free the VM.
  pkFreeVM(vm);
  
  return 0;
}
```

The `pkNewVM()` function creates a new VM instance. You can pass `NULL` to use the default configuration, or pass a `PkConfiguration` structure to customize the VM behavior. After creating the VM, you can run PocketLang code using `pkRunString()` or `pkRunFile()`. When you're done, call `pkFreeVM()` to clean up.

## Passing Values Between C and PocketLang

To make C functions callable from PocketLang, you need to register them as native functions. Native functions receive a `PKVM*` pointer and use the slot-based API to read arguments and return values.

Here's an example that registers a native function and calls it from PocketLang:

```c
#include <pocketlang.h>

// The pocket script we're using to test.
static const char* code =
  "  from my_module import cFunction \n"
  "  a = 42                          \n"
  "  b = cFunction(a)                \n"
  "  print('[pocket] b = $b')        \n"
  ;

// Native function that can be called from PocketLang.
static void cFunction(PKVM* vm) {
  
  // Get the parameter from pocket VM.
  double a;
  if (!pkValidateSlotNumber(vm, 1, &a)) return;
  
  printf("[C] a = %f\n", a);
  
  // Return value to the pocket VM.
  pkSetSlotNumber(vm, 0, 3.14);
}

int main(int argc, char** argv) {
  // Create a new pocket VM.
  PKVM* vm = pkNewVM(NULL);
  
  // Registering a native module.
  PkHandle* my_module = pkNewModule(vm, "my_module");
  pkModuleAddFunction(vm, my_module, "cFunction", cFunction, 1);
  pkRegisterModule(vm, my_module);
  pkReleaseHandle(vm, my_module);
  
  // Run the code.
  PkResult result = pkRunString(vm, code);
  
  // Free the VM.
  pkFreeVM(vm);
  
  return (int) result;
}
```

### Understanding the Slot API

PocketLang uses a slot-based API for passing values between C and PocketLang. Slots are indexed starting from 0:

- **Slot 0**: Reserved for the return value of native functions
- **Slot 1, 2, 3...**: Function arguments (slot 1 is the first argument, slot 2 is the second, etc.)

In the example above:
- `pkValidateSlotNumber(vm, 1, &a)` reads the first argument (slot 1) as a number
- `pkSetSlotNumber(vm, 0, 3.14)` sets the return value (slot 0) to 3.14

### Slot Validation Functions

Before reading a slot value, you should validate its type. The validation functions return `false` if the type doesn't match, and set a runtime error:

- `pkValidateSlotBool(vm, slot, &value)` - Validates and reads a boolean
- `pkValidateSlotNumber(vm, slot, &value)` - Validates and reads a number
- `pkValidateSlotInteger(vm, slot, &value)` - Validates and reads an integer
- `pkValidateSlotString(vm, slot, &value, &length)` - Validates and reads a string
- `pkValidateSlotType(vm, slot, type)` - Validates the slot type matches

### Setting Slot Values

To return values from native functions or set slot values:

- `pkSetSlotNull(vm, slot)` - Set to null
- `pkSetSlotBool(vm, slot, value)` - Set a boolean
- `pkSetSlotNumber(vm, slot, value)` - Set a number
- `pkSetSlotString(vm, slot, value)` - Set a string
- `pkSetSlotHandle(vm, slot, handle)` - Set a handle (module, class, etc.)

## Creating Custom Native Types

You can create custom C types that can be instantiated and used from PocketLang. This is useful for wrapping C structures or creating high-performance native objects.

Here's a complete example that creates a `Vec2` (2D vector) class:

```c
#include <pocketlang.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// The script we're using to test the native Vector type.
static const char* code =
  "  from vector import Vec2               \n"
  "  print('Class     = $Vec2')            \n"
  "                                        \n"
  "  v1 = Vec2(1, 2)                       \n"
  "  print('v1        = $v1')              \n"
  "  print('v1.length = ${v1.length}')     \n"
  "  print()                               \n"
  "                                        \n"
  "  v1.x = 3; v1.y = 4;                   \n"
  "  print('v1        = $v1')              \n"
  "  print('v1.length = ${v1.length}')     \n"
  "  print()                               \n"
  "                                        \n"
  "  v2 = Vec2(5, 6)                       \n"
  "  print('v2        = $v2')              \n"
  "  v3 = v1 + v2                          \n"
  "  print('v3        = $v3')              \n"
  "                                        \n"
  ;

// The Vector structure
typedef struct {
  double x, y;
} Vector;

// Native instance allocation callback.
void* _newVec(PKVM* vm) {
  Vector* vec = pkRealloc(vm, NULL, sizeof(Vector));
  vec->x = 0;
  vec->y = 0;
  return vec;
}

// Native instance de-allocation callback.
void _deleteVec(PKVM* vm, void* vec) {
  pkRealloc(vm, vec, 0);
}

// Getter for properties (x, y, length)
void _vecGetter(PKVM* vm) {
  const char* name = pkGetSlotString(vm, 1, NULL);
  Vector* self = (Vector*)pkGetSelf(vm);
  if (strcmp("x", name) == 0) {
    pkSetSlotNumber(vm, 0, self->x);
    return;
  } else if (strcmp("y", name) == 0) {
    pkSetSlotNumber(vm, 0, self->y);
    return;
  } else if (strcmp("length", name) == 0) {
    double length = sqrt(pow(self->x, 2) + pow(self->y, 2));
    pkSetSlotNumber(vm, 0, length);
    return;
  }
}

// Setter for properties (x, y)
void _vecSetter(PKVM* vm) {
  const char* name = pkGetSlotString(vm, 1, NULL);
  Vector* self = (Vector*)pkGetSelf(vm);
  if (strcmp("x", name) == 0) {
    double x;
    if (!pkValidateSlotNumber(vm, 2, &x)) return;
    self->x = x;
    return;
  } else if (strcmp("y", name) == 0) {
    double y;
    if (!pkValidateSlotNumber(vm, 2, &y)) return;
    self->y = y;
    return;
  }
}

// Constructor (_init method)
void _vecInit(PKVM* vm) {
  double x, y;
  if (!pkValidateSlotNumber(vm, 1, &x)) return;
  if (!pkValidateSlotNumber(vm, 2, &y)) return;

  Vector* self = (Vector*) pkGetSelf(vm);
  self->x = x;
  self->y = y;
}

// Vec2 '+' operator method.
void _vecAdd(PKVM* vm) {
  Vector* self = (Vector*) pkGetSelf(vm);

  pkReserveSlots(vm, 5); // Now we have slots [0, 1, 2, 3, 4].

  pkPlaceSelf(vm, 2);   // slot[2] = self
  pkGetClass(vm, 2, 2); // slot[2] = Vec2 class.

  // slot[1] is slot[2] == other is Vec2 ?
  if (!pkValidateSlotInstanceOf(vm, 1, 2)) return;
  Vector* other = (Vector*) pkGetSlotNativeInstance(vm, 1);

  // slot[3] = new.x
  pkSetSlotNumber(vm, 3, self->x + other->x);

  // slot[4] = new.y
  pkSetSlotNumber(vm, 4, self->y + other->y);

  // slot[0] = Vec2(slot[3], slot[4]) => return value.
  if (!pkNewInstance(vm, 2, 0, 2, 3)) return;
}

// String representation (_str method)
void _vecStr(PKVM* vm) {
  Vector* self = (Vector*)pkGetSelf(vm);
  pkSetSlotStringFmt(vm, 0, "[%g, %g]", self->x, self->y);
}

// Register the 'Vector' module and it's functions.
void registerVector(PKVM* vm) {
  PkHandle* vector = pkNewModule(vm, "vector");

  PkHandle* Vec2 = pkNewClass(vm, "Vec2", NULL /*Base Class*/,
                              vector, _newVec, _deleteVec);

  pkClassAddMethod(vm, Vec2, "@getter", _vecGetter, 1);
  pkClassAddMethod(vm, Vec2, "@setter", _vecSetter, 2);
  pkClassAddMethod(vm, Vec2, "_init",   _vecInit,   2);
  pkClassAddMethod(vm, Vec2, "_str",    _vecStr,    0);
  pkClassAddMethod(vm, Vec2, "+",       _vecAdd,    1);
  pkReleaseHandle(vm, Vec2);

  pkRegisterModule(vm, vector);
  pkReleaseHandle(vm, vector);
}

int main(int argc, char** argv) {
  PKVM* vm = pkNewVM(NULL);
  registerVector(vm);
  pkRunString(vm, code);
  pkFreeVM(vm);
  
  return 0;
}
```

### Key Concepts for Native Types

1. **Allocation and Deallocation**: The `_newVec` function allocates memory for the native instance using `pkRealloc()`. The `_deleteVec` function frees it. Always use `pkRealloc()` instead of `malloc()`/`free()` so the VM can track memory usage.

2. **Getting Self**: Use `pkGetSelf(vm)` to get a pointer to the native instance from within a method.

3. **Property Access**: The `@getter` and `@setter` special methods handle property access. Slot 1 contains the property name, and slot 2 (for setters) contains the value.

4. **Constructor**: The `_init` method is called when creating a new instance. Arguments are passed starting from slot 1.

5. **Operator Overloading**: You can overload operators like `+`, `-`, `*`, etc. by registering methods with those names.

6. **String Representation**: The `_str` method is called when converting the instance to a string (e.g., in `print()`).

7. **Reserving Slots**: When you need multiple slots, call `pkReserveSlots(vm, count)` first. This ensures you have enough slots available.

## Dynamic Library Extensions

PocketLang supports loading native extensions as dynamic libraries (`.so` on Linux, `.dll` on Windows). This allows you to create extensions that can be loaded at runtime without recompiling the main application.

### Creating a Dynamic Library Extension

Here's a minimal example of a dynamic library extension:

```c
#include <pocketlang.h>

PK_EXPORT void hello(PKVM* vm) {
  pkSetSlotString(vm, 0, "hello from dynamic lib.");
}

PK_EXPORT PkHandle* pkExportModule(PKVM* vm) {
  PkHandle* mylib = pkNewModule(vm, "mylib");
  
  pkModuleAddFunction(vm, mylib, "hello", hello, 0);

  return mylib;
}
```

The `PK_EXPORT` macro marks functions that should be exported from the dynamic library. The `pkExportModule` function is the entry point that the VM calls when loading the extension.

### Using the Extension from PocketLang

```ruby
## Import the native extension module.
## from either mylib.so, or mylib.dll
import mylib

if _name == "@main"
  ## Call the registered function.
  print('mylib.hello() = ${mylib.hello()}')
end
```

### Compiling Dynamic Library Extensions

#### GCC
```bash
gcc -fPIC -c mylib.c pknative.c -I../../../src/include/
gcc -shared -o mylib.so mylib.o pknative.o
rm *.o
```

#### MSVC
```cmd
cl /LD mylib.c pknative.c /I../../../src/include/
rm *.obj *.exp *.lib
```

Note: When creating dynamic library extensions, you need to include `pknative.c` which provides the native API wrapper. This file is generated and contains function pointers to all PocketLang API functions, allowing the extension to work without linking against the PocketLang library directly.

## Memory Management

Always use `pkRealloc()` instead of `malloc()`/`free()` when allocating memory for native instances. This allows the VM to track memory usage and integrate with the garbage collector. The function signature is:

```c
void* pkRealloc(PKVM* vm, void* ptr, size_t size);
```

- If `ptr` is `NULL` and `size > 0`, it allocates new memory
- If `ptr` is not `NULL` and `size > 0`, it reallocates existing memory
- If `ptr` is not `NULL` and `size == 0`, it frees the memory

## Error Handling

When a validation function returns `false`, it means the argument type was incorrect and a runtime error has been set. You should return immediately from your native function:

```c
double value;
if (!pkValidateSlotNumber(vm, 1, &value)) return;
// Continue with valid value...
```

You can also set custom runtime errors using `pkSetRuntimeError()`:

```c
pkSetRuntimeError(vm, "Custom error message");
return;
```

## Handle Management

Handles (modules, classes, etc.) are reference-counted. When you create a handle with functions like `pkNewModule()` or `pkNewClass()`, you must release it when you're done using `pkReleaseHandle()`:

```c
PkHandle* module = pkNewModule(vm, "my_module");
// ... use module ...
pkReleaseHandle(vm, module);
```

Failure to release handles will cause memory leaks.

## Compiling Examples

All examples can be compiled with:

#### GCC / MinGw / Clang
```bash
gcc example0.c -o example0 ../../src/core/*.c ../../src/libs/*.c -I../../src/include -lm
```

#### MSVC
```cmd
cl example0.c ../../src/core/*.c ../../src/libs/*.c /I../../src/include
```

Note: On Unix-like systems, you may need to link with `-ldl` for dynamic library support.

