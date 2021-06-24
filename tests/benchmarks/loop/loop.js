
// Note that javascript in Node/chrome (V8) is JIT compiled
// which makes more faster than other bytecode interpreted
// VM language listed here.

var start = process.hrtime();

var i=0, list=[];
for (; i < 10000000; i++) list.push(i);

var sum = 0;
for (i=0; i < list.length; i++) sum += list[i];
console.log(sum);

var end = process.hrtime(start);
var secs = (end[0] + end[1] / 1e9).toFixed(6) + 's';
console.log('elapsed:', secs);
