# Import statements and Modules

Each source file in pocketlang itself is a module that can be imported in another module, which makes it easier to split and share the project. There are two major kinds of modules in pocketlang. First one is core modules which are builtin to the VM and the second one is the local modlues where it's a written script file you'll import it with the path of it.

## Importing a core module

The import statement of the pocketlang is highly inspired from python's import syntax. Here how it looks like.

```ruby
# To import a core modlue.
import lang

# Import multiple modules.
import lang, math

# Import functions from a module.
from lang import write, gc

# Using alias to bind with a differt name.
import math as foo
from lang import clock as bar

# Import everything from a module.
from math import *

```

## Importing a local module

Importing a local module is same as importing a core module but instead of using the module name, you have to use it's path (either relative or absolute).

```ruby
# To import a local script with relative path.
import "foo.pk"
import "foo/bar.pk"

# Same rules with multiple imports and aliasing.
import 'foo.pk' as foo, '/bar.pk' as bar
from '../baz.pk' import *

```

If the local scripts have defined a module name, they'll imported and binded with it's module name unless they've imported with an alias. If the local script don't have a module name and imported without an alias, every symbols (global variables, and functions) will be imported and that's similer to import all statement.

```ruby
# 'foo.pk' isn't defined a module name.
import 'foo.pk'
fn() ## The function imported from 'foo.pk'

# We can use alias as a namespace if it doesn't have one.
import 'foo.pk' as foo
foo.fn()

# 'bar.pk' is defined with a module name 'bar'.
# It'll imported and binded as variable bar.
import 'bar.pk'
bar.fn()

``` 

## The module keyword.

We can define a name to a modlue with the `module` keyword. The name will become the namespace for that module's functions and global variables when importing it.


```ruby
# 'foo.pk'
module foo
```

Note that the module name must be the first statement of the script and declared only once.

```ruby
# 'bar.pk'
module bar
fn = func print('hello') end

# 'foo.pk'
import './bar.pk'
bar.fn() ## prints 'hello'
```
