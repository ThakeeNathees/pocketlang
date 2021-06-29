## Example on how to integrate pocket VM with in your application.
### This readme is incomplete (WIP)

- Including this example this repository contains 3 examples on how to integrate
pocket VM with your application
  - This example
  - The `cli/` application
  - The `docs/try/main.c` web assembly version of pocketlang


- To integrate pocket VM in you project:
  - Just drag and drop the `src/` directory in your project
  - Add all C files in the directory with your source files
  - Add `src/include` to the include path
  - Compile

----

Compile the `example.c` file which contains examples on how to write your custom module
to pocketlang, using the below command.

```
gcc example.c -o example ../../src/*.c -I../../src/include -lm
```

Note that only calling C from pocket is completed and calling pocket from C is WIP.
