## Example on how to integrate pocket VM with in your application.

- Including this example this repository contains several examples on how to
integrate pocket VM with your application
  - These examples (currently 2 examples)
  - The `cli/` application
  - The `docs/try/main.c` web assembly version of pocketlang


- To integrate pocket VM in you project:
  - Just drag and drop the `src/` directory in your project
  - Add all C files in the directory with your source files
  - Add `src/include` to the include path
  - Compile

----

#### `example0.c` - Contains how to run simple pocket script
```
gcc example0.c -o example0 ../../src/core/*.c ../../src/libs/*.c -I../../src/include -lm
```

#### `example1.c` - Contains how to pass values between pocket VM and C
```
gcc example1.c -o example1 ../../src/core/*.c ../../src/libs/*.c -I../../src/include -lm
```

#### `example2.c` - Contains how to create your own custom native type in C
```
gcc example2.c -o example2 ../../src/core/*.c ../../src/libs/*.c -I../../src/include -lm
```

