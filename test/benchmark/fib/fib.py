from time import process_time as clock

def fib(n):
  if n < 2: return n
  return fib(n - 1) + fib(n - 2)

start = clock()
for i in range(0, 10):
  print(fib(32))
print("elapsed: ", clock() - start, 's')
