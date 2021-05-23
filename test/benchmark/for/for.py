from time import process_time as clock

start = clock()

list = []
for i in range(10000000): list.append(i)
sum = 0
for i in list: sum += i
print(sum)

print("elapsed:", clock() - start)
