local start = os.clock()

local list = {}
for i = 1, 10000000 do
	list[i] = i - 1
end

local sum = 0

for i = 1, #list do
	sum = sum + list[i]
end

print(sum)

local seconds = os.clock() - start
print('elapsed: ' .. seconds .. 's')
