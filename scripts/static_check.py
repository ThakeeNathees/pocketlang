#!python
## Copyright (c) 2020-2021 Thakee Nathees
## Copyright (c) 2021-2022 Pocketlang Contributors
## Distributed Under The MIT License

## This will run static checks on the source files, for line length,
## uses of tabs, and trailing white spaces, etc.

import os, sys, re
from os import listdir
from os.path import *

## Pocket lang root directory. All the listed paths bellow are relative to
## the root path.
ROOT_PATH = abspath(join(dirname(__file__), ".."))

## A list of source files, to check if the fnv1a hash values match it's
## corresponding cstring in the CASE_ATTRIB(name, hash) macro calls.
HASH_CHECK_LIST = [
  "src/core/core.c",
  "src/core/value.c",
]

## A list of extension to perform static checks, of all the files in the
## SOURCE_DIRS.
CHECK_EXTENTIONS = ('.c', '.h', '.py', '.pk', '.js')

## A list of strings, if a line contains it we allow it to be longer than
## 79 characters, It's not "the correct way" but it works.
ALLOW_LONG_LINES = ('http://', 'https://', '<script ', '<link ', '<svg ')

## A list of files that are ignored for static check. Usually third party
## files and generated source files.
IGNORE_FILES = (
  "cli/modules/pknative.gen.c", ## FIXME: set gen path.
  "cli/argparse.h",             ## FIXME: collect all thirdparty files.  
  ## Contain longer lines. I shoule add something like static-check-off,
  ## static-check-on like clang or just use clang.
  "src/libs/std_term.c",
  ## Contain extendedted ascii characters that raise UnicodeDecodeError.
  "src/libs/std_term.pk",
)

## A list of directory, contains C source files to perform static checks.
## This will include all files with extension from CHECK_EXTENTIONS.
SOURCE_DIRS = [
  "src/core/",
  "src/libs/",
  "cli/",

  "docs/",
  "docs/wasm/",
]

## This global variable will be set to true if any check failed.
checks_failed = False

## Converts a list of relative paths from the working directory
## to a list of relative paths from this file's absolute directory.
def to_abs_paths(sources):
  return map(lambda s: os.path.join(ROOT_PATH, s), sources)

## Converts the path from absolute path to relative path from the
## toplelve of the project.
def to_rel_path(path):
  return relpath(path, ROOT_PATH)
  
def main():
  check_fnv1_hash(to_abs_paths(HASH_CHECK_LIST))
  check_static(to_abs_paths(SOURCE_DIRS))
  if checks_failed:
    sys.exit(1)
  print("Static checks were passed.")

def check_fnv1_hash(sources):
  PATTERN = r'CHECK_HASH\(\s*"([A-Za-z0-9_]+)"\s*,\s*(0x[0-9abcdef]+)\)'
  for file in sources:
    fp = open(file, 'r')
    
    line_no = 0
    for line in fp.readlines():
      line_no += 1
      match = re.findall(PATTERN, line)
      if len(match) == 0: continue
      name, val = match[0]
      hash = hex(fnv1a_hash(name))
      
      if val == hash: continue
      
      ## Path of the file relative to top-level.
      file_path = to_rel_path(file)
      report_error(f"{location(file_path, line_no)} - hash mismatch. "
        f"hash('{name}') = {hash} not {val}")
        
    fp.close()

## Check each source file ('.c', '.h', '.py') in the [dirs] contains tabs,
## more than 79 characters, and trailing white space.
def check_static(dirs):
  for dir in dirs:
    
    for file in listdir(dir):
      if not file.endswith(CHECK_EXTENTIONS): continue
      if os.path.isdir(join(dir, file)): continue
      
      curr_file = normpath(join(dir, file))

      skip = False
      for ignore in IGNORE_FILES:
        if curr_file == normpath(join(ROOT_PATH, ignore)):
          skip = True; break;
      if skip: continue

      fp = open(curr_file, 'r')
      
      ## Path of the file relative to top-level.
      file_path = to_rel_path(join(dir, file))
      lines = list(fp.readlines())
      fp.close()

      if len(lines) > 0 and not lines[-1].endswith('\n'):
        report_error(f"{file_path} - file isn't ending with new line.")

      ## This will be set to true if the last line is empty.
      is_last_empty = False; line_no = 0
      for line in lines:
        line_no += 1; line = line[:-1] # remove the line ending.
        
        ## Check if the line contains any tabs.
        if '\t' in line:
          _location = location(file_path, line_no)
          report_error(f"{_location} - contains tab(s) ({repr(line)}).")
            
        if len(line) >= 80:
          skip = False
          for ignore in ALLOW_LONG_LINES:
            if ignore in line:
              skip = True
              break
          if skip: continue
          
          _location = location(file_path, line_no)
          report_error(
            f"{_location} - contains {len(line)} (> 79) characters.")
            
        if line.endswith(' '):
          _location = location(file_path, line_no)
          report_error(f"{_location} - contains trailing white space.")
            
        if line == '':
          if is_last_empty:
            _location = location(file_path, line_no)
            report_error(f"{_location} - consequent empty lines.")
          is_last_empty = True
        else:
          is_last_empty = False

## Returns a formated string of the error location.
def location(file, line):
  return f"{'%-17s'%file} : {'%4s'%line}"

## FNV-1a hash. See: http://www.isthe.com/chongo/tech/comp/fnv/
def fnv1a_hash(string):
  FNV_prime_32_bit = 16777619
  FNV_offset_basis_32_bit = 2166136261
  
  hash = FNV_offset_basis_32_bit
  
  for c in string:
    hash ^= ord(c)
    hash *= FNV_prime_32_bit
    hash &= 0xffffffff ## intentional 32 bit overflow.
  return hash

def report_error(msg):
  global checks_failed
  checks_failed = True
  print(msg, file=sys.stderr)

if __name__ == '__main__':
  main()
