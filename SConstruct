#!python
import os, subprocess, sys

opts = Variables([], ARGUMENTS)
## Define our options
opts.Add(EnumVariable('platform', "Compilation platform", '', ['', 'windows', 'x11', 'linux', 'osx']))
opts.Add(EnumVariable('target', "Compilation target", 'debug', ['debug', 'release']))
opts.Add(EnumVariable('bits', 'output program bits', '64', ['32', '64']))
opts.Add(BoolVariable('use_llvm', "Use the LLVM / Clang compiler", False))
opts.Add(BoolVariable('use_mingw', "Use Mingw compiler", False))

opts.Add(BoolVariable('vsproj', "make a visual studio project", False))
opts.Add(BoolVariable('verbose', "use verbose build command", True))

opts.Add(BoolVariable('lib_shared', "Compile as a shared library (only).", False))

## Setup the Environment
DefaultEnvironment(tools=[]) ## not using any tools
env = Environment()

## Updates the environment with the option variables.
opts.Update(env)

if env['use_llvm']:
	env['CC'] = 'clang'
	env['CXX'] = 'clang++'
elif env['use_mingw']:
	env['tools'] = ['mingw']

## find platform
if env['platform'] == '':
	if sys.platform == 'win32':
		env['platform'] = 'windows'
	elif sys.platform in ('x11', 'linux', 'linux2'):
		env['platform'] = 'linux'
	elif sys.platform == 'darwin':
		env['platform'] = 'osx'
	else:
		print("platform(%s) not supported." % sys.platform)
		quit()

## For the reference:
## - CCFLAGS are compilation flags shared between C and C++
## - CFLAGS are for C-specific compilation flags
## - CXXFLAGS are for C++-specific compilation flags
## - CPPFLAGS are for pre-processor flags
## - CPPDEFINES are for pre-processor defines
## - LINKFLAGS are for linking flags

## Check our platform specifics
if env['platform'] == "osx":
	env.Append(CXXFLAGS=['-std=c++17'])
	if env['target'] == 'debug':
		env.Append(CCFLAGS=['-g', '-O2', '-arch', 'x86_64'])
		env.Append(LINKFLAGS=['-arch', 'x86_64'])
	else:
		env.Append(CCFLAGS=['-g', '-O3', '-arch', 'x86_64'])
		env.Append(LINKFLAGS=['-arch', 'x86_64'])

elif env['platform'] == 'x11':
	env.Append(LIBS=['dl', 'pthread']) 
	env.Append(CXXFLAGS=['-std=c++17'])
	if env['target'] == 'debug':
		env.Append(CCFLAGS=['-fPIC', '-g3', '-Og'])
	else:
		env.Append(CCFLAGS=['-fPIC', '-g', '-O3'])

elif env['platform'] == "windows":
	env.Append(CXXFLAGS=['/std:c++17', '/bigobj'])
	env.Append(CPPDEFINES=['_CRT_SECURE_NO_WARNINGS'])
	env.Append(CPPDEFINES=['WIN32', '_WIN32', '_WINDOWS'])
	env.Append(CCFLAGS=['-W3', '-GR', '/FS'])
	env.Append(LINKFLAGS='-SUBSYSTEM:CONSOLE')
	env.Append(LIBS=[])

	if env['bits'] == '32': env['TARGET_ARCH'] = 'x86'
	else:                   env['TARGET_ARCH'] = 'x86_64'

	if env['target'] == 'debug':
		env.Append(CPPDEFINES=['DEBUG'])
		env.Append(CCFLAGS=['-EHsc', '-MDd', '-ZI'])
		env.Append(LINKFLAGS=['-DEBUG'])
	else:
		env.Append(CPPDEFINES=['NDEBUG'])
		env.Append(CCFLAGS=['-O2', '-EHsc', '-MD'])

## --------------------------------------------------------------------------------

