import subprocess, os, sys
import json, re
from os.path import join

## FIXME: this file needs to be refactored.

## All benchmark files. ## TODO: pass args for iterations.
benchmarks = {
  "factors"      : ['.pk', '.py', '.rb', '.js', '.wren'],
  "fib"          : ['.pk', '.py', '.rb', '.js', '.wren'],
  "list"         : ['.pk', '.py', '.rb', '.js'],
  "loop"         : ['.pk', '.py', '.rb', '.js', '.wren'],
  "primes"       : ['.pk', '.py', '.rb', '.js', '.wren'],
}

def main():
  run_all_benchmarks()

def run_all_benchmarks():
  print_title("BENCHMARKS")

  def get_interpreter(file):
    if file.endswith('.pk'  ) : return 'pocket'
    if file.endswith('.py'  ) : return 'python'
    if file.endswith('.rb'  ) : return 'ruby'
    if file.endswith('.wren') : return 'wren'
    if file.endswith('.js'  ) : return 'node'
    assert False

  for bm_name in benchmarks:
    print(bm_name + ":")
    for ext in benchmarks[bm_name]:
      file = join(bm_name, bm_name + ext)
      interpreter = get_interpreter(file)
      print('  %10s: '%interpreter, end=''); sys.stdout.flush()
      result = run_command([interpreter, file])
      time = re.findall(r'elapsed:\s*([0-9\.]+)\s*s',
                result.stdout.decode('utf8'),
                re.MULTILINE)
      assert len(time) == 1, r'elapsed:\s*([0-9\.]+)\s*s --> no mach found.'
      print('%10ss'%time[0])

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
