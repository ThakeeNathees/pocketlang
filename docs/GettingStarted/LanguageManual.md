# Language Manual

You can consider pocketlang's syntax as python without indentation. For block statements it uses the ruby way, 'end' keyword
to close a block, end a class, function etc. Most of the semantics are simillar to python and if you're know python it fairly
easier to grasp pocketlang.

## Hello World

```ruby
print('Hello World')
```

## Comments and White Spaces

Comments are marked with `#` and ends at the next new line character.

```ruby
# Beautiful is better than ugly.
# Explicit is better than implicit.
# Simple is better than complex.
# Complex is better than complicated.
```

Except for new lines all the white spaces are ignored. New lines are used to end a statement, every statement should
ends with a new line or semicollon.

## Code blocks

Code blocks are following the same rule of ruby. All the blocks are closed with the `end` keyword,
all blocks are stards with either a new line and an optional "block entering keyword". For a single line
block these keywords are must. `if` blocks starts with the `then`, for `while` and `for` loops they starts
with the `do` keyword.

```ruby
## The `do` keyword is a must here.
while cond do something() end

## The `do` keyword is a optional here.
for i in 0..10
  print('$i')
end

## `then` is optional if new line is present.
if cond1 then
  foo()
else if cond2
  bar()
else
  baz()
end
```


# Data Types

Pocketlang has 3 primitive types which are `null`, `boolean` and `number` all the number values are represented as
IEEE 754 double precision floats. `null`, `true`, `false` are their literals and number support binary, hex and scientific
literals `0b101001`, `0xc0ffee`, `3e8`.

There are a handfull number of reference types like String, Range, List, Map, Closure (which is the first class functions),
Fiber, etc. And you can define you own with the `class` keyword.

## String

String sin pocketlang can be either double quoted or single quoted. At the moment pocket lang only supported a limited
number of string escape characters and UTF8 is not supported. However it'll be implemented before the first release.

```ruby
"Hello there!"; 'I\'m a string.'

"I'm a multi
line string."
```

Additionally pocketlang strings by default support interpolation with the `$` symbol. For a single identifier
it can be just written without any enclosing brackets, and for a long expressions they should be inside curly
brackets. And they can be nested upto 8 depth.

```ruby
n = 25
print('sqrt($n) = ${sqrt(n)}')
```

## Range

A range value represent an interval, they can be created with the `..` operator between two whole numbers.

```ruby
for i in 0..10
  print(i)
end
```

!> Unlike ruby we don't have a `...` operator and pocketlang Range objects are exclusive.

Exclusive ranges are more natural and more usefull when it comes to iterate over the length of something, thus
pocketlang's ranges are always exclusive. And having two operator for the same but slightly different operation is
a bit redundant in our humble openion.

## List

Lists are a collection of ordered objects. Each element can be indexed starting at 0. Internally a list is a
dynamically allocated variables buffer they'll grow or shrink as needed to store them.

```ruby
[ 'apples', true, 0..5 ]

[
  [ 1, 2, 3],
  [ 4, 5, 6],
  [ 7, 8, 9],
]
```

## Map

Maps are a collection of un ordered objects that stores the mapping of unique keys to the values. Internally it's
implemented as a has table and it require a value to be hashable for it to be a key. In pocketlang only primitive
types and immutable objects (`String` and `Range`) are hashable.

```ruby
{
  12 : 34,
  0..3 : [0, 1, 2],
  'foo' : 'bar',
}
```

## Closure

Closure are the first class citizen of functions that can be created, assigned to variables, passed as an argument,
or returned from a function call. A closure can be created with the `fn` keyword, which will crate an
anonymous function and wraps itself around it.

```ruby
y = fn (x) return x * x end
```

Pocketlang support lexical scoping which allows the closure to capture an external local variable
out of it's own scope. They're more like lambda expressions in other langauges.

```ruby
def make_function(m, c)
  return fn(x)
    return m * x + c
  end
end
```

?> Note that a call of form `f(fn ... end)` is a syntax sugar for `f fn ... end` where `f` is a function
or a method that takes a single literal function as argument, like Lua.

```ruby
5.times fn(i)
  print(i)
end
```

## Fiber

