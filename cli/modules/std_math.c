/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include "modules.h"

#include <math.h>

// M_PI is non standard. For a portable solution, we're defining it ourselves.
#define PK_PI 3.14159265358979323846

DEF(stdMathFloor,
  "floor(value:num) -> num\n") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, floor(num));
}

DEF(stdMathCeil,
  "ceil(value:num) -> num\n") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, ceil(num));
}

DEF(stdMathPow,
  "pow(a:num, b:num) -> num\n") {

  double num, ex;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  if (!pkValidateSlotNumber(vm, 2, &ex)) return;
  pkSetSlotNumber(vm, 0, pow(num, ex));
}

DEF(stdMathSqrt,
  "sqrt(value:num) -> num\n") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, sqrt(num));
}

DEF(stdMathAbs,
  "abs(value:num) -> num\n") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  if (num < 0) num = -num;
  pkSetSlotNumber(vm, 0, num);
}

DEF(stdMathSign,
  "sign(value:num) -> num\n") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  if (num < 0) num = -1;
  else if (num > 0) num = +1;
  else num = 0;
  pkSetSlotNumber(vm, 0, num);
}

DEF(stdMathSine,
  "sin(rad:num) -> num\n"
  "Return the sine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkValidateSlotNumber(vm, 1, &rad)) return;
  pkSetSlotNumber(vm, 0, sin(rad));
}

DEF(stdMathCosine,
  "cos(rad:num) -> num\n"
  "Return the cosine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkValidateSlotNumber(vm, 1, &rad)) return;
  pkSetSlotNumber(vm, 0, cos(rad));
}

DEF(stdMathTangent,
  "tan(rad:num) -> num\n"
  "Return the tangent value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkValidateSlotNumber(vm, 1, &rad)) return;
  pkSetSlotNumber(vm, 0, tan(rad));
}

DEF(stdMathSinh,
  "sinh(val) -> val\n"
  "Return the hyperbolic sine value of the argument [val].") {

  double val;
  if (!pkValidateSlotNumber(vm, 1, &val)) return;
  pkSetSlotNumber(vm, 0, sinh(val));
}

DEF(stdMathCosh,
  "cosh(val) -> val\n"
  "Return the hyperbolic cosine value of the argument [val].") {

  double val;
  if (!pkValidateSlotNumber(vm, 1, &val)) return;
  pkSetSlotNumber(vm, 0, cosh(val));
}

DEF(stdMathTanh,
  "tanh(val) -> val\n"
  "Return the hyperbolic tangent value of the argument [val].") {

  double val;
  if (!pkValidateSlotNumber(vm, 1, &val)) return;
  pkSetSlotNumber(vm, 0, tanh(val));
}

DEF(stdMathArcSine,
  "asin(num) -> num\n"
  "Return the arcsine value of the argument [num] which is an angle "
  "expressed in radians.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;

  if (num < -1 || 1 < num) {
    pkSetRuntimeError(vm, "Argument should be between -1 and +1");
  }

  pkSetSlotNumber(vm, 0, asin(num));
}

DEF(stdMathArcCosine,
  "acos(num) -> num\n"
  "Return the arc cosine value of the argument [num] which is "
  "an angle expressed in radians.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;

  if (num < -1 || 1 < num) {
    pkSetRuntimeError(vm, "Argument should be between -1 and +1");
  }

  pkSetSlotNumber(vm, 0, acos(num));
}

DEF(stdMathArcTangent,
  "atan(num) -> num\n"
  "Return the arc tangent value of the argument [num] which is "
  "an angle expressed in radians.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, atan(num));
}

DEF(stdMathLog10,
  "log10(value:num) -> num\n"
  "Return the logarithm to base 10 of argument [value]") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, log10(num));
}

DEF(stdMathRound,
  "round(value:num) -> num\n"
  "Round to nearest integer, away from zero and return the number.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, round(num));
}

void registerModuleMath(PKVM* vm) {

  PkHandle* math = pkNewModule(vm, "math");

  // Set global value PI.
  pkReserveSlots(vm, 2);
  pkSetSlotHandle(vm, 0, math);   // slot[0]    = math
  pkSetSlotNumber(vm, 1, PK_PI);  // slot[1]    = 3.14
  pkSetAttribute(vm, 0, 1, "PI"); // slot[0].PI = slot[1]

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

  pkRegisterModule(vm, math);
  pkReleaseHandle(vm, math);
}
