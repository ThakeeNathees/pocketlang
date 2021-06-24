local function fib(n)
	if n < 2 then
		return n
	else
		return fib(n - 1) + fib(n - 2)
	end
end

local start = os.clock()

for i=1, 10 do
	print(fib(30))
end

local seconds = os.clock() - start
print('elapsed: ' .. seconds .. 's')
