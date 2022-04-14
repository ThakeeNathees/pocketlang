from time import process_time as clock
from math import floor

def reverse_list(list):
	count = floor(len(list) / 2)
	for i in range(count):
		last_index = len(list) - i - 1
		list[i], list[last_index] = list[last_index], list[i]
	return list

N = 20000000
l = list(range(N))

start = clock()
reverse_list(l)
print('elapsed: ', clock() - start, 's')
