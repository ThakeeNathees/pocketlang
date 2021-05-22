import subprocess
import json, os

files = [
  "lang/basics.pk",
  "lang/functions.pk",
  "lang/if.pk",
  "examples/fib.pk",
  "examples/prime.pk",
]

FMT_PATH = "%-25s"
INDENTATION = '      | '


def run_all_tests():
	for path in files:
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

