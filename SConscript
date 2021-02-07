Import('env')
import os

env.PROJECT_NAME = "MiniScript"
env.RUN_TARGET = os.path.join(env['variant_dir'], 'bin/miniscript')

## MiniScript source files
SOURCES = [
	Glob('src/*.c'),
	Glob('src/types/*.c'),
	Glob('src/types/gen/*.c'),	
]

CPPPATH = [
	'include/',
]

## Compile miniscript lib.
vm = env.Library(
	target  = 'bin/miniscript',
	source  = SOURCES,
	CPPPATH = CPPPATH,
)

## Test executable
test = env.Program(
	target  = 'bin/miniscript',
	source  = ['test/main.c'],
	CPPPATH = CPPPATH,
	LIBPATH = 'bin',
	LIBS    = 'miniscript',
)

env.Append(CPPPATH=CPPPATH)

Requires(test, vm)


