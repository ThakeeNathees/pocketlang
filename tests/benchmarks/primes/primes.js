function isPrime(n) {
	if (n < 2) return false;
	for (var i=2; i < n; i++) {
		if (n % i === 0) return false;
	}
	return true;
}

var start = process.hrtime();

var i=0, N=30000, primes=[];
for (; i < N; i++) {
	if (isPrime(i)) {
		primes.push(i);
	}
}

var end = process.hrtime(start);
var secs = (end[0] + end[1] / 1e9).toFixed(6) + 's';
console.log('elapsed:', secs);
