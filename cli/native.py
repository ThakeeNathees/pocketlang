#!python
## Copyright (c) 2020-2021 Thakee Nathees
## Copyright (c) 2021-2022 Pocketlang Contributors
## Distributed Under The MIT License

import re, os
from os.path import (join, exists, abspath,
                     relpath, dirname)

## The absolute path of this file, when run as a script.
## This file is not intended to be included in other files at the moment.
THIS_PATH = abspath(dirname(__file__))

POCKET_HEADER = join(THIS_PATH, "../src/include/pocketlang.h")
TARGET_NATIVE = join(THIS_PATH, "./modules/pknative.gen.c")
TARGET_DL     = join(THIS_PATH, "./modules/std_dl_api.gen.h")

PK_API = "pk_api"
PK_API_TYPE = "PkNativeApi"
API_DEF = f'''\
static {PK_API_TYPE} {PK_API};

void pkInitApi({PK_API_TYPE}* api) {{%s
}}
'''

SOURCE_GEN = f'''\
/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */

#include <pocketlang.h>

// !! THIS FILE IS GENERATED DO NOT EDIT !!

'''

def get_source():
  f = open(POCKET_HEADER, 'r')
  source = f.read()
  f.close()
  start = source.find("POCKETLANG PUBLIC API")
  source = source[start:]
  return source

MULTIPLE_WHITE_SPACES = re.compile(r"\s+")
def flaten(s):
  return MULTIPLE_WHITE_SPACES.sub(" ", s).strip()

def parse_definition(_def):
  assert '\n' not in _def
  pattern = r'PK_PUBLIC (.*?) ([a-zA-Z0-9_]+)\('
  match = re.search(pattern, _def)
  assert(match)
  return_type = match.group(1)
  fn_name = match.group(2)
  params = []
  if '()' in _def or '(void)' in _def:
    pass ## No parameters.
  else:
    params_match = _def[_def.find('(')+1:_def.find(')')]
    for param in params_match.split(','):
      last_space = param.rfind(' ')
      param_type = param[:last_space].strip()
      param_name = param[last_space:].strip()
      params.append((param_name, param_type))
  return fn_name, params, return_type

def get_api_functions():
  api_functions = []
  source = get_source()
  match = re.findall(r'^PK_PUBLIC [\S\n ]+?;', source, re.MULTILINE)
  for m in match:
    definition = flaten(m)
    api_functions.append(parse_definition(definition))
  return api_functions

def fn_typedefs(api_functions):
  typedefs = ""
  for fn, params, ret in api_functions:
    params_join = ", ".join(map(lambda x: x[1], params))
    typedefs += f'typedef {ret} (*{fn}_t)({params_join});\n'
  return typedefs + '\n'

def api_typedef(api_functions):
  typedef = "typedef struct {\n"
  for fn, params, ret in api_functions:
    typedef += f"  {fn}_t {fn}_ptr;\n"
  typedef += f"}} {PK_API_TYPE};\n"
  return typedef + '\n'

def define_functions(api_functions):
  definitions = ""
  for fn, params, ret in api_functions:
    fn_def = ""
    params_join = ", ".join(map(lambda x: x[1] + " " + x[0], params))
    param_names_join = ", ".join(map(lambda x: x[0], params))
    return_ = "" if ret == "void" else "return "
    fn_def += f'{ret} {fn}({params_join}) {{\n'
    fn_def += f"  {return_}{PK_API}.{fn}_ptr({param_names_join});\n"
    fn_def += "}\n\n"
    definitions += fn_def
  return definitions[:-1] ## Skip the last newline.

def init_api(api_functions):
  assign = ""
  for fn, params, ret in api_functions:
    assign += f"\n  {PK_API}.{fn}_ptr = api->{fn}_ptr;"
  return API_DEF % assign + '\n'

def make_api(api_functions):
  source = "#if defined(NATIVE_API_IMPLEMENT)\n\n"
  source += f"{PK_API_TYPE} dlMakeApi() {{\n\n"
  source += f"  {PK_API_TYPE} api;\n\n"
  for fn, params, ret in api_functions:
    source += f"  api.{fn}_ptr = {fn};\n"
  source += "\n"
  source += "  return api;\n"
  source += "}\n\n"
  source += "#endif // NATIVE_API_IMPLEMENT\n\n"
  return source

def generate():
  api_functions = get_api_functions()

  ## Generate pocket native api.
  with open(TARGET_NATIVE, 'w') as fp:
    fp.write(SOURCE_GEN)
    fp.write(fn_typedefs(api_functions))
    fp.write(api_typedef(api_functions))
    fp.write(init_api(api_functions))
    fp.write(define_functions(api_functions))

  ## Generate dl module api definition.
  with open(TARGET_DL, 'w') as fp:
    fp.write(SOURCE_GEN)
    fp.write(fn_typedefs(api_functions))
    fp.write(api_typedef(api_functions))
    fp.write(make_api(api_functions))

if __name__ == "__main__":
  generate()
  print("Generated:", relpath(TARGET_NATIVE, os.getcwd()))
  print("Generated:", relpath(TARGET_DL, os.getcwd()))
