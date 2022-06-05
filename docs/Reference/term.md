# term

### init

```ruby
term.init(capture_events:Bool) -> Null
```

Initialize terminal with raw mode for tui applications, set [capture_events] true to enable event handling.

### cleanup

```ruby
term.cleanup() -> Null
```

Cleanup and resotre the last terminal state.

### isatty

```ruby
term.isatty() -> Bool
```

Returns true if both stdin and stdout are tty.

### new_screen_buffer

```ruby
term.new_screen_buffer() -> Null
```

Switch to an alternative screen buffer.

### restore_screen_buffer

```ruby
term.restore_screen_buffer() -> Null
```

Restore the alternative buffer which was created with term.new_screen_buffer()

### getsize

```ruby
term.getsize() -> types.Vector
```

Returns the screen size.

### getposition

```ruby
term.getposition() -> types.Vector
```

Returns the cursor position in the screen on a zero based coordinate.

### read_event

```ruby
term.read_event(event:term.Event) -> Bool
```

Read an event and update the argument [event] and return true.If no event was read it'll return false.

## Event
The terminal event type, that'll be used at term.read_event function to fetch events.

### binary_mode

```ruby
term.binary_mode() -> Null
```

On windows it'll set stdout to binary mode, on other platforms this function won't make make any difference.
