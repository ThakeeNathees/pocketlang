/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <math.h>

#ifndef PK_AMALGAMATED
#include "libs.h"
#endif

// M_PI is non standard. For a portable solution, we're defining it ourselves.
#define PK_PI 3.14159265358979323846

DEF(stdMathFloor,
  "math.floor(value:Numberber) -> Numberber",
  "Return the floor value.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, floor(num));
}

DEF(stdMathCeil,
  "math.ceil(value:Number) -> Number",
  "Returns the ceiling value.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, ceil(num));
}

DEF(stdMathPow,
  "math.pow(a:Number, b:Number) -> Number",
  "Returns the power 'b' of 'a' similler to a**b.") {

  double num, ex;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  if (!pkValidateSlotNumber(vm, 2, &ex)) return;
  pkSetSlotNumber(vm, 0, pow(num, ex));
}

DEF(stdMathSqrt,
  "math.sqrt(value:Number) -> Number",
  "Returns the square root of the value") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, sqrt(num));
}

DEF(stdMathAbs,
  "math.abs(value:Number) -> Number",
  "Returns the absolute value.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  if (num < 0) num = -num;
  pkSetSlotNumber(vm, 0, num);
}

DEF(stdMathSign,
  "math.sign(value:Number) -> Number",
  "return the sign of the which is one of (+1, 0, -1).") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  if (num < 0) num = -1;
  else if (num > 0) num = +1;
  else num = 0;
  pkSetSlotNumber(vm, 0, num);
}

DEF(stdMathSine,
  "math.sin(rad:Number) -> Number",
  "Return the sine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkValidateSlotNumber(vm, 1, &rad)) return;
  pkSetSlotNumber(vm, 0, sin(rad));
}

DEF(stdMathCosine,
  "math.cos(rad:Number) -> Number",
  "Return the cosine value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkValidateSlotNumber(vm, 1, &rad)) return;
  pkSetSlotNumber(vm, 0, cos(rad));
}

DEF(stdMathTangent,
  "math.tan(rad:Number) -> Number",
  "Return the tangent value of the argument [rad] which is an angle expressed "
  "in radians.") {

  double rad;
  if (!pkValidateSlotNumber(vm, 1, &rad)) return;
  pkSetSlotNumber(vm, 0, tan(rad));
}

DEF(stdMathSinh,
  "math.sinh(val:Number) -> Number",
  "Return the hyperbolic sine value of the argument [val].") {

  double val;
  if (!pkValidateSlotNumber(vm, 1, &val)) return;
  pkSetSlotNumber(vm, 0, sinh(val));
}

DEF(stdMathCosh,
  "math.cosh(val:Number) -> Number",
  "Return the hyperbolic cosine value of the argument [val].") {

  double val;
  if (!pkValidateSlotNumber(vm, 1, &val)) return;
  pkSetSlotNumber(vm, 0, cosh(val));
}

DEF(stdMathTanh,
  "math.tanh(val:Number) -> Number",
  "Return the hyperbolic tangent value of the argument [val].") {

  double val;
  if (!pkValidateSlotNumber(vm, 1, &val)) return;
  pkSetSlotNumber(vm, 0, tanh(val));
}

DEF(stdMathArcSine,
  "math.asin(num:Number) -> Number",
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
  "math.acos(num:Number) -> Number",
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
  "math.atan(num:Number) -> Number",
  "Return the arc tangent value of the argument [num] which is "
  "an angle expressed in radians.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, atan(num));
}

DEF(stdMathArcTan2,
  "math.atan2(y:Number, x:Number) -> Number",
  "These functions calculate the principal value of the arc tangent "
  "of y / x, using the signs of the two arguments to determine the "
  "quadrant of the result") {

  double y, x;
  if (!pkValidateSlotNumber(vm, 1, &y)) return;
  if (!pkValidateSlotNumber(vm, 2, &x)) return;

  pkSetSlotNumber(vm, 0, atan2(y, x));
}

DEF(stdMathLog10,
  "math.log10(value:Number) -> Number",
  "Return the logarithm to base 10 of argument [value]") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, log10(num));
}

DEF(stdMathRound,
  "math.round(value:Number) -> Number",
  "Round to nearest integer, away from zero and return the number.") {

  double num;
  if (!pkValidateSlotNumber(vm, 1, &num)) return;
  pkSetSlotNumber(vm, 0, round(num));
}

DEF(stdMathRand,
  "math.rand() -> Number",
  "Return a random runber in the range of 0..0x7fff.") {

  // RAND_MAX is implementation dependent but is guaranteed to be at least
  // 0x7fff on any standard library implementation.
  // https://www.cplusplus.com/reference/cstdlib/RAND_MAX/
  pkSetSlotNumber(vm, 0, rand() % 0x7fff);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleMath(PKVM* vm) {

  PkHandle* math = pkNewModule(vm, "math");

  // Set global value PI.
  pkReserveSlots(vm, 2);
  pkSetSlotHandle(vm, 0, math);   // slot[0]    = math
  pkSetSlotNumber(vm, 1, PK_PI);  // slot[1]    = 3.14
  pkSetAttribute(vm, 0, "PI", 1); // slot[0].PI = slot[1]

  REGISTER_FN(math, "floor",  stdMathFloor,      1);
  REGISTER_FN(math, "ceil",   stdMathCeil,       1);
  REGISTER_FN(math, "pow",    stdMathPow,        2);
  REGISTER_FN(math, "sqrt",   stdMathSqrt,       1);
  REGISTER_FN(math, "abs",    stdMathAbs,        1);
  REGISTER_FN(math, "sign",   stdMathSign,       1);
  REGISTER_FN(math, "sin",    stdMathSine,       1);
  REGISTER_FN(math, "cos",    stdMathCosine,     1);
  REGISTER_FN(math, "tan",    stdMathTangent,    1);
  REGISTER_FN(math, "sinh",   stdMathSinh,       1);
  REGISTER_FN(math, "cosh",   stdMathCosh,       1);
  REGISTER_FN(math, "tanh",   stdMathTanh,       1);
  REGISTER_FN(math, "asin",   stdMathArcSine,    1);
  REGISTER_FN(math, "acos",   stdMathArcCosine,  1);
  REGISTER_FN(math, "atan",   stdMathArcTangent, 1);
  REGISTER_FN(math, "atan2",  stdMathArcTan2,    2);
  REGISTER_FN(math, "log10",  stdMathLog10,      1);
  REGISTER_FN(math, "round",  stdMathRound,      1);
  REGISTER_FN(math, "rand",   stdMathRand,       0);

  pkRegisterModule(vm, math);
  pkReleaseHandle(vm, math);
}
