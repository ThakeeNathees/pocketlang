
<p align="center" >
<img src="https://user-images.githubusercontent.com/41085900/117528974-88fa8d00-aff2-11eb-8001-183c14786362.png" width="500" >
</p>

**Pocketlang** is a small (~3000 semicollons) and fast functional programming
language written in C. It's syntactically similar to Ruby and it can be learned
in less than an hour. Including the compiler, bytecode VM and runtime, it's a
standalone executable with zero external dependecies just as it's self descriptive
name. The pocketlang VM can be embedded in another hosting program very easily.

The language is written using [Wren Language](https://wren.io/) and their
wonderful book [craftinginterpreters](http://www.craftinginterpreters.com/) as
a reference.

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

## Performance

All the tests are ran on, Windows10 (64bit), ASUS N552VX, Intel Core i7-6700HQ 2.6GHz
with 12GB SODIMM Ram. And the language versions are: pocketlang (pre-alpha), wren v0.3.0,
python v3.7.4, ruby v2.7.2.

![preformance](https://user-images.githubusercontent.com/41085900/120123257-6f043280-c1cb-11eb-8c20-a42153268a0f.png)

The source files used to run benchmarks could be found at `test/benchmarks/`
directory. They were ran using a small python script in the test directory.

## Building From Source

It can be build from source easily without any depencency, or additional
requirenments except for a c99 compatible compiler. And build systems are
optional. It can be compiled with the following command.

#### GCC
```
gcc -o pocket cli/*.c src/*.c -Isrc/include -lm -Wno-int-to-pointer-cast
```

#### MSVC
```
cl /Fepocket cli/*.c src/*.c /Isrc/include && rm *.obj
```

### For other compiler/IDE

1. Create an empty project file / makefile.
2. Add all C files in the src directory.
3. Add all C files in the cli directory (**not** recursively).
4. Add src/include to include path.
5. Compile.

If you weren't able to compile it, please report by [opening an issue](https://github.com/ThakeeNathees/pocketlang/issues/new)).
In addition you can use some of our build scripts (Makefile, batch script for MSVC, SCons)
in the `build/` directory. For more see [build from source docs](https://thakeenathees.github.io/pocketlang/Getting%20Started/build%20from%20source.html).


## References
- Bob Nystrom.(2021) *craftinginterpreters* [online] Available at: www.craftinginterpreters.com/ (Accessed January 2021)

- leonard schütz. (2020) *Dynamic Typing and NaN Boxing* [online] Available at: https://leonardschuetz.ch/blog/nan-boxing/ (Accessed December 2020)

- Bob Nystrom.(2011) *Pratt Parsers: Expression Parsing Made Easy* [online] Avaliable at: http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/ (Accessed December 2020)

- Carol E. Wolf of Pace University, adapted by P. Oser. *The Shunting Yard Algorithm* [online] Available at: http://mathcenter.oxford.emory.edu/site/cs171/shuntingYardAlgorithm/ (Accessed September 2020)

