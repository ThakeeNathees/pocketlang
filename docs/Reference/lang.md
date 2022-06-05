# lang

### gc

```ruby
lang.gc() -> Number
```

Trigger garbage collection and return the amount of bytes cleaned.

### disas

```ruby
lang.disas(fn:Closure) -> String
```

Returns the disassembled opcode of the function [fn].

### backtrace

```ruby
lang.backtrace() -> String
```

Returns the backtrace as a string, each line is formated as '<function>;<file>;<line>
'.

### modules

```ruby
lang.modules() -> List
```

Returns the list of all registered modules.

### debug_break

```ruby
lang.debug_break() -> Null
```

A debug function for development (will be removed).
