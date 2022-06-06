# path

### getcwd

```ruby
path.getcwd() -> String
```

Returns the current working directory.

### abspath

```ruby
path.abspath(path:String) -> String
```

Returns the absolute path of the [path].

### relpath

```ruby
path.relpath(path:String, from:String) -> String
```

Returns the relative path of the [path] argument from the [from] directory.

### join

```ruby
path.join(...) -> String
```

Joins path with path seperator and return it. The maximum count of paths which can be joined for a call is MAX_JOIN_PATHS.

### normpath

```ruby
path.normpath(path:String) -> String
```

Returns the normalized path of the [path].

### basename

```ruby
path.basename(path:String) -> String
```

Returns the final component for the path

### dirname

```ruby
path.dirname(path:String) -> String
```

Returns the directory of the path.

### isabspath

```ruby
path.isabspath(path:String) -> Bool
```

Returns true if the path is absolute otherwise false.

### getext

```ruby
path.getext(path:String) -> String
```

Returns the file extension of the path.

### exists

```ruby
path.exists(path:String) -> String
```

Returns true if the file exists.

### isfile

```ruby
path.isfile(path:String) -> Bool
```

Returns true if the path is a file.

### isdir

```ruby
path.isdir(path:String) -> Bool
```

Returns true if the path is a directory.

### listdir

```ruby
path.listdir(path:String='.') -> List
```

Returns all the entries in the directory at the [path].
