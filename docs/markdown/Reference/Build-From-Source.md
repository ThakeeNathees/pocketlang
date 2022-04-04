
Pocketlang can be build from source easily without any dependencies, or
additional requirements except for a c99 compatible compiler. It can be
compiled with the following command.

## GCC / MinGw / Clang (alias with gcc)

```
gcc -o pocket cli/*.c src/*.c -Isrc/include -lm
```


## MSVC
```
cl /Fepocket cli/*.c src/*.c /Isrc/include && rm *.obj
```

## Makefile
```
make
```
To run the make file on windows with `mingw`, you require the GNU `make` tool which you can get
from [msys2](https://www.msys2.org/) or [cygwin](https://www.cygwin.com/).

## Windows batch script
```
build
```
You don't have to run the script from a Visual Studio .NET developer command prompt, It'll search
for the MSVS installation path and setup the build environment.

## For other compiler/IDE

1. Create an empty project file / makefile.
2. Add all C files in the src directory.
3. Add all C files in the cli directory (**NOT** recursively).
4. Add `src/include` to include path.
5. Compile.

If you weren't able to compile it, please report us by [opening an issue](https://github.com/ThakeeNathees/pocketlang/issues/new).
