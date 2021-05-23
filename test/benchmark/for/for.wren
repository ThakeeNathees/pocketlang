
var start = System.clock

var list = []
for (i in 0...10000000) list.add(i)
var sum = 0
for (i in list) sum = sum + i
System.print(sum)

System.print("elapsed: %(System.clock - start)")