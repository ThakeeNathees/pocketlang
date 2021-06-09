
start = Time.now
N = 50000000; factors = []
for i in 1...N+1
	if N % i == 0
		factors.append(i)
	end
end
puts "elapsed: " + (Time.now - start).to_s + ' s'
