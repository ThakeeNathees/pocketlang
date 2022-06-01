#!python
## Copyright (c) 2020-2021 Thakee Nathees
## Copyright (c) 2021-2022 Pocketlang Contributors
## Distributed Under The MIT License

import re, os
from os.path import (join, exists, abspath,
                     relpath, dirname)

## Pocket lang root directory. All the listed paths bellow are relative to
## the root path.
ROOT_PATH = abspath(join(dirname(__file__), "../"))

NATIVE_API_EXT = "pknative.c"
NATIVE_API_IMP = "nativeapi.h"

POCKET_HEADER = join(ROOT_PATH, f"src/include/pocketlang.h")
TARGET_EXT    = join(ROOT_PATH, f"tests/native/dl/{NATIVE_API_EXT}")
TARGET_IMP    = join(ROOT_PATH, f"src/libs/gen/{NATIVE_API_IMP}")
DL_IMPLEMENT  = 'PK_DL_IMPLEMENT'

PK_API = "pk_api"
PK_API_TYPE = "PkNativeApi"
PK_API_INIT = 'pkInitApi'
PK_EXPORT_MODULE = 'pkExportModule'
PK_CLEANUP_MODULE = 'pkCleanupModule'

API_DEF = f'''\
static {PK_API_TYPE} {PK_API};

PK_EXPORT void {PK_API_INIT}({PK_API_TYPE}* api) {{%s
}}
'''

## '%s' left for other includes.
HEADER = f'''\
/*
 *  Copyright (c) 2020-2022 Thakee Nathees
 *  Copyright (c) 2021-2022 Pocketlang Contributors
 *  Distributed Under The MIT License
 */
%s
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
    if '...' in definition: continue ## FIXME: VA ARGS
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
  source = f"{PK_API_TYPE} pkMakeNativeAPI() {{\n\n"
  source += f"  {PK_API_TYPE} api;\n\n"
  for fn, params, ret in api_functions:
    source += f"  api.{fn}_ptr = {fn};\n"
  source += "\n"
  source += "  return api;\n"
  source += "}\n\n"
  return source

def generate():
  api_functions = get_api_functions()

  ## Generate pocket native header.
  with open(TARGET_EXT, 'w') as fp:
    fp.write(HEADER % '\n#include <pocketlang.h>\n')
    fp.write(fn_typedefs(api_functions))
    fp.write(api_typedef(api_functions))
    fp.write(init_api(api_functions))
    fp.write(define_functions(api_functions))

  ## Generate native api source.
  with open(TARGET_IMP, 'w') as fp:
    fp.write(HEADER % '\n#ifndef PK_AMALGAMATED\n#include <pocketlang.h>\n#endif\n\n')

    fp.write(fn_typedefs(api_functions))
    fp.write(api_typedef(api_functions))

    fp.write(f'#define PK_API_INIT_FN_NAME "{PK_API_INIT}" \n')
    fp.write(f'#define PK_EXPORT_FN_NAME "{PK_EXPORT_MODULE}" \n\n')
    fp.write(f'#define PK_CLEANUP_FN_NAME "{PK_CLEANUP_MODULE}" \n\n')
    fp.write(f'typedef void (*{PK_API_INIT}Fn)({PK_API_TYPE}*);\n')
    fp.write(f'typedef PkHandle* (*{PK_EXPORT_MODULE}Fn)(PKVM*);\n')
    fp.write(f'\n')

    fp.write(f'#ifdef {DL_IMPLEMENT}\n\n')
    fp.write(make_api(api_functions))
    fp.write(f'#endif // {DL_IMPLEMENT}\n')

if __name__ == "__main__":
  generate()
  print("Generated:", relpath(TARGET_EXT, ROOT_PATH))
  print("Generated:", relpath(TARGET_IMP, ROOT_PATH))
