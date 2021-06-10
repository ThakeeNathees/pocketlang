
// Note that javascript in Node/chrome (V8) is JIT compiled
// which makes more faster than other bytecode interpreted
// VM language listed here.

var start = +new Date();

list = []
for (var i = 0; i < 10000000; i++) { list.push(i) }
sum = 0
for (var i = 0; i < list.length; i++) { sum += list[i]; }
console.log(sum)

var end = +new Date();
console.log('elapsed: ' + (end - start)/1000 + 's');
