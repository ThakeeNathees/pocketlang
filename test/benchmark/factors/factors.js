
// Note that javascript in Node/chrome (V8) is JIT compiled
// which makes more faster than other bytecode interpreted
// VM language listed here.

var start = +new Date();

var N = 50000000;
var factors = []
for (var i = 0; i <= N; i++) {
	if (N % i == 0) factors.push(i);
}

var end = +new Date();
console.log('elapsed: ' + (end - start)/1000 + 's');
