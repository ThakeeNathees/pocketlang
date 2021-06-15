
# %% Learn pocketlang in 15 minutes %%

```ruby

# This is a comment.

x = 0  # Creating a variable.

# In pocketlang statements should end with a new line
# or a semicolon. White space characters except for new
# lines are ignored in pocketlang.
a = 1; b = 2;

# Data types.
# -----------

null                   # A null type.
true; false            # Booleans.
42; 3.14               # Numbers.
0..10; 10..0           # Range (0..10 = 0 <= r < 10).
"hello"; 'world'       # Strings (support multiline).
[42, 'foo', null]      # Lists.
{ 'Key':'value' }      # Maps.
func(x) return x*x end # Lambda/literal functions.

# Control flow.
# -------------

# If condition.
if x == 'foo'
	print('bar')
elif x == 'bar'
	print('baz')
end

# In a single line (should add 'then' keyword).
if x == 'foo' then print('bar') end

# For loops, here 'do' keyword is optional if we have a
# newline after the sequence (like 'then' in if statements).
for i in 0..10 do
	print(i)
end

# While statement.
while x > 0 do print(x -= 1) end

# In pocketlang variable's lifetime are scope based.
if true then
	local = null
end
#print(local) # Error: Name 'local' is not defined.

# Functions.
#-----------

def add(a, b)
	return a + b
end

# Functions can be assigned to a variable.
fn = func(x) return x*x end

# Functions can be passed as an argument and can be returned.
def call(fn, x)
	fn(x)
	return func print('foo') end
end

# Concatenative call operator '->'

str_lower(str_strip('Foo ')) # This can be written as below
'Foo ' -> str_strip -> str_lower

'foo' -> print # similar to print('foo')

# Fibers & Coroutine
#-------------------

def fn(p1, p2)
	print(yield(42)) # Prints 3.14
end

fb = fiber_new(fn)
val = fiber_run(fb, 1, 2)
print(val) ## Prints 42
fiber_resume(fb, 3.14)
```
