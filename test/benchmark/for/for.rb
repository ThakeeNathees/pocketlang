
start = Time.now

list = []
for i in 0...10000000 do list.append(i) end
sum = 0
for i in list do sum += i end
puts sum

puts "elapsed: " + (Time.now - start).to_s + ' s'
