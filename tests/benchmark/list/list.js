function reverse(list) {
	var i=0, tmp, idx, count=list.length;
	for (; i < count; i++) {
		idx = count - i - 1;
		tmp = list[idx];
		list[idx] = list[i];
		list[i] = tmp;
	}
}

var start = process.hrtime();

var i=0, N=20000000, list=Array(N);
for (; i < N; i++) list[i] = i;
reverse(list);

var end = process.hrtime(start);
var secs = (end[0] + end[1] / 1e9).toFixed(6) + 's';
console.log('elapsed:', secs);
