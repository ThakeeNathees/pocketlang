
os = import('os')

def fib(n)
  if n < 2 do return 1 end
  return fib(n - 1) + fib(n - 2)
end

start = os.clock()
for i in 0..10
  print(fib(28))
end
print('elapsed:', os.clock() - start, 's')

