/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "modules.h"

#include <math.h>

// M_PI is non standard. The macro _USE_MATH_DEFINES defining before importing
// <math.h> will define the constants for MSVC. But for a portable solution,
// we're defining it ourselves if it isn't already.
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

DEF(stdMathFloor,
  "floor(value:num) -> num\n") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  pkReturnNumber(vm, floor(num));
}

DEF(stdMathCeil,
  "ceil(value:num) -> num\n") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  pkReturnNumber(vm, ceil(num));
}

DEF(stdMathPow,
  "pow(value:num) -> num\n") {

  double num, ex;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  if (!pkGetArgNumber(vm, 2, &ex)) return;
  pkReturnNumber(vm, pow(num, ex));
}

DEF(stdMathSqrt,
  "sqrt(value:num) -> num\n") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  pkReturnNumber(vm, sqrt(num));
}

DEF(stdMathAbs,
  "abs(value:num) -> num\n") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  if (num < 0) num = -num;
  pkReturnNumber(vm, num);
}

DEF(stdMathSign,
  "sign(value:num) -> num\n") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  if (num < 0) num = -1;
  else if (num > 0) num = +1;
  else num = 0;
  pkReturnNumber(vm, num);
}

DEF(stdMathSine,
  "sin(rad:num) -> num\n"
  "Return the sine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkGetArgNumber(vm, 1, &rad)) return;
  pkReturnNumber(vm, sin(rad));
}

DEF(stdMathCosine,
  "cos(rad:num) -> num\n"
  "Return the cosine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkGetArgNumber(vm, 1, &rad)) return;
  pkReturnNumber(vm, cos(rad));
}

DEF(stdMathTangent,
  "tan(rad:num) -> num\n"
  "Return the tangent value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkGetArgNumber(vm, 1, &rad)) return;
  pkReturnNumber(vm, tan(rad));
}

DEF(stdMathSinh,
  "sinh(val) -> val\n"
  "Return the hyperbolic sine value of the argument [val].") {

  double val;
  if (!pkGetArgNumber(vm, 1, &val)) return;
  pkReturnNumber(vm, sinh(val));
}

DEF(stdMathCosh,
  "cosh(val) -> val\n"
  "Return the hyperbolic cosine value of the argument [val].") {

  double val;
  if (!pkGetArgNumber(vm, 1, &val)) return;
  pkReturnNumber(vm, cosh(val));
}

DEF(stdMathTanh,
  "tanh(val) -> val\n"
  "Return the hyperbolic tangent value of the argument [val].") {

  double val;
  if (!pkGetArgNumber(vm, 1, &val)) return;
  pkReturnNumber(vm, tanh(val));
}

DEF(stdMathArcSine,
  "asin(num) -> num\n"
  "Return the arcsine value of the argument [num] which is an angle "
  "expressed in radians.") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;

  if (num < -1 || 1 < num) {
    pkSetRuntimeError(vm, "Argument should be between -1 and +1");
  }

  pkReturnNumber(vm, asin(num));
}

DEF(stdMathArcCosine,
  "acos(num) -> num\n"
  "Return the arc cosine value of the argument [num] which is "
  "an angle expressed in radians.") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;

  if (num < -1 || 1 < num) {
    pkSetRuntimeError(vm, "Argument should be between -1 and +1");
  }

  pkReturnNumber(vm, acos(num));
}

DEF(stdMathArcTangent,
  "atan(num) -> num\n"
  "Return the arc tangent value of the argument [num] which is "
  "an angle expressed in radians.") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  pkReturnNumber(vm, atan(num));
}

DEF(stdMathLog10,
  "log10(value:num) -> num\n"
  "Return the logarithm to base 10 of argument [value]") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  pkReturnNumber(vm, log10(num));
}

DEF(stdMathRound,
  "round(value:num) -> num\n"
  "Round to nearest integer, away from zero and return the number.") {

  double num;
  if (!pkGetArgNumber(vm, 1, &num)) return;
  pkReturnNumber(vm, round(num));
}

void registerModuleMath(PKVM* vm) {

  PkHandle* math = pkNewModule(vm, "math");

  pkModuleAddFunction(vm, math, "floor",  stdMathFloor,      1);
  pkModuleAddFunction(vm, math, "ceil",   stdMathCeil,       1);
  pkModuleAddFunction(vm, math, "pow",    stdMathPow,        2);
  pkModuleAddFunction(vm, math, "sqrt",   stdMathSqrt,       1);
  pkModuleAddFunction(vm, math, "abs",    stdMathAbs,        1);
  pkModuleAddFunction(vm, math, "sign",   stdMathSign,       1);
  pkModuleAddFunction(vm, math, "sin",    stdMathSine,       1);
  pkModuleAddFunction(vm, math, "cos",    stdMathCosine,     1);
  pkModuleAddFunction(vm, math, "tan",    stdMathTangent,    1);
  pkModuleAddFunction(vm, math, "sinh",   stdMathSinh,       1);
  pkModuleAddFunction(vm, math, "cosh",   stdMathCosh,       1);
  pkModuleAddFunction(vm, math, "tanh",   stdMathTanh,       1);
  pkModuleAddFunction(vm, math, "asin",   stdMathArcSine,    1);
  pkModuleAddFunction(vm, math, "acos",   stdMathArcCosine,  1);
  pkModuleAddFunction(vm, math, "atan",   stdMathArcTangent, 1);
  pkModuleAddFunction(vm, math, "log10",  stdMathLog10,      1);
  pkModuleAddFunction(vm, math, "round",  stdMathRound,      1);

  // FIXME:
  // Refactor native type interface and add PI as a global to the module.
  //
  // Note that currently it's mutable (since it's a global variable, not
  // constant and pocketlang doesn't support constant) so the user shouldn't
  // modify the PI, like in python.
  //pkModuleAddGlobal(vm, math, "PI", Handle-Of-PI);

  pkRegisterModule(vm, math);
  pkReleaseHandle(vm, math);
}
