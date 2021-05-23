
var start = System.clock

var N = 50000000
var factors = []
for (i in 0..N) {
	if (N % i == 0) {
		factors.add(i)
	}
}

System.print("elapsed: %(System.clock - start) s")
