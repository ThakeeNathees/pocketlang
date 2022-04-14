local function reverse(arr)
	local len = #arr
	local max = math.floor(len / 2)

	for i=1, max do
		local idx = len + 1 - i
		arr[i], arr[idx] = arr[idx], arr[i]
	end
end

local N = 20000000
local list = {}
for i=1, N do
	list[i] = i
end

local start = os.clock()
reverse(list)
local seconds = os.clock() - start
print('elapsed: ' .. seconds .. 's')
