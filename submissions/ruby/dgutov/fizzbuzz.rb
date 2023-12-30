# frozen_string_literal: true
MULT = 400
FMT = ("%d\n%d\nFizz\n%d\nBuzz\nFizz\n%d\n%d\nFizz\nBuzz\n%d\nFizz\n%d\n%d\nFizzBuzz\n" * MULT).freeze
WORKERS = 16

idxs = (0..MULT).flat_map do |i|
  [0, 1, 3, 6, 7, 10, 12, 13].map { |j| j + i * 15 }
end

eval <<EOS
  def spawn_worker
    Ractor.new do
      loop do
        i = Ractor.receive
        Ractor.yield format(FMT, #{idxs.map { |n| "i + #{n}" }.join(', ') }).freeze
      end
    end
  end
EOS

workers = (1...WORKERS).map { spawn_worker }

ii = 1

loop do
  workers.each do |worker|
    worker.send(ii)
    ii += 15 * MULT
  end
  workers.each { |worker| puts worker.take }
end
