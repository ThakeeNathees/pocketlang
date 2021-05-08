import time

def fib(n):
  if n < 2: return 1
  return fib(n - 1) + fib(n - 2)

start = time.process_time()
for i in range(0, 10):
  print(fib(28))
print("elapsed: " + str(time.process_time() - start), ' s')
