
This directory contains all the scripts that were used to build, test, measure
performance etc. pocketlang. These scripts in this directory are path
independent -- can be run from anywhere.

## Using premake

To download the premake **which is only 1.3 MB** stand alone binary, run the
bellow command (on windows it might be `python` and not `python3`) or you
can download it your self at https://premake.github.io/download.

```
python3 download_premake.py
```

It will download and place the `premake.exe` (in windows) binary next to
it, next run the following command to generate Visual studio solution files.

```
premake5 vs2019
```

for other project files and information see: https://premake.github.io/docs/

## Running Benchmarks

Build a release version of the pocketlang (using make file or the build.bat
script) and run the following command to run benchmarks. It'll generate a
benchmark report named `report.html` in this directory.

```
python3 run_benchmarks.py
```

## Other Scripts

`generate_native.py` - will generate the native interface (C extension
for pocketlang) source files from the `pocketlang.h` header. Rest of
the scripts name speak for it self. If you want to add a build script
or found a bug, feel free to open an issue or a PR.
 