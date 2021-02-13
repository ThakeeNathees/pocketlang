## MiniScript Language

MiniScript is a simple embeddable, functional, dynamic-typed, bytecode-interpreted, scripting language written in C. It uses the [mark-and-sweep](https://en.wikipedia.org/wiki/Tracing_garbage_collection) method for garbage collection. MiniScript is  is syntactically similar to Ruby. The frontend and expression parsing techniques were written using [Wren Language](https://wren.io/)  and their wonderful book [craftinginterpreters](http://www.craftinginterpreters.com/) as a reference.

### What MiniScript looks like

```ruby

## Find and return the maximum value in the [list].
def get_max(list)
  ret = list[0]
  for i in 1..list.length
    ret = max(ret, list[i])
  end
  return ret
end

## Return a list where each element returns true with function [fn] and
## belongs to [list].
def filter(list, fn)
  ret = []
  for elem in list
    if fn(elem)
      list_append(ret, elem)
    end
  end
  return ret
end

## A list of range literal, first class functions and more types.
list = [42, null, 0..10, function() print('hello') end, [3.14]]
nums = filter(list, is_num)
print("Max is", get_max(nums))

```

