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

// Note that the cli itself is not a part of the pocketlang compiler, instead
// it's a host application to run pocketlang from the command line. We're
// embedding the pocketlang VM and we can only use its public APIs, not any
// internals of it, including assertion macros. So that we've copyied the
// "common.h" header. This can be moved to "src/include/common.h" and include
// as optional header, which is something I don't like because it makes
// pocketlang contain 2 headers (we'll always try to be minimal).
#include "common.h"

#define CLI_NOTICE                                                            \
  "PocketLang " PK_VERSION_STRING " (https://github.com/ThakeeNathees/pocketlang/)\n" \
  "Copyright(c) 2020 - 2021 ThakeeNathees.\n"                                 \
  "Free and open source software under the terms of the MIT license.\n"

 // FIXME: the vm user data of cli.
typedef struct {
  bool repl_mode;
} VmUserData;

#endif // COMMON_H
