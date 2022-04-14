
var reverse_list = Fn.new { |list|
  var count = (list.count / 2).floor
  for (i in 0...count) {
    var last_index = list.count - i - 1
    var last = list[last_index]
    list[last_index] = list[i]
    list[i] = last
  }
  return list
}

var N = 20000000
var list = []
for (i in 0...N) {
  list.add(i)
}

var start = System.clock
reverse_list.call(list)
System.print("elapsed: %(System.clock - start) s")
