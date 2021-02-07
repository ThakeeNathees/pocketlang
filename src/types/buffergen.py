from pathlib import Path ## python 3.4
import shutil
import os, sys

## usage buffergen.py [--clean]

SCRIPT_PATH = Path(os.path.realpath(__file__))
ROOT = str(SCRIPT_PATH.parent)

GEN_LIST = [
	## name       type
	('Int',       'int'),
	('Byte',      'uint8_t'),
	('Var',       'Var'),
	('String',    'String*'),
	('Function',  'Function*'),
]

def log(msg):
	print('[buffergen.py]', msg)

def gen():
	cwd = os.getcwd()
	os.chdir(ROOT)
	_gen()
	os.chdir(cwd)
	return 0

def clean():
	cwd = os.getcwd()
	os.chdir(ROOT)
	_clean()
	os.chdir(cwd)
	return 0

def _replace(text, _data):
	text = text.replace('$name$', _data[0])
	text = text.replace('$name_l$', _data[0].lower())
	text = text.replace('$name_u$', _data[0].upper())
	text = text.replace('$type$', _data[1])

	## Fix relative imports.
	text = text.replace('../vm.h', '../../vm.h')
	text = text.replace('../utils.h', '../../utils.h')
	text = text.replace('../common.h', '../../common.h')

	return text

def _gen():

	header = ''
	source = ''
	with open('buffer.template.h', 'r') as f:
		header = f.read()
	with open('buffer.template.c', 'r') as f:
		source = f.read()

	for _data in GEN_LIST:
		_header = header.replace('''\
// A place holder typedef to prevent IDE syntax errors. Remove this line 
// when generating the source.
typedef uint8_t $type$;
''', '')
		_header = _replace(_header, _data)

		_source = source.replace('''\
// Replace the following line with "$name$_buffer.h"
#include "buffer.template.h"''', '#include "%s_buffer.h"' % _data[0].lower())
		_source = _replace(_source, _data)

		if not os.path.exists('gen/'):
			os.mkdir('gen/')

		with open('gen/' + _data[0].lower() + '_buffer.h', 'w') as f:
			f.write(_header)
			log(_data[0].lower() + '_buffer.h' + ' generated' )
		with open('gen/' + _data[0].lower() + '_buffer.c', 'w') as f:
			f.write(_source)
			log(_data[0].lower() + '_buffer.c' + ' generated' )


def _clean():
	shutil.rmtree('gen/')
	log("Buffer source files removed")


def error_exit(msg):
	print("Error: %s\n\tusage buffergen.py [--clean]" % msg)
	exit(1)

if __name__ == '__main__':
	if len(sys.argv) > 2:
		error_exit("invalid arg count")
	if len(sys.argv) == 2:
		if sys.argv[1] == '--clean':
			_clean()
		else:
			error_exit("unknown argument")
	else:
		_gen()
	exit(0)