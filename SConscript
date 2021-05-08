Import('env')
import os

env.PROJECT_NAME = "pocketlang"
env.RUN_TARGET = os.path.join(env['variant_dir'], 'bin/pocket')

## TODO: automate types file generation.

## PocketLang source files
SOURCES = [
	Glob('src/*.c'),
	Glob('src/types/gen/*.c'),	
]

if env['lib_shared']:
	## Compile pocketlang dynamic lib.
	dll = env.SharedLibrary(
		target     = 'bin/pocket' + env['bin_suffix'],
		source     = SOURCES,
		CPPPATH    = ['include/'],
		CPPDEFINES = [env['CPPDEFINES'], 'MS_DLL', 'MS_COMPILE'],
	)
else:
	## Compile pocketlang static lib.
	lib = env.Library(
		target  = 'bin/pocket' + env['bin_suffix'],
		source  = SOURCES,
		CPPPATH = ['include/'],
	)

	## Test executable
	test = env.Program(
		target  = 'bin/pocket' + env['bin_suffix'],
		source  = ['src/main/main.c'],
		CPPPATH = ['include/'],
		LIBPATH = 'bin',
		LIBS    = 'pocket' + env['bin_suffix'],
	)
	
	env.Append(CPPPATH=['include/'])
	
	Requires(test, lib)


