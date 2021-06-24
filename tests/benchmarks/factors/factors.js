
// Note that javascript in Node/chrome (V8) is JIT compiled
// which makes more faster than other bytecode interpreted
// VM language listed here.

var start = process.hrtime();

var N = 50000000;
var i=0, factors = []
for (; i <= N; i++) {
	if (N % i === 0) factors.push(i);
}

var end = process.hrtime(start);
var secs = (end[0] + end[1] / 1e9).toFixed(6) + 's';
console.log('elapsed:', secs);
