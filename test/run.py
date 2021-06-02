import subprocess, os, sys
import json, re
from os.path import join

FMT_PATH = "%-25s"
INDENTATION = '      | '

test_files = [
  "lang/basics.pk",
  "lang/functions.pk",
  "lang/controlflow.pk",
  "examples/fib.pk",
  "examples/prime.pk",
]

benchmarks = {
	"factors"      : ['.pk', '.py', '.rb', '.wren'],
	"fib"          : ['.pk', '.py', '.rb', '.wren'],
	"list_reverse" : ['.pk', '.py', '.rb'],
	"loop"         : ['.pk', '.py', '.rb', ".wren"],
	"primes"       : ['.pk', '.py', '.rb', ".wren"],
}

def run_all_benchmarks():
	print("--------------------------------")
	print(" BENCHMARKS ")
	print("--------------------------------")
	
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
	print("--------------------------------")
	print(" TESTS ")
	print("--------------------------------")
	for path in test_files:
		run_file(path)
	
def run_file(path):
	print(FMT_PATH % path, end='')
	result = run_command(['pocket', path])
	if result.returncode != 0:
		print('-- Failed')
		err = INDENTATION + result.stderr \
		      .decode('utf8')             \
			  .replace('\n', '\n' + INDENTATION)
		print(err)
	else:
		print('-- OK')


def run_command(command):
	return subprocess.run(command,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)


run_all_tests()
run_all_benchmarks()