## no_verbose function is from : https://github.com/godotengine/godot/blob/master/methods.py
def no_verbose(sys, env):
	colors = {}
	# Colors are disabled in non-TTY environments such as pipes. This means
	# that if output is redirected to a file, it will not contain color codes

	colors["cyan"] = "\033[96m"   if sys.stdout.isatty() else ""
	colors["purple"] = "\033[95m" if sys.stdout.isatty() else ""
	colors["blue"] = "\033[94m"	  if sys.stdout.isatty() else ""
	colors["green"] = "\033[92m"  if sys.stdout.isatty() else ""
	colors["yellow"] = "\033[93m" if sys.stdout.isatty() else ""
	colors["red"] = "\033[91m"	  if sys.stdout.isatty() else ""
	colors["end"] = "\033[0m"	  if sys.stdout.isatty() else ""

	compile_source_message = "{}Compiling {}==> {}$SOURCE{}".format(
		colors["blue"], colors["purple"], colors["yellow"], colors["end"]
	)
	java_compile_source_message = "{}Compiling {}==> {}$SOURCE{}".format(
		colors["blue"], colors["purple"], colors["yellow"], colors["end"]
	)
	compile_shared_source_message = "{}Compiling shared {}==> {}$SOURCE{}".format(
		colors["blue"], colors["purple"], colors["yellow"], colors["end"]
	)
	link_program_message = "{}Linking Program {}==> {}$TARGET{}".format(
		colors["red"], colors["purple"], colors["yellow"], colors["end"]
	)
	link_library_message = "{}Linking Static Library {}==> {}$TARGET{}".format(
		colors["red"], colors["purple"], colors["yellow"], colors["end"]
	)
	ranlib_library_message = "{}Ranlib Library {}==> {}$TARGET{}".format(
		colors["red"], colors["purple"], colors["yellow"], colors["end"]
	)
	link_shared_library_message = "{}Linking Shared Library {}==> {}$TARGET{}".format(
		colors["red"], colors["purple"], colors["yellow"], colors["end"]
	)
	java_library_message = "{}Creating Java Archive {}==> {}$TARGET{}".format(
		colors["red"], colors["purple"], colors["yellow"], colors["end"]
	)
	env.Append(CXXCOMSTR=[compile_source_message])
	env.Append(CCCOMSTR=[compile_source_message])
	env.Append(SHCCCOMSTR=[compile_shared_source_message])
	env.Append(SHCXXCOMSTR=[compile_shared_source_message])
	env.Append(ARCOMSTR=[link_library_message])
	env.Append(RANLIBCOMSTR=[ranlib_library_message])
	env.Append(SHLINKCOMSTR=[link_shared_library_message])
	env.Append(LINKCOMSTR=[link_program_message])
	env.Append(JARCOMSTR=[java_library_message])
	env.Append(JAVACCOMSTR=[java_compile_source_message])

if not env['verbose']:
	no_verbose(sys, env)
	
Export('env')
env['variant_dir'] = 'build/' + env['target'] + '/'
env['bin_suffix'] = '' ## TODO: Maybe '.%s.%s' % (env['platform'], env['bits'])
SConscript('SConscript', variant_dir=env['variant_dir'], duplicate=0)

## --------------------------------------------------------------------------------

## visual studio targets
def get_vsproj_context():
	targets = []
	variants = [] ## ["debug|Win32", "debug|x64", "release|Win32", "release|x64"]
	for target in 'debug', 'release':
		for bits in '32', '64':
			variants.append(target+'|'+('Win32' if bits=='32' else 'x64'))
			targets.append(env.RUN_TARGET)
	return variants, targets


def recursive_collect(dir, suffix):
	ret = []
	for file in os.listdir(dir):
		file = os.path.join(dir, file)
		if os.path.isfile(file):
			for suff in suffix:
				if (file.endswith(suff)):
					ret.append('$(ProjectDir)' + os.path.relpath(file))
		elif os.path.isdir(file):
			ret += recursive_collect(file, suffix)
	return ret

def msvs_collect_header():
	return recursive_collect('.', ('.h', '.hpp'))

def msvc_collect_sources():
	return recursive_collect('.', ('.c', '.cpp', '.cc', '.cxx'))

def msvc_build_commandline(commands):
	common_build_prefix = [
		'cmd /V /C set "bits=64"',
		'(if "$(PlatformTarget)"=="x86" (set "bits=32"))',
	]
	return " ^& ".join(common_build_prefix + [commands])


if env['vsproj']:
	env["MSVSBUILDCOM"] = msvc_build_commandline(
		"scons --directory=\"$(ProjectDir.TrimEnd('\\'))\" platform=windows target=$(Configuration) bits=!bits!"
	)
	env["MSVSREBUILDCOM"] = msvc_build_commandline(
		"scons --directory=\"$(ProjectDir.TrimEnd('\\'))\" platform=windows target=$(Configuration) bits=!bits! vsproj=yes"
	)
	env["MSVSCLEANCOM"] = msvc_build_commandline(
		"scons --directory=\"$(ProjectDir.TrimEnd('\\'))\" --clean platform=windows bits=!bits! target=$(Configuration)"
	)
	variants, targets = get_vsproj_context()
	env.MSVSProject(target = env.PROJECT_NAME + env['MSVSPROJECTSUFFIX'],
		srcs = msvc_collect_sources(),
		incs = msvs_collect_header(),
		variant = variants,
		runfile = targets,
		buildtarget = targets,
	)


## Generates help for the -h scons option.
Help(opts.GenerateHelpText(env))