
# %% Fibers %%

Pocketlang support coroutines via [fibers](https://en.wikipedia.org/wiki/Fiber_(computer_science))
(light weight threads with cooperative multitask). A fiber object is a wrapper
around a function, contains the execution state (simply the stack and the
instruction pointer) for that function, which can be run and once yielded
resumed.

```ruby
def fn(h, w)
	print(h, w)
end

fb = fiber_new(fn)              # Create a fiber.
fiber_run(fb, 'hello', 'world') # Run the fiber.
```

## %% Yielding %%

When a function is yielded, it's state will be stored in the fiber it's
belongs to and will return from the function, to parent fiber it's running
from (not the caller of the function). And you can pass values between
fibers when they yield.

```ruby
def fn()
	print('running')
	yield()
	print('resumed')
end

fb = fiber_new(fn)
fiber_run(fb)    # Prints 'running'.
print('before resumed')
fiber_resume(fb) # Prints 'resumed'.
```

Yield from the fiber with a value.

```ruby
def fn()
	print('running')
	yield(42) # Return 42.
	print('resumed')
end

fb = fiber_new(fn)
val = fiber_run(fb) # Prints 'running'.
print(val)          # Prints 42.
fiber_resume(fb)    # Prints 'resumed'.
```

Resume the fiber with a value.

```ruby
def fn()
	print('running')
	val = yield() # Resumed value.
	print(val)    # Prints 42.
	print('resumed')
end

fb = fiber_new(fn)
val = fiber_run(fb)  # Prints 'running'.
fiber_resume(fb, 42) # Resume with 42, Prints 'resumed'.
```

Once a fiber is done execution, trying to resume it will cause a runtime
error. To check if the fiber is finished it's execution use the function
`fiber_is_done` and use the function `fiber_get_func` to get it's function,
which could be used to create a new fiber to "re-start" the fiber.

```ruby
fiber_run(fb = fiber_new(
func()
  for i in 0..5 do
    yield(i)
  end
end))

while not fiber_is_done(fb)
	fiber_resume(fb)
end

# Get the function from the fiber.
fn = fiber_get_func(fb)
```
