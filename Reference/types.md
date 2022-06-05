# types

### hashable

```ruby
types.hashable(value:Var) -> Bool
```

Returns true if the [value] is hashable.

### hash

```ruby
types.hash(value:Var) -> Number
```

Returns the hash of the [value]

## ByteBuffer
A simple dynamically allocated byte buffer type. This can be used for constructing larger strings without allocating and adding smaller intermeidate strings.

### []

```ruby
types.ByteBuffer.[](index:Number)
```



### []=

```ruby
types.ByteBuffer.[]=(index:Number, value:Number)
```



### reserve

```ruby
types.ByteBuffer.reserve(count:Number) -> Null
```

Reserve [count] number of bytes internally. This is use full if the final size of the buffer is known beforehand to avoid reduce the number of re-allocations.

### fill

```ruby
types.ByteBuffer.fill(value:Number) -> Null
```

Fill the buffer with the given byte value. Note that the value must be in between 0 and 0xff inclusive.

### clear

```ruby
types.ByteBuffer.clear() -> Null
```

Clear the buffer values.

### write

```ruby
types.ByteBuffer.write(data:Number|String) -> Null
```

Writes the data to the buffer. If the [data] is a number that should be in between 0 and 0xff inclusively. If the [data] is a string all the bytes of the string will be written to the buffer.

### string

```ruby
types.ByteBuffer.string() -> String
```

Returns the buffered values as String.

### count

```ruby
types.ByteBuffer.count() -> Number
```

Returns the number of bytes that have written to the buffer.

## Vector
A simple vector type contains x, y, and z components.

### _repr

```ruby
types.Vector._repr()
```


