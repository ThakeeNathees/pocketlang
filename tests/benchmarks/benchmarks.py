#!python
## Copyright (c) 2020-2021 Thakee Nathees
## Distributed Under The MIT License

import subprocess, re, os, sys, platform
from os.path import join, abspath, dirname, relpath
from shutil import which

## The absolute path of this file, when run as a script.
## This file is not intended to be included in other files at the moment.
THIS_PATH = abspath(dirname(__file__))

## Map from systems to the relative pocket binary path.
SYSTEM_TO_BINARY_PATH = {
  "Windows": "..\\..\\build\\release\\bin\\pocket.exe",
  "Linux"  : "../../build/release/pocket",
  "Darwin" : "../../build/release/pocket",
}

## A list of benchmark directories, relative to THIS_PATH
BENCHMARKS = (
  "factors",
  "fib",
  "list",
  "loop",
  "primes",
)

## Map from file extension to it's interpreter, Will be updated.
INTERPRETERS = {
  '.pk'   : None,
  '.wren' : None,
  '.rb'   : None,
  '.rb'   : None,
}

def main():
  update_interpreters()
  run_all_benchmarsk()

## ----------------------------------------------------------------------------
## RUN ALL BENCHMARKS
## ----------------------------------------------------------------------------

def run_all_benchmarsk():
  for benchmark in BENCHMARKS:
    print_title(benchmark)
    dir = join(THIS_PATH, benchmark)
    for file in os.listdir(dir):
      file = abspath(join(dir, file))
      
      ext = get_ext(file) ## File extension.
      if ext not in INTERPRETERS: continue
      lang, interp = INTERPRETERS[ext]
      if not interp: continue
      
      print("%-10s : "%lang, end=''); sys.stdout.flush()
      result = _run_command([interp, file])
      time = re.findall(r'elapsed:\s*([0-9\.]+)\s*s',
                result.stdout.decode('utf8'),
                re.MULTILINE)

      if len(time) != 1:
        print() # Skip the line.
        error_exit(r'elapsed:\s*([0-9\.]+)\s*s --> no mach found.')
      print('%.6fs'%float(time[0]))
  pass

def _run_command(command):
  return subprocess.run(command,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)

## ----------------------------------------------------------------------------
## UPDATE INTERPRETERS
## ----------------------------------------------------------------------------

def update_interpreters():
  pocket = _get_pocket_binary()
  python = 'python' if platform.system() == 'Windows' else 'python3'
  print_title("CHECKING FOR INTERPRETERS")
  global INTERPRETERS
  INTERPRETERS['.pk']   = _find_interp('pocketlang', pocket)
  INTERPRETERS['.wren'] = _find_interp('wren',       'wren')
  INTERPRETERS['.rb']   = _find_interp('ruby',       'ruby')
  INTERPRETERS['.py']   = _find_interp('python',     python)
  INTERPRETERS['.js']   = _find_interp('javascript', 'node')  

## This will return the path of the pocket binary (on different platforms).
## The debug version of it for enabling the assertions.
def _get_pocket_binary():
  system = platform.system()
  if system not in SYSTEM_TO_BINARY_PATH:
    error_exit("Unsupported platform %s" % system)

  pocket = abspath(join(THIS_PATH, SYSTEM_TO_BINARY_PATH[system]))
  if not os.path.exists(pocket):
    error_exit("Pocket interpreter not found at: '%s'" % pocket)

  return pocket

## Find and return the interpreter from the path.
## as (lang, interp) tuple.
def _find_interp(lang, interpreter):
  print('%-25s' % ('Searching for %s ' % lang), end='')
  sys.stdout.flush()
  if which(interpreter):
    print_success('-- found')
    return (lang, interpreter)
  print_warning('-- not found (skipped)')
  return (lang, None)

## Return the extension from the file name.
def get_ext(file_name):
  period = file_name.rfind('.'); assert period > 0
  return file_name[period:]

## ----------------------------------------------------------------------------
## PRINT FUNCTIONS
## ----------------------------------------------------------------------------

def print_title(title):
  print("----------------------------------")
  print(" %s " % title)
  print("----------------------------------")

## ANSI color codes to print messages.
COLORS = {
  'GREEN'     : '\u001b[32m',
  'YELLOW'    : '\033[93m',
  'RED'       : '\u001b[31m',
  'UNDERLINE' : '\033[4m' ,
  'END'       : '\033[0m' ,
}

## Prints a warning message to stdour.
def print_warning(msg):
  os.system('') ## This will enable ANSI codes in windows terminal.
  for line in msg.splitlines():
    print(COLORS['YELLOW'] + line + COLORS['END'])

## print success message to stdout.
def print_success(msg):
  os.system('') ## This will enable ANSI codes in windows terminal.
  for line in msg.splitlines():
    print(COLORS['GREEN'] + line + COLORS['END'])

## prints an error message to stderr and exit
## immediately.
def error_exit(msg):
  os.system('') ## This will enable ANSI codes in windows terminal.
  print(COLORS['RED'] + 'Error: ' + msg + COLORS['END'], end='')
  sys.exit(1)
  
if __name__ == '__main__':
  main()
