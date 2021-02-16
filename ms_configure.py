import sys, os

## A simple version of the build script.
MINI_BUILD_SCRIPT = '''\
import os

cc = 'gcc' ## '"tcc\\tcc"'

sources = [
	'src/types/name_table.c',
	'test/main.c',
]

def add_sources(path):
	for file in os.listdir(path):
		if file.endswith('.c'):
			sources.append(
				path + '/' + file if path != '.' else file)
add_sources('src/')
add_sources('src/types/gen')

cmd = '%s -Wno-int-to-pointer-cast -o miniscript -Iinclude %s' % (cc, ' '.join(sources))
print(cmd)
os.system(cmd)

'''

def main():
	generate_files()

def log(*msg):
	print("[ms:configure.py]", end='')
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
	
	with open('build.py', 'w') as f:
		f.write(MINI_BUILD_SCRIPT)
		log('build.py generated')
		
	log("buffer source files generated")
	
	return ec
	
if __name__ == '__main__':
	main()