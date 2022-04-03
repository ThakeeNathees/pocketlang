
Pocketlang support coroutines via [fibers](https://en.wikipedia.org/wiki/Fiber_(computer_science))
(light weight threads with cooperative multitask). A fiber object is a wrapper
around a function, contains the execution state (simply the stack and the
instruction pointer) for that function, which can be run and once yielded
resumed.

```ruby
import Fiber

def fn(h, w)
  print(h, w)
end

fb = Fiber.new(fn)              # Create a fiber.
Fiber.run(fb, 'hello', 'world') # Run the fiber.
```

## Yielding

When a function is yielded, it's state will be stored in the fiber it's
belongs to and will return from the function, to parent fiber it's running
from (not the caller of the function). And you can pass values between
fibers when they yield.

```ruby
import Fiber

def fn()
  print('running')
  yield()
  print('resumed')
end

fb = Fiber.new(fn)
Fiber.run(fb)    # Prints 'running'.
print('before resumed')
Fiber.resume(fb) # Prints 'resumed'.
```

Yield from the fiber with a value.

```ruby
import Fiber

def fn()
  print('running')
  yield(42) # Return 42.
  print('resumed')
end

fb = Fiber.new(fn)
val = Fiber.run(fb) # Prints 'running'.
print(val)          # Prints 42.
Fiber.resume(fb)    # Prints 'resumed'.
```

Resume the fiber with a value.

```ruby
import Fiber

def fn()
  print('running')
  val = yield() # Resumed value.
  print(val)    # Prints 42.
  print('resumed')
end

fb = Fiber.new(fn)
val = Fiber.run(fb)  # Prints 'running'.
Fiber.resume(fb, 42) # Resume with 42, Prints 'resumed'.
```

Once a fiber is done execution, trying to resume it will cause a runtime
error. To check if the fiber is finished check its attribute
`is_done` and use the attribute `function` to get it's function,
which could be used to create a new fiber to "re-start" the fiber.

```ruby
import Fiber

Fiber.run(fb = Fiber.new(
func()
  for i in 0..5 do
    yield(i)
  end
end))

while not fb.is_done
  Fiber.resume(fb)
end

# Get the function from the fiber.
fn = Fiber.get_func(fb)
```
