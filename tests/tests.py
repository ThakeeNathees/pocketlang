import subprocess, os, sys
import json, re
from os.path import join

## TODO: Re write this in doctest (https://github.com/onqtam/doctest)

## All the test files.
test_files = [
  "lang/basics.pk",
  "lang/core.pk",
  "lang/controlflow.pk",
  "lang/fibers.pk",
  "lang/functions.pk",
  "lang/import.pk",

  "examples/brainfuck.pk",
  "examples/fib.pk",
  "examples/fizzbuzz.pk",
  "examples/helloworld.pk",
  "examples/pi.pk",
  "examples/prime.pk",

]

## All benchmark files. ## TODO: pass args for iterations.
benchmarks = {
  "factors"      : ['.pk', '.py', '.rb', '.wren'],
  "fib"          : ['.pk', '.py', '.rb', '.wren'],
  "list"         : ['.pk', '.py', '.rb'],
  "loop"         : ['.pk', '.py', '.rb', ".wren"],
  "primes"       : ['.pk', '.py', '.rb', ".wren"],
}

def main():
  run_all_tests()
  #run_all_benchmarks()

def run_all_benchmarks():
  print_title("BENCHMARKS")

  def get_interpreter(file):
    if file.endswith('.pk'  ) : return 'pocket'
    if file.endswith('.py'  ) : return 'python'
    if file.endswith('.rb'  ) : return 'ruby'
    if file.endswith('.wren') : return 'wren'
    assert False

  for bm_name in benchmarks:
    print(bm_name + ":")
    for ext in benchmarks[bm_name]:
      file = join('benchmark', bm_name, bm_name + ext)
      interpreter = get_interpreter(file)
      print('  %10s: '%interpreter, end=''); sys.stdout.flush()
      result = run_command([interpreter, file])
      time = re.findall(r'elapsed:\s*([0-9\.]+)\s*s',
                result.stdout.decode('utf8'),
                re.MULTILINE)
      assert len(time) == 1, r'elapsed:\s*([0-9\.]+)\s*s --> no mach found.'
      print('%10ss'%time[0])

def run_all_tests():
  print_title("TESTS")
  
  FMT_PATH = "%-25s"
  INDENTATION = '      | '
  for path in test_files:
    print(FMT_PATH % path, end='')
    result = run_command(['pocket', path])
    if result.returncode != 0:
      print('-- Failed')
      err = INDENTATION + result.stderr \
          .decode('utf8')               \
          .replace('\n', '\n' + INDENTATION)
      print(err)
    else:
      print('-- OK')


def run_command(command):
  return subprocess.run(command,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)

def print_title(title):
  print("--------------------------------")
  print(" %s " % title)
  print("--------------------------------")
  
if __name__ == '__main__':
  main()
