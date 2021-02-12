## MiniScript Language

MiniScript is a simple embeddable, functional, dynamic-typed, bytecode-interpreted, scripting language written in C. It uses the [mark-and-sweep](https://en.wikipedia.org/wiki/Tracing_garbage_collection) method for garbage collection. MiniScript is  is syntactically similar to Ruby. The frontend and expression parsing techniques were written using [Wren Language](https://wren.io/)  and their wonderful book [craftinginterpreters](http://www.craftinginterpreters.com/) as a reference.

### What MiniScript looks like

```ruby

## Find and return the maximum value in the array.
def get_max(arr)
  ret = arr[0]
  for i in 1..arr.length
    ret = max(ret, arr[i])
  end
  return ret
end

## Return an array where each element returns true with function [fn] and
## belongs to [arr].
def filter(arr, fn)
  ret = []
  for elem in arr
    if fn(elem)
        array_append(ret, elem)
    end
  end
  return ret
end

array = [42, null, 3.14, "String", 0..10, [100]]
nums = filter(array, is_num)
print(get_max(nums))

```

