## Example on how compile a native extension.

#### MSVC
```
cl /LD mylib.c pknative.c /I../../../src/include/
rm *.obj *.exp *.lib
```

#### GCC
```
gcc -fPIC -c mylib.c pknative.c -I../../../src/include/
gcc -shared -o mylib.so mylib.o pknative.o
rm *.o
```
