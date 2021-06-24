local function isPrime(n)
	if n < 2 then
		return false
	end
  for i=2, n-1 do
    if n % i == 0 then
			return false
		end
  end
  return true
end

local start = os.clock()

local N = 30000
local primes = {}

for i = 0, N - 1 do
	if isPrime(i) then
		table.insert(primes, i)
	end
end

local seconds = os.clock() - start
print('elapsed: ' .. seconds .. 's')
