local start = os.clock()

local factors = {}
local N = 50000000

for i=1,N do
	if N % i == 0 then
		table.insert(factors, i)
	end
end

local seconds = os.clock() - start
print('elapsed: ' .. seconds .. 's')
