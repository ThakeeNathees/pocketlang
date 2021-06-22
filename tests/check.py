#!python
## Copyright (c) 2020-2021 Thakee Nathees
## Distributed Under The MIT License

## This will run a static checks on the source files, for line length,
## uses of tabs and trailing white spaces, etc.

import os, sys, re
from os import listdir
from os.path import (
  join, abspath, dirname, relpath)

## The absolute path of this file, when run as a script.
## This file is not intended to be included in other files at the moment.
THIS_PATH = abspath(dirname(__file__))

## Converts a list of relative paths from the working directory
## to a list of relative paths from this file's absolute directory.
def to_abs_paths(sources):
  return map(lambda s: os.path.join(THIS_PATH, s), sources)

## Converts the path from absolute path to relative path from the
## toplelve of the project.
def to_tolevel_path(path):
  return relpath(path, join(THIS_PATH, '..'))

## A list of source files, to check if the fnv1a hash values match it's
## corresponding cstring in the CASE_ATTRIB(name, hash) macro calls.
HASH_CHECK_LIST = [
  "../src/pk_core.c",
]

## A list of directory, contains C source files to perform static checks.
## This will include '.c', '.h', '.py' and '.pk' files.
C_SOURCE_DIRS = [
  "../src/",
  "../cli/",
]

## A list of common header that just copied in different projects.
## These common header cannot be re-used because we're trying to achieve
## minimalistic with the count of the sources in pocketlang.
COMMON_HEADERS = [
  "../src/pk_common.h",
  "../cli/common.h",
]

## This global variable will be set to true if any check failed.
checks_failed = False

def main():
  check_fnv1_hash(to_abs_paths(HASH_CHECK_LIST))
  check_static(to_abs_paths(C_SOURCE_DIRS))
  check_common_header_match(to_abs_paths(COMMON_HEADERS))
  if checks_failed:
    sys.exit(1)
  print("Static checks were passed.")

def check_fnv1_hash(sources):
  PATTERN = r'CASE_ATTRIB\(\s*"([A-Za-z0-9_]+)"\s*,\s*(0x[0-9abcdef]+)\)'
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
      file_path = to_tolevel_path(file)
      report_error(f"{location(file_path, line_no)} - hash mismatch. "
        f"hash('{name}') = {hash} not {val}")
        
    fp.close()

## Check each source file ('.c', '.h', '.py') in the [dirs] contains tabs,
## more than 79 characters and trailing white space.
def check_static(dirs):
  valid_ext = ('.c', '.h', '.py', '.pk')
  for dir in dirs:
    
    for file in listdir(dir):
      if not file.endswith(valid_ext): continue
      if os.path.isdir(join(dir, file)): continue
      
      fp = open(join(dir, file), 'r')
      
      ## Path of the file relative to top-level.
      file_path = to_tolevel_path(join(dir, file))

      ## This will be set to true if the last line is empty.
      is_last_empty = False; line_no = 0
      for line in fp.readlines():
        line_no += 1; line = line[:-1] # remove the line ending.
        
        ## Check if the line contains any tabs.
        if '\t' in line:
          _location = location(file_path, line_no)
          report_error(f"{_location} - contains tab(s) ({repr(line)}).")
            
        if len(line) >= 80:
          if 'http://' in line or 'https://' in line: continue
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
        
      fp.close()

## Assert all the content of the headers list below are the same.
def check_common_header_match(headers):
  headers = list(headers)
  assert len(headers) >= 1
  
  content = ''
  with open(headers[0], 'r') as f:
    content = f.read()
  
  for i in range(1, len(headers)):
    with open(headers[i], 'r') as f:
      if f.read() != content:
        main_header = to_tolevel_path(headers[0])
        curr_header = to_tolevel_path(headers[i])
        report_error("File content mismatch: \"%s\" and \"%s\"\n"
                     "    These files contants should be the same."
                     %(main_header, curr_header))

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
