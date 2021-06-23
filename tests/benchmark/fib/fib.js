function fib(n) {
	if (n < 2) return n;
	return fib(n - 1) + fib(n - 2);
}

var start = process.hrtime();
for (var i=0; i < 10; i++) {
	process.stdout.write(fib(30) + '\n');
}

var end = process.hrtime(start);
var secs = (end[0] + end[1] / 1e9).toFixed(6) + 's';
console.log('elapsed:', secs);
