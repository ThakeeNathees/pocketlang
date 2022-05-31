#!python
## Copyright (c) 2020-2021 Thakee Nathees
## Copyright (c) 2021-2022 Pocketlang Contributors
## Distributed Under The MIT License

import os, sys, platform
import subprocess, json, re
from os.path import (join, abspath, dirname,
                     relpath, exists)

## TODO: Re-write this in doctest (https://github.com/onqtam/doctest)

## Pocket lang root directory.
ROOT_PATH = abspath(join(dirname(__file__), ".."))

## Output debug cli executable path relative to root.
POCKET_BINARY = "build/Debug/bin/pocket"

## All the test files, relative to root/tests/ directory.
TEST_SUITE = {
  "Unit Tests": (
    "lang/basics.pk",
    "lang/builtin_fn.pk",
    "lang/builtin_ty.pk",
    "lang/class.pk",
    "lang/closure.pk",
    "lang/controlflow.pk",
    "lang/fibers.pk",
    "lang/functions.pk",
    "lang/import.pk",
  ),
  
  "Modules Test" : (
    "modules/dummy.pk",
    "modules/math.pk",
    "modules/io.pk",
    "modules/json.pk",
  ),

  "Random Scripts" : (
    "random/linked_list.pk",
    "random/lisp_eval.pk",
    "random/string_algo.pk",
  ),

  "Examples": (
    "examples/brainfuck.pk",
    "examples/fib.pk",
    "examples/fizzbuzz.pk",
    "examples/helloworld.pk",
    "examples/matrix.pk",
    "examples/pi.pk",
    "examples/prime.pk",
  ),
}

## This global variable will be set to true if any test failed.
tests_failed = False

def main():
  run_all_tests()
  if tests_failed:
    sys.exit(1)

def run_all_tests():
  ## get the interpreter.
  pocket = get_pocket_binary()
  
  for suite in TEST_SUITE:
    print_title(suite)
    for test in TEST_SUITE[suite]:
      path = join(ROOT_PATH, "tests", test)
      run_test_file(pocket, test, path)

def run_test_file(pocket, test, path):
  FMT_PATH = "%-25s"
  INDENTATION = '  | '
  print(FMT_PATH % test, end='')

  sys.stdout.flush()
  result = run_command([pocket, path])
  if result.returncode != 0:
    print_error('-- Failed')
    err = INDENTATION + result.stderr \
        .decode('utf8')               \
        .replace('\n', '\n' + INDENTATION)
    print_error(err)
  else:
    print_success('-- PASSED')

## This will return the path of the pocket binary (on different platforms).
## The debug version of it for enabling the assertions.
def get_pocket_binary():
  system = platform.system()
  if system not in ("Windows", "Linux", "Darwin"):
    error_exit("Unsupported platform")
  binary = join(ROOT_PATH, POCKET_BINARY)
  if system == "Windows": binary += ".exe"
  if not exists(binary):
    error_exit(f"Pocket interpreter not found at: '{binary}'")
  return binary

def run_command(command):
  return subprocess.run(command,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)

## ----------------------------------------------------------------------------

## ANSI color codes to print messages.
COLORS = {
    'GREEN'     : '\u001b[32m',
    'YELLOW'    : '\033[93m',
    'RED'       : '\u001b[31m',
    'UNDERLINE' : '\033[4m' ,
    'END'       : '\033[0m' ,
}

## All tests messages are written to stdout because they're mixed
## and github actions io calls seems asynchronous. And ANSI colors
## not seems to working with multiline in github actions, so
## printing the message line by line.

## prints an error to stderr and continue tests.
def print_error(msg):
  global tests_failed
  tests_failed = True
  for line in msg.splitlines():
    print(COLORS['RED'] + line + COLORS['END'])

## print success message to stdout.
def print_success(msg):
  for line in msg.splitlines():
    print(COLORS['GREEN'] + line + COLORS['END'])

## prints an error message to stderr and exit immediately.
def error_exit(msg):
  print("Error:", msg, file=sys.stderr)
  sys.exit(1)

def print_title(title):
  print("----------------------------------")
  print(" %s " % title)
  print("----------------------------------")
  
if __name__ == '__main__':
  ## This will enable ANSI codes in windows terminal.
  os.system('')
  main()
