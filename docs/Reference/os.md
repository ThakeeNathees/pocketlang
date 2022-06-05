# os

### getcwd

```ruby
os.getcwd() -> String
```

Returns the current working directory

### chdir

```ruby
os.chdir(path:String)
```

Change the current working directory

### mkdir

```ruby
os.mkdir(path:String)
```

Creates a directory at the path. The path should be valid.

### rmdir

```ruby
os.rmdir(path:String)
```

Removes an empty directory at the path.

### unlink

```ruby
os.rmdir(path:String)
```

Removes a file at the path.

### moditime

```ruby
os.moditime(path:String) -> Number
```

Returns the modified timestamp of the file.

### filesize

```ruby
os.filesize(path:String) -> Number
```

Returns the file size in bytes.

### system

```ruby
os.system(cmd:String) -> Number
```

Execute the command in a subprocess, Returns the exit code of the child process.

### getenv

```ruby
os.getenv(name:String) -> String
```

Returns the environment variable as String if it exists otherwise it'll return null.

### exepath

```ruby
os.exepath() -> String
```

Returns the path of the pocket interpreter executable.