Pocketlang support coroutines via [fibers](https://en.wikipedia.org/wiki/Fiber_(computer_science))
(light weight threads with cooperative multitask). A fiber object is a wrapper around a closure, contains
the execution state (simply the stack and the instruction pointer) of that closure, which can be run
and once yielded resumed.

```ruby
def foo()
  print("Hello from fiber!")
end

fb = Fiber(foo)
fb.run()
```

When a function is yielded, it's state will be stored in the fiber it's belongs to and will return from
the function, to parent fiber it's running from (not the caller of the function). You can pass values
between fibers when they yield.

```ruby
def foo()
  print('running')
  yield()
  print('resumed')
end

fb = Fiber(foo)
fb.run()    # Prints 'running'.
print('before resumed')
fb.resume() # Prints 'resumed'.
```

Yield from the fiber with a value.

```ruby
def foo()
  print('running')
  yield(42) # Return 42.
  print('resumed')
end

fb = Fiber(foo)
val = fb.run() # Prints 'running'.
print(val)     # Prints 42.
fb.resume()    # Prints 'resumed'.
```

Resume the fiber with a value.

```ruby
def foo()
  print('running')
  val = yield() # Resumed value.
  print(val)    # Prints 42.
  print('resumed')
end

fb = Fiber(foo)
val = fb.run()  # Prints 'running'.
fb.resume(42)   # Resume with 42, Prints 'resumed'.
```

Once a fiber is done execution, trying to resume it will cause a runtime error. To check if the
fiber is finished check its attribute `is_done` and use the attribute `function` to get it's function,
which could be used to create a new fiber to "re-start" the fiber.

```ruby
fb = Fiber fn()
  for i in 0..5
    yield(i)
  end
end

fb.run()

while not fb.is_done
  fb.resume()
end

# Get the function from the fiber.
f = fb.get_func()
```

## Classes

Classes are the blueprint of objects, contains method definitions and behaviors for its instances.
The instance of a class method can be accessed with the `self` keyword.

```ruby
class Foo end
foo = Foo() ## Create a foo instance of Foo class.
```

To initialize an instance when it's constructed use `_init` method. Pocketlang instance attributes
are dynamic (means you can add a new field to an instance on the fly).

```ruby
class Foo
  def _init(bar, baz)
    self.bar = bar
  end
end

foo = Foo('bar', 'baz')
```

To override an operator just use the operator symbol as the method name.

```ruby
class Vec2
  def _init(x, y)
    self.x = x; self.y = y
  end
  def _str
    return "<${self.x}, ${self.y}>"
  end
  def + (other)
    return Vec2(self.x + other.x,
                self.y + other.y)
  end
  def += (other)
    self.x += other.x
    self.y += other.y
    return self
  end
  def == (other)
    return self.x == other.x and self.y == other.y
  end
end
```

To distinguish unary operator with binary operator the `self` keyword should be used.

```ruby
class N
  def _init(n)
    self.n = n
  end

  def - (other) ## N(1) - N(2)
    return N(self.n - other.n)
  end

  def -self () ## -N(1)
    return N(-self.n)
  end
end
```

All classes are ultimately inherit an abstract class named `Object` to inherit from any other class
use `is` keyword at the class definition. However you cannot inherit from the builtin class like
Number, Boolean, Null, String, List, ...

```ruby
class Shape # Implicitly inherit Object class
end

class Circle is Shape # Inherits the Shape class
end
```

To override a method just redefine the method in a subclass.

```ruby
class Shape
  def area()
    assert(false)
  end
end

class Circle is Shape
  def _init(r)
    self.r = r
  end
  def area()
    return math.PI * r ** 2
  end
end
```

To call the a method on the super class use `super` keyword. If the method name is same as the current
method `super()` will do otherwise method name should be specified `super.method_name()`.

```ruby
class Rectangle is Shape
  def _init(w, h)
    self.w = w; self.h = h
  end
  def scale(fx, fy)
    self.w *= fx
    self.h *= fy
  end
end

class Square is Rectangle
  def _init(s)
    super(s, s) ## Calls super._init(s, s)
  end
  
  def scale(x)
    super(x, x)
  end

  def scale2(x, y)
    super.scale(x, y)
  end
end
```

## Importing Modules

A module is a collection of functions classes and global variables. Usually a single script file
will be compiled to a module. To import a module use `import` statement.

```ruby
import math # Import math module.
import lang, math # Import multiple modules.
from math import sin, tan # Import symbols from a module.

# Using alias to bind with a different name.
import math as foo
from lang import clock as now

# Import inside a directory
import foo.bar # Import module bar from foo directory
import baz # If baz is a directory it'll import baz/_init.pk

# It'll only search for foo relatievly.
import .foo # ./foo.pk or ./foo/_init.pk or ./foo.dll, ...

# ^ meaning parent directory relative to this script.
import ^^foo.bar.baz # ../../foo/bar/baz.pk

```

?> Note that star import `from foo import *` is not supported but may be in the future.
