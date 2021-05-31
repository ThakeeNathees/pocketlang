
<p align="center" >
<img src="https://user-images.githubusercontent.com/41085900/117528974-88fa8d00-aff2-11eb-8001-183c14786362.png" width="500" >
</p>

**Pocketlang** is a small (~3000 semicollons) and fast functional programming language written in C. It's syntactically similar to Ruby and it can be learned in less than an hour. Including the compiler, bytecode VM and runtime, it's a standalone executable with zero external dependecies just as it's self descriptive name. The pocketlang VM can be embedded in another hosting program very easily.

The language is written using [Wren Language](https://wren.io/) and their wonderful book [craftinginterpreters](http://www.craftinginterpreters.com/) as a reference.

### What pocketlang looks like

```ruby
# Python like import statement.
from lang import clock as now

# A recursive fibonacci function.
def fib(n)
  if n < 2 then return n end
  return fib(n-1) + fib(n-2)
end

# Prints all fibonacci from 0 to 10 exclusive.
for i in 0..10
  print(fib(i))
end
```

## Performance!

The source files used to run benchmarks could be found at `test/benchmarks/` directory. They were ran using a small python script in the test directory.
- PC: ASUS N552VX, Intel Core i7-6700HQ 2.6GHz, 12GB SODIMM Ram
- Lnaguages: pocketlang (pre-alpha), wren v0.3.0, python v3.7.4, ruby v2.7.2,

![preformance2](https://user-images.githubusercontent.com/41085900/120123257-6f043280-c1cb-11eb-8c20-a42153268a0f.png)


When it comes to loops pocketlang much faster (even faster than [wren](https://wren.io/), the language I'm using as a reference) because we have dedicated opcode instructions for iterations and calling builtin functions. And it's is a functional language, which makes is possible to avoid object orianted/ class instance creation overhead.


## Building From Source

It can be build from source easily without any depencency, requirenments or even a build system, just a c99 compatible compiler is enough. It can be compiled with the following command.

#### gcc/mingw
```
gcc -o pocket cli/*.c src/*.c -Isrc/include -lm -Wno-int-to-pointer-cast
```

#### msvc
```
cl /Fepocket cli/*.c src/*.c /Isrc/include && rm *.obj
```

For other compilers it's the same (If you weren't able to compile it, please report by [opening an issue](https://github.com/ThakeeNathees/pocketlang/issues/new)). In addition you can use some of our build scripts (Makefile, batch script for MSVC, SCons) in the `build/` directory. For more see [build from source docs](https://thakeenathees.github.io/pocketlang/Getting%20Started/build%20from%20source.html).

