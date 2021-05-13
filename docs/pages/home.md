
## PocketLang is a small, fast and friendly language for scripting and embedding.

An easy to use programming language with mixed syntax of ruby and python, that can be learned in less than an hour or in no time if you have python/ruby background.

```ruby
# Python like import statement.
from os import clock as now

# A recursive fibonacci function.
def fib(n)
  if n < 2 then return n end
  return fib(n-1) + fib(n-2)
end

# Print all fibonacci from 0 to 5 exclusive.
for i in 0..5
  print(fib(i))
end
```

The complete language (including the internals and runtime) is a standalone executable that can be easily copied to a flash drive. And the language itself can be compiled from source in seconds without any dependencies (of cause you need a C compiler and optionally a build system).