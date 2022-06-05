# io

### write

```ruby
io.write(stream:Var, bytes:String) -> Null
```

Warning: the function is subjected to be changed anytime soon.
Write [bytes] string to the stream. stream should be any of io.stdin, io.stdout, io.stderr.

### flush

```ruby
io.flush() -> Null
```

Warning: the function is subjected to be changed anytime soon.
Flush stdout buffer.

### getc

```ruby
io.getc() -> String
```

Read a single character from stdin and return it.

## File
A simple file type.

### open

```ruby
io.File.open(path:String, mode:String) -> Null
```

Opens a file at the [path] with the [mode]. Path should be either absolute or relative to the current working directory. and [mode] can be'r', 'w', 'a' in combination with 'b' (binary) and/or '+' (extended).
```
 mode | If already exists | If does not exist |
 -----+-------------------+-------------------|
 'r'  |  read from start  |   failure to open |
 'w'  |  destroy contents |   create new      |
 'a'  |  write to end     |   create new      |
 'r+' |  read from start  |   error           |
 'w+' |  destroy contents |   create new      |
 'a+' |  write to end     |   create new      |
```

### read

```ruby
io.File.read(count:Number) -> String
```

Reads [count] number of bytes from the file and return it as String.If the count is -1 it'll read till the end of file and return it.

### write

```ruby
io.File.write(data:String) -> Null
```

Write the [data] to the file. Since pocketlang string support any validbyte value in it's string, binary data can also be written with strings.

### getline

```ruby
io.File.getline() -> String
```

Reads a line from the file and return it as string. This function can only be used for files that are opened with text mode.

### close

```ruby
io.File.close()
```

Closes the opend file.

### seek

```ruby
io.File.seek(offset:Number, whence:Number) -> Null
```

Move the file read/write offset. where [offset] is the offset from [whence] which should be any of the bellow three.
  0: Begining of the file.
  1: Current position.
  2: End of the file.

### tell

```ruby
io.File.tell() -> Number
```

Returns the read/write position of the file.
