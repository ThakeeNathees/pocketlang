
<h1 class="text-center">
Pocketlang <sub>v0.1.0</sub>
</h1>

<p class="text-center">
Pocketlang is a lightweight & fast object oriented programming language designed for embedding and
scripting. Including the compiler, bytecode VM and runtime it's a <strong>standalone</strong> executable
which is less than <strong>300KB</strong> and the language itself can be compiled in less than
<strong>4s</strong> without any external dependencies.
</p>

<div class="center">
    <a class="button" target="_blank" href="/try-online.html"> Try Online </a>
    <a class="button" target="_blank" href="https://www.github.com/ThakeeNathees/pocketlang/"> GitHub </a>
</div>

<br>

Simplicity is the zen of pocketlang. The syntax is more of an executable pseudocode. Compiling the language
itself takes just a single gcc/msvc command. No configuration is required to embed it in another application.
pocket VM along with its compiler and standard runtime are a single executable that's less than 300KB. It's not
just small but much faster with memory efficient dynamic type system, self adjusting heap for garbage
collection, tail call optimization, single pass compilation etc, that makes it compete with main stream langages
like python, ruby, wren and lua. And more things that makes pocketlang a swiss knife of programmers.

## What it looks like

```ruby
# A recursive fibonacci function.
def fib(n)
  if n < 2 then return n end
  return fib(n-1) + fib(n-2)
end

# Prints all fibonacci from 0 to 10 exclusive.
for i in 0..10
  print("fib($i) = ${fib(i)}")
end
```

## Features

- No setup. Single binary and your're good to go.
- REPL
- Object oriented
- Dynamic typing
- Concurrency
- Garbage collection
- Operator overloading
- First class functions
- Lexical scoping
- Embeddable
- Direct interop with C
- Highly optimized loops
- Tail call optimization

## Getting Started

Note that at the moment there isn't any releases and it's actively being developed. To get your hands
dirty clone the [source](https://www.github.com/ThakeeNathees/pocketlang/) and build it. Compiling
the language itself takes just a single gcc/msvc command. See [README](https://github.com/ThakeeNathees/pocketlang#readme)
for more detail.
