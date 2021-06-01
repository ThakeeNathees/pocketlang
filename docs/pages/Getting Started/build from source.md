
# Build From Source

## Without a build script

It can be build from source easily without any depencency, or additional
requirenments except for a c99 compatible compiler. And build systems are
optional. It can be compiled with the following command.

Using gcc
```
gcc -o pocket cli/*.c src/*.c -Isrc/include -lm -Wno-int-to-pointer-cast
```

Using MSVC
```
cl /Fepocket cli/*.c src/*.c /Isrc/include && rm *.obj
```

For other compiler/IDE

1. Create an empty project file / makefile.
2. Add all C files in the src directory.
3. Add all C files in the cli directory (**not** recursively).
4. Add src/include to include path.
5. Compile.

If you weren't able to compile it, please report by [opening an issue](https://github.com/ThakeeNathees/pocketlang/issues/new)).

## Using a build script

You could find some of the build script for different build system in the `build/`
direcroty. If you don't find a scirpt for your prifered system, request on github
by [opening an issue](https://github.com/ThakeeNathees/pocketlang/issues/new) or
feel free to contribute one.

## Makefile

```
cd build && make
```

I haven't tested the makefile on different platforms except for "windows subsystem for linux".
And the makefile is still need to be imporved. If you find any problems with the script or
have any improvements, let us know in [github](https://github.com/ThakeeNathees/pocketlang).


## Batch script with MSVC

```
cd build && build
```

You don't need to run the script from a Visual Studio .NET Command Prompt,
It'll search for the MSVS installation path and use it to compile. But if it
can't, try running on VS command prompt.

## SCons

pocketlang mainly focused on [scons](https://www.scons.org/), and it's
recommended to use it for contributions since it has various configurations
and compiler support. It's a python based build system. You need to have
python 3.6+ installed in your development environment. To install scons run
`python3 -m pip install scons`. Make sure it's version is 3.0+ and for Visual
Studio 2019 it require v3.1.1+. In Linux if scons using python2 instead of 3
you'll have to edit `/usr/local/bin/scons` or `~/.local/bin/scons` to ensure
that it points to `/usr/bin/env python3` and not `python`

```
cd build && scons
```

You can specify the number of jobs scons to use to speed up the building process
using the -j flag (-j6, -j8). To generate Visual Studio project files add `vsproj=true`
argument when building. To compile using mingw in windows use `use_mingw=true` argument.
If your build failed feel free to [open an issue](https://github.com/ThakeeNathees/pocketlang/issues/new). 
