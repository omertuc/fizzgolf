# frozen_string_literal: true
FMT = "%d\n%d\nFizz\n%d\nBuzz\nFizz\n%d\n%d\nFizz\nBuzz\n%d\nFizz\n%d\n%d\nFizzBuzz"
(1..).each_slice(15) do |slice|
  puts format(FMT, slice[0], slice[1], slice[3], slice[6], slice[7], slice[10], slice[12], slice[13])
end
