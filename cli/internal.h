/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#ifndef COMMON_H
#define COMMON_H

#include <pocketlang.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "modules/common.h"

#if defined(__GNUC__)
  #pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
  #pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined(__clang__)
  #pragma clang diagnostic ignored "-Wint-to-pointer-cast"
  #pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(_MSC_VER)
  #pragma warning(disable:26812)
#endif

#define CLI_NOTICE                                                            \
  "PocketLang " PK_VERSION_STRING " (https://github.com/ThakeeNathees/pocketlang/)\n" \
  "Copyright (c) 2020-2021 ThakeeNathees\n"                                   \
  "Copyright (c) 2021-2022 Pocketlang Contributors\n"                         \
  "Free and open source software under the terms of the MIT license.\n"

 // FIXME: the vm user data of cli.
typedef struct {
  bool repl_mode;
} VmUserData;

#endif // COMMON_H
