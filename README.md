# PocketLang

<p align="center" >
<img src="https://user-images.githubusercontent.com/41085900/117528974-88fa8d00-aff2-11eb-8001-183c14786362.png" width="500" >
</p>

**PocketLang** is a simple and fast programming language written in C. It's syntactically similar to Ruby and it can be learned in less than an hour. Including the compiler, bytecode VM and runtime, it's a standalone executable with zero external dependecies just as it's self descriptive name. The pocketlang VM can be embedded in another hosting program very easily, as static/shared library or a generated single header version of the source, which makes it even effortless.

The language is written using [Wren Language](https://wren.io/) and their wonderful book [craftinginterpreters](http://www.craftinginterpreters.com/) as a reference.

### What PocketLang looks like

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

