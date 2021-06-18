#!python
## Copyright (c) 2020-2021 Thakee Nathees
## Distributed Under The MIT License

import os, sys, platform
import subprocess, json, re
from os.path import join, abspath, relpath

## TODO: Re write this in doctest (https://github.com/onqtam/doctest)

## The absolute path of this file, when run as a script.
## This file is not intended to be included in other files at the moment.
THIS_PATH = abspath(os.path.dirname(__file__))

## All the test files.
TEST_SUITE = {
  "Unit Tests": (
    "lang/basics.pk",
    "lang/core.pk",
    "lang/controlflow.pk",
    "lang/fibers.pk",
    "lang/functions.pk",
    "lang/import.pk",
  ),

  "Examples": (
    "examples/brainfuck.pk",
    "examples/fib.pk",
    "examples/fizzbuzz.pk",
    "examples/helloworld.pk",
    "examples/pi.pk",
    "examples/prime.pk",
  ),
}

## Map from systems to the relative binary path
SYSTEM_TO_BINARY_PATH = {
  "Windows": "..\\build\\debug\\bin\\pocket.exe",
  "Linux": "../build/debug/pocket",
  "Darwin": "../build/debug/pocket",
}

## This global variable will be set to true if any test failed.
tests_failed = False

def main():

  ## This will enable ANSI codes in windows terminal.
  os.system('')
  
  run_all_tests()
  if tests_failed:
    sys.exit(1)

def run_all_tests():
  ## get the interpreter.
  pocket = get_pocket_binary()
  
  for suite in TEST_SUITE:
    print_title(suite)
    for test in TEST_SUITE[suite]:
      path = join(THIS_PATH, test)
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
  if system not in SYSTEM_TO_BINARY_PATH:
    error_exit("Unsupported platform %s" % system)

  pocket = abspath(join(THIS_PATH, SYSTEM_TO_BINARY_PATH[system]))
  if not os.path.exists(pocket):
    error_exit("Pocket interpreter not found at: '%s'" % pocket)

  return pocket

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

## All tests messages are written to stdour because they're mixed
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

## prints an error message to stderr and exit
## immediately.
def error_exit(msg):
  print("Error:", msg, file=sys.stderr)
  sys.exit(1)

def print_title(title):
  print("----------------------------------")
  print(" %s " % title)
  print("----------------------------------")
  
if __name__ == '__main__':
  main()
