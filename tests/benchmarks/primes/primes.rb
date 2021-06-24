
def is_prime(n)
  if n < 2 then return false end
  for i in 2...n
    if n % i == 0 then return false end
  end
  return true
end

start = Time.now
N = 30000; primes = []
for i in 0...N
  if is_prime(i)
    primes.append(i)
  end
end

puts "elapsed: " + (Time.now - start).to_s + " s"

