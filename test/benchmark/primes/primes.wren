
var is_prime = Fn.new {|n|
  if (n < 2) return false
  for (i in 2...n) {
    if (n % i == 0) return false
  }
  return true
}


var start = System.clock
var N = 60000
var primes = []
for (i in 0...N) {
  if (is_prime.call(i)) {
    primes.add(i)
  }
}
System.print("elapsed: %(System.clock - start)")

