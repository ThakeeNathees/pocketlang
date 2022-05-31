##
## Copyright (c) 2020-2022 Thakee Nathees
## Copyright (c) 2021-2022 Pocketlang Contributors
## Distributed Under The MIT License
##

##
## USAGE: python amalgamate.py > pocketlang.h
##

import os, re, sys
from os.path import *

## Pocket lang root directory. All the listed paths bellow are relative to
## the root path.
ROOT_PATH = abspath(join(dirname(__file__), ".."))

## Filter all the files in the [path] with extension and return them.
def files(path, ext, filter_= lambda file : True):
  return [
    join(ROOT_PATH, path, file) for file in os.listdir(join(ROOT_PATH, path))
    if file.endswith(ext) and filter_(file)
  ]

SOURCES = [
  *files('src/core/', '.c'),
  *files('src/libs/', '.c'),
]

PUBLIC_HEADER = join(ROOT_PATH, 'src/include/pocketlang.h')

HEADERS = [
  *[ ## The order of the headers are important.
    join(ROOT_PATH, header)
    for header in [
      'src/core/common.h',
      'src/core/utils.h',
      'src/core/internal.h',
      'src/core/buffers.h',
      'src/core/value.h',
      'src/core/compiler.h',
      'src/core/core.h',
      'src/core/debug.h',
      'src/core/vm.h',      
      'src/libs/libs.h',
    ]
  ]
]

## return include path from a linclude "statement" line.
def _get_include_path(line):
  assert '#include ' in line
  index = line.find('#include ')
  start = line.find('"', index)
  end = line.find('"', start + 1)
  return line[start+1:end]

## Since the stdout is redirected to a file, stderr can be
## used for logging.
def log(*msg):
  message = ' '.join(map(lambda x: str(x), msg))
  sys.stderr.write(message + '\n')

def parse(path):
  dir = dirname(path).replace('\\', '/') ## Windows.
  text = ""  
  with open(path, 'r') as fp:
    for line in fp.readlines():
      if "//<< AMALG_IGNORE >>" in line:
        continue
      elif "//<< AMALG_INLINE >>" in line:
        path = join(dir, _get_include_path(line))
        path = path.replace('\\', '/')
        text += parse(path) + '\n'
      else: text += line
      
  ## Note that this comment stripping regex is dubious, and wont
  ## work on all cases ex:
  ##
  ##    const char* s = "//";\n
  ##
  ## The above '//' considered as comment but it's inside a string.
  text = re.sub('/\*.*?\*/', '', text, flags=re.S)
  text = re.sub('//.*?\n', '\n', text, flags=re.S)
  text = re.sub('[^\n\S]*\n', '\n', text, flags=re.S)
  text = re.sub('\n\n+', '\n\n', text, flags=re.S)
  return text

def generate():

  gen = ''
  with open(PUBLIC_HEADER, 'r') as fp:
    gen = fp.read()

  gen += '\n#define PK_AMALGAMATED\n'
  for header in HEADERS:
    gen += '// Amalgamated file: ' +\
            relpath(header, ROOT_PATH).replace("\\", "/") +\
            '\n'
    gen += parse(header) + '\n'

  gen += '#ifdef PK_IMPLEMENT\n\n'
  for source in SOURCES:
    gen += parse(source)
  gen += '#endif // PK_IMPLEMENT\n'

  return gen
  
if __name__ == '__main__':
  print(generate())


