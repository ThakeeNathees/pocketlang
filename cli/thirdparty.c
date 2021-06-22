/*
 *  Copyright (c) 2020-2021 Thakee Nathees
 *  Distributed Under The MIT License
 */

// This file will include all thirdparty source files from the thirdparty sub
// directories here into a single file. That'll make it easy to compile with
// the command `gcc cli/*.c ...` instead of having to add every single sub
// directory to the list of source directory.

// Library : cwalk
// Source  : https://github.com/likle/cwalk
// Doc     : https://likle.github.io/cwalk/
// About   : Path library for C/C++. Cross-Platform for Windows, MacOS and
//           Linux. Supports UNIX and Windows path styles on those platforms.
#include "thirdparty/cwalk/cwalk.c"

