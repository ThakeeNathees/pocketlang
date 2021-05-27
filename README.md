# PocketLang

<p align="center" >
<img src="https://user-images.githubusercontent.com/41085900/117528974-88fa8d00-aff2-11eb-8001-183c14786362.png" width="500" >
</p>

**PocketLang** is a small (~3000 semicollons) and fast programming language written in C. It's syntactically similar to Ruby and it can be learned in less than an hour. Including the compiler, bytecode VM and runtime, it's a standalone executable with zero external dependecies just as it's self descriptive name. The pocketlang VM can be embedded in another hosting program very easily, as static/shared library or a generated single header version of the source, which makes it even effortless.

The language is written using [Wren Language](https://wren.io/) and their wonderful book [craftinginterpreters](http://www.craftinginterpreters.com/) as a reference.

### What PocketLang looks like

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

