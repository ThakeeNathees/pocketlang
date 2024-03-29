
def reverse_list(list)
  (list.length >> 1).times do |i|
    last_index = list.length - i - 1
    list[i], list[last_index] = list[last_index], list[i]
  end
end

N = 20000000
list = (0...N).to_a

start = Time.now
reverse_list(list)
puts "elapsed: " + (Time.now - start).to_s + ' s'
