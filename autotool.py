import sys, os

def log(*msg):
	print("[autotool.py]", end='')
	for _msg in msg:
		print(' ' + _msg, end='')
	print()
	
def generate_files():
	log("generating types/buffer source files")
	sys.path.insert(1, 'src/types/')
	import buffergen
	ec = buffergen.gen()
	
	if sys.platform == 'win32':
		with open('src/types/gen.bat', 'w') as f:
			f.write('python buffergen.py')
		with open('src/types/clean.bat', 'w') as f:
			f.write('python buffergen.py --clean')
	log("buffer source files generated")
	
	return ec
	
if __name__ == '__main__':
	generate_files()