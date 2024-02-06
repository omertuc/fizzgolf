#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <thread>
#include <sys/mman.h>

namespace {

// Constexpr helper for calculating 10^X since std::pow is not constexpr.
constexpr int64_t PowTen(int x) {
  int64_t result = 1;
  for (int i = 0; i < x; ++i) {
    result *= 10;
  }
  return result;
}

// We process each batch in parallel from |kParallelism| threads. This number
// should be set to the available CPU cores or less. Note that higher number
// doesn't necessarily mean better performance due to the synchronization
// overhead between threads.
constexpr int64_t kParallelism = 7;
// The last kSuffixDigits digits of each number line are untouched when
// iterating.
constexpr int64_t kSuffixDigits = 6;
// Increment the first right-most touched digit by this much in one step. Must
// be divisible by 3. The code currently only handles when this is single-digit.
constexpr int64_t kIncrementBy = 3;
// One batch contains maximum this many lines.
constexpr int64_t kMaxLinesInBatch = PowTen(kSuffixDigits) * kIncrementBy / 3;

constexpr int kFizzLength = 4;
constexpr int kBuzzLength = 4;
constexpr int kNewlineLength = 1;

// A barrier that busy waits until all other threads reach the barrier.
template <typename Completion>
class SpinningBarrier {
 public:
  // Constructs a spinning barrier with |count| participating threads and
  // completion callback |completion_cb|.
  // After all threads reach the barrier, the last thread executes the
  // completion callback. The other threads are blocked until the completion
  // callback returns.
  SpinningBarrier(int64_t count, Completion completion_cb) :
      count_(count), spaces_(count), generation_(0),
      completion_cb_(completion_cb) {}

  void Wait() {
      int64_t my_generation = generation_;
      if (!--spaces_) {
          spaces_ = count_;
          completion_cb_();
          ++generation_;
      } else {
          while(generation_ == my_generation);
      }
  }

 private:
  int64_t count_;
  std::atomic<int64_t> spaces_;
  std::atomic<int64_t> generation_;
  Completion completion_cb_;
};

// Owns the output buffers and maintains which buffer was used last.
class OutputHandler {
 static constexpr size_t kBufferSize = 14 * 1024 * 1024;

 public:
  OutputHandler() {
    for (int i = 0; i < 3; ++i) {
      buffers_[i] =
        static_cast<char*>(aligned_alloc(2 * 1024 * 1024, kBufferSize));
      madvise(buffers_[i], kBufferSize, MADV_HUGEPAGE);
    }
  }

  ~OutputHandler() {
    for (int i = 0; i < 3; ++i) {
      free(buffers_[i]);
    }
  }

  void Output(int buffer_id, size_t bytes) {
    // We use three buffers. We have to ensure that while a buffer (or its
    // part) is in the pipe, it won't get modified. There is no API to know
    // when a downstream process is finished reading some data from the pipe,
    // so we choose the size of the pipe smartly.
    // As long as the pipe cannot fit more than two full buffers, we can ensure
    // that after after outputting buffer 0, 1, 2 in this order, the pipe no
    // longer contains data from buffer 0. However, if we make the pipe too
    // small, the program will be slower. The optimal pipe size is calculated by
    // TargetPipeSize. Since there is a minimum pipe size which we
    // cannot go below (4kb on Linux), this approach won't work when the
    // buffer size is too small. In these cases we fall back to write() which
    // copies the content into the pipe, therefore there is no risk of
    // overwriting memory that is still being read from the downstream process.
    // However, if in the subsequent call to Output(), a smaller size were
    // passed (and therefore the else branch were executed), the pipe could
    // still end up containing some data from the current iteration and the
    // entire data from the next iteration. We assume that Output() will be
    // invoked with monotonically increasing sizes (which is true in practice
    // but it'd be better not to depend on this assumption).
    SetPipeSize(TargetPipeSize(bytes));
    if (2 * bytes >= pipe_size_) {
      OutputWithVmSplice(buffers_[buffer_id], bytes);
    } else {
      if (write(STDOUT_FILENO, buffers_[buffer_id], bytes) < 0) {
        std::cerr << "write error: " << errno;
        std::abort();
      }
    }
  }

  char* GetBuffer(int buffer_id) {
    return buffers_[buffer_id];
  }

  // Returns the next buffer id that can be filled up and outputted.
  // Callers are responsible to actually output the buffer after requesting it
  // with this method.
  int NextBufferId() {
    buffer_id_ = (buffer_id_ + 1) % 3;
    return buffer_id_;
  }

  static constexpr int64_t BufferSize() {
    return kBufferSize;
  }

 private:
  // Calculates the optimal pipe size for outputting |out_bytes|.
  size_t TargetPipeSize(size_t out_bytes) const {
    // Pipe sizes must be powers of 2 and >= 4kb on Linux.
    // We want that the pipe is not bigger than twice the output (but still
    // maximize the pipe size), so we round |out_bytes| up to the nearest power
    // of two.
    return std::max(4ul * 1024, std::bit_ceil(out_bytes));
  }

  void OutputWithVmSplice(char* buffer, size_t bytes) const {
    iovec iov;
    iov.iov_base = buffer;
    iov.iov_len = bytes;
    while (true) {
      int64_t ret = vmsplice(STDOUT_FILENO, &iov, 1, SPLICE_F_NONBLOCK);
      if (ret >= 0) {
        iov.iov_len -= ret;
        iov.iov_base = reinterpret_cast<char*>(iov.iov_base) + ret;
        if (iov.iov_len == 0) {
          break;
        }
      } else {
        if (errno != EAGAIN) {
          std::cerr << "vmsplice error: " << errno;
          std::abort();
        }
      }
    }
  }

  void SetPipeSize(size_t size) {
    if (pipe_size_ == size) {
      return;
    }
    size_t new_pipe_size = fcntl(STDOUT_FILENO, F_SETPIPE_SZ, size);
    if (new_pipe_size < 0) {
      std::cerr << "Error while calling fcntl F_SETPIPE_SZ " << errno
      << "\nPerhaps you need to update /proc/sys/fs/pipe-max-size or "
         "run the program as sudo";
      std::abort();
    }

    pipe_size_ = new_pipe_size;
  }

  std::array<char*, 3> buffers_;
  int buffer_id_ = 0;
  size_t pipe_size_;
};

// Inserts the fizzbuzz line for line number |line| and a newline character
// into |out|.
// Returns the pointer pointing to the character after the newline.
char* InsertFizzBuzzLine(char* out, int64_t line) {
  if (line % 15 == 0) {
    std::memcpy(out, "FizzBuzz\n", 9);
    return out + 9;
  } else if (line % 3 == 0) {
    std::memcpy(out, "Fizz\n", 5);
    return out + 5;
  } else if (line % 5 == 0) {
    std::memcpy(out, "Buzz\n", 5);
    return out + 5;
  } else {
    // We support numbers up to 10^20.
    char* next = std::to_chars(out, out + 20, line).ptr;
    *next = '\n';
    return next + 1;
  }
}

// A run refers to all lines where the line numbers have |DIGITS| digits.
// Run<1>: [1,9]
// Run<2>: [10,99]
// ...
template<int DIGITS>
class Run {

  static_assert(DIGITS >= 1);

  static constexpr int FizzBuzzLineLength(int64_t number_mod_15) {
    if (number_mod_15 % 15 == 0) {
      return 9;
    } else if (number_mod_15 % 3 == 0) {
      return 5;
    } else if (number_mod_15 % 5 == 0) {
      return 5;
    } else {
      return DIGITS + 1;
    }
  }

  // Returns the size of one fifteener in bytes.
  static constexpr size_t FifteenerBytes() {
    size_t size = 0;
    for (int i = 0; i < 15; ++i) {
      size += FizzBuzzLineLength(i);
    }
    return size;
  }

  // Returns the number of lines in this run.
  static constexpr int64_t LinesInRun() {
    return PowTen(DIGITS) - PowTen(DIGITS - 1);
  }

  // The entire fizz-buzz output for this run takes this many bytes.
  static constexpr size_t RunBytes() {
    if constexpr(DIGITS == 1) {
      return 5 + 3 * kFizzLength + 1 * kBuzzLength + 9 * kNewlineLength;
    } else {
      return LinesInRun() / 15 * FifteenerBytes();
    }
  }

  // Returns the number of batches in this run.
  static constexpr int64_t BatchesInRun() {
    if constexpr (DIGITS > kSuffixDigits) {
      return PowTen(DIGITS - kSuffixDigits - 1) * 9;
    } else {
      return 1;
    }
  }

 public:
  // Outputs all lines for this run by using the buffers from |output_handler|.
  static void Execute(OutputHandler& output_handler) {
    Batch<0> batch0(&output_handler);
    Batch<1> batch1(&output_handler);
    Batch<2> batch2(&output_handler);
    // We fill up each batch with the initial values. This is a relatively slow
    // process so we only do it once per run. In subsequent iterations, we
    // only increment the numbers (see below) which is much faster.
    batch0.Init();
    batch0.Output();
    if constexpr (BatchesInRun() > 1) {
      batch1.Init();
      batch1.Output();
    }
    if constexpr (BatchesInRun() > 2) {
      batch2.Init();
      batch2.Output();
    }

    if constexpr (BatchesInRun() > 3) {
      int64_t prefix = PowTen(DIGITS - kSuffixDigits - 1);
      // We update the batch from |kParallelism| threads
      // We use a spinning barrier for synchronizing between the threads.
      // After all threads reach the barrier, the completion function is
      // executed and the output is written out. Then the next batch is
      // processed.
      SpinningBarrier barrier(kParallelism, [&] {
        switch (prefix % 3) {
          // In the beginning
          //   batch0 corresponds to prefix 10..00 ( ≡ 1 mod 3),
          //   batch1 corresponds to prefix 10..01 ( ≡ 2 mod 3),
          //   batch2 corresponds to prefix 10..02 ( ≡ 0 mod 3).
          // After all 3 batches are processed, the prefix is incremented by 3,
          // hence the mods don't change.
          case 0: batch2.Output(); break;
          case 1: batch0.Output(); break;
          case 2: batch1.Output(); break;
        }
        prefix++;
      });

      [&]<size_t... THREAD_ID>(std::index_sequence<THREAD_ID...>) {
        // Launch |kParallelism| number of threads. We could also use a thread
        // pool, but one run takes long enough that launching new threads is
        // negligible.
        (std::jthread([&] {
          for (int64_t batch = 3; batch < BatchesInRun();
               batch += 3) {
            // Each thread processes their corresponding chunk in the batch.
            Chunk<0, THREAD_ID>(batch0).IncrementNumbers(prefix);
            // At this point, all threads wait until every other thread reaches
            // the barrier, the last thread to finish will invoke batch.Output()
            // (see above at the definition of |barrier|).
            barrier.Wait();
            Chunk<1, THREAD_ID>(batch1).IncrementNumbers(prefix);
            barrier.Wait();
            Chunk<2, THREAD_ID>(batch2).IncrementNumbers(prefix);
            barrier.Wait();
          }
        }) , ...);
      }(std::make_index_sequence<kParallelism>());
    }
  }

  // A batch represents 10^|kSuffixDigits| lines of the output.
  // This is useful because the last |kSuffixDigits| digits don't need to be
  // updated. Furthermore, line numbers in one batch share the same prefix.
  // BATCH_ID ∈ [0, 1, 2]
  template<int BATCH_ID>
  class Batch {
    static_assert(BATCH_ID < 3);
    using PreviousBatch = Batch<BATCH_ID - 1>;

    public:
    Batch(OutputHandler* output_handler) : output_handler_(output_handler) {
      static_assert(OutputHandler::BufferSize() >= BytesInBatch());
    }

    // Initializes this batch by taking the next available buffer from
    // the output handler and filling it with the initial values.
    void Init() {
      buffer_id_ = output_handler_->NextBufferId();
      char* out = GetBuffer();
      int64_t start = PowTen(DIGITS - 1) + BATCH_ID * LinesInBatch();
      int64_t end = std::min(PowTen(DIGITS), start + LinesInBatch());
      for (int64_t line = start; line < end; ++line) {
        out = InsertFizzBuzzLine(out, line);
      }
    }

    // Returns the first line number of this chunk mod 15.
    static constexpr int64_t FirstLineNumberMod15() {
      if constexpr (BATCH_ID == 0) {
        return DIGITS > 1 ? 10 : 1;
      } else {
        return (PreviousBatch::FirstLineNumberMod15() +
          PreviousBatch::LinesInBatch()) % 15;
      }
    }

    // Returns the number of lines in this batch.
    static constexpr int64_t LinesInBatch() {
      return std::min(kMaxLinesInBatch, LinesInRun());
    }

    // Returns the size of this batch in bytes.
    static constexpr int64_t BytesInBatch() {
      if constexpr (LinesInBatch() < kMaxLinesInBatch) {
        return RunBytes();
      } else {
        size_t size = LinesInBatch() / 15 * FifteenerBytes();
        for (int64_t i = FirstLineNumberMod15() + LinesInBatch() / 15 * 15;
             i < FirstLineNumberMod15() + LinesInBatch(); ++i) {
          size += FizzBuzzLineLength(i);
        }
        return size;
      }
    }

    void Output() {
      output_handler_->Output(buffer_id_, BytesInBatch());
    }

    char* GetBuffer() {
      return output_handler_->GetBuffer(buffer_id_);
    }

    OutputHandler* output_handler_;
    // The buffer id that this batch should use in |output_handler_|.
    int buffer_id_;
  };

  // Represents a chunk, a part of batch processed by thread with id
  // |THREAD_ID|. THREAD_ID ∈ [0, kParallelism)
  // Since numbers in each chunk need to be incremented at different indexes,
  // we specialize this class for each BATCH_ID and THREAD_ID so the indexes can
  // be precomputed at compile time.
  template<int BATCH_ID, int THREAD_ID>
  class Chunk {
    using PreviousChunk = Chunk<BATCH_ID, THREAD_ID - 1>;

    public:
    // Initializes a chunk that resides in |batch|.
    Chunk(Batch<BATCH_ID> batch) : batch_(batch) {}

    // Returns the first line number of this chunk mod 15.
    static constexpr int64_t FirstLineNumberMod15() {
      if constexpr (THREAD_ID == 0) {
        return Batch<BATCH_ID>::FirstLineNumberMod15();
      } else {
        return (PreviousChunk::FirstLineNumberMod15() +
          PreviousChunk::LinesInChunk()) % 15;
      }
    }

    // Returns the index of the start byte of this chunk in the batch.
    static constexpr int64_t StartIndexInBatch() {
      if constexpr (THREAD_ID == 0) {
        return 0;
      } else {
        return PreviousChunk::StartIndexInBatch() +
          PreviousChunk::BytesInChunk();
      }
    }

    // Returns the number of lines in this chunk.
    static constexpr int64_t LinesInChunk() {
      int64_t done = THREAD_ID == 0 ? 0 :
        PreviousChunk::CumulativeLinesUpToChunk();
      int64_t remaining_lines = Batch<BATCH_ID>::LinesInBatch() - done;
      int64_t remaining_threads = kParallelism - THREAD_ID;
      // equivalent to ceil(remaining_lines / remaining_threads)
      return (remaining_lines - 1) / remaining_threads + 1;
    }

    // Returns the number of lines in this and all previous chunks in the batch.
    static constexpr int64_t CumulativeLinesUpToChunk() {
      if constexpr (THREAD_ID < 0) {
        return 0;
      } else {
        return PreviousChunk::CumulativeLinesUpToChunk() + LinesInChunk();
      }
    }

    // Returns the length of this chunk in bytes.
    static constexpr int64_t BytesInChunk() {
      size_t size = LinesInChunk() / 15 * FifteenerBytes();
      for (int64_t i = FirstLineNumberMod15() + LinesInChunk() / 15 * 15;
           i < FirstLineNumberMod15() + LinesInChunk(); ++i) {
        size += FizzBuzzLineLength(i);
      }
      return size;
    }

    // Increments all the numbers in the chunk.
    // This function wraps IncrementNumbersImpl for efficiently dispatching to
    // specialized versions based on |prefix|.
    void IncrementNumbers(int64_t prefix) {
      // If DIGITS < kSuffixDigits, it means that all the numbers within a run
      // will fit into a single batch, so we should not use IncrementNumbers().
      // The below implementation would not even work.
      static_assert(DIGITS >= kSuffixDigits);
      constexpr int64_t max_overflow_digits = DIGITS - kSuffixDigits;
      // Contains an IncrementChunkImpl() specialization for each value in
      // 0..max_overflow_digits. We use it to jump to the right specialization.
      constexpr auto increment_chunk_impls = []() {
        std::array<void (*)(char*), max_overflow_digits + 1> res{};
        [&]<size_t... OVERFLOW_DIGITS>(std::index_sequence<OVERFLOW_DIGITS...>) {
          ((res[OVERFLOW_DIGITS] = &IncrementNumbersImpl<OVERFLOW_DIGITS>), ...);
        }(std::make_index_sequence<max_overflow_digits + 1>());
        return res;
      }();

      increment_chunk_impls[OverflowDigits(prefix)](batch_.GetBuffer());
    }

    private:
    // Increments this chunk in |batch|.
    //
    // Each number line is incremented by |kIncrementBy| * 10^kSuffixDigits.
    // If OVERFLOW_DIGITS > 0, we assume that the operation will overflow,
    // therefore, we need to increment this many digits beforehand. It's the
    // caller's responsibility to calculate the number of digits that will need
    // to be updated in this chunk.

    // For example, the chunk if kIncrementBy = 3 and kSuffixDigits = 6, the
    // chunk [100000000, 100999999] can be incremented to [103000000; 103999999]
    // with OVERFLOW_DIGITS = 0 (no overflow).
    // When incrementing [108000000, 108999999] to [111000000; 111999999],
    // OVERFLOW_DIGITS = 1 (one-digit overflow).
    // When incrementing [198000000, 198999999] to [201000000, 201999999],
    // OVERFLOW_DIGITS = 2 (two-digit overflow)
    template<int OVERFLOW_DIGITS>
    static void IncrementNumbersImpl(char* batch) {
      char* out = batch;
      constexpr int64_t start_index = StartIndexInBatch();
      constexpr int first_line_number_mod_15 = FirstLineNumberMod15();

      // Increments the |num_lines| starting from |out|.
      // |num_lines| must be divisible by 120 (except in the last iteration).
      auto increment = [&] (int num_lines) __attribute__((always_inline)) {
        int line_start = 0;
        #pragma GCC unroll 120
        for (int64_t line = 0; line < num_lines; ++line) {
          if (IsFizzBuzzNumber(first_line_number_mod_15 + line)) {
            // In order for the compiler to generate efficient code, the
            // second and third params should be deducible to constants.
            // Since the loop is unrolled, the value of |line_start| is
            // known in every iteration. |start_index| is constexpr, so
            // its value is also known.
            IncrementNumber(out, line_start + start_index, OVERFLOW_DIGITS);
          }
          line_start +=
              FizzBuzzLineLength((first_line_number_mod_15 + line) % 15);
        }
        // Since num_lines is a multiply of 120, the right hand side is a
        // multiply of 8 which ensures that |out| is aligned to 8 bytes
        // afterwards.
        out += FifteenerBytes() * num_lines / 15;
      };

      for (int64_t i = 0; i < LinesInChunk() / 120; ++i) {
        increment(120);
      }
      increment(LinesInChunk() % 120);
    }

    // Returns whether this number is printed as-is ie. it's not a multiply of 3
    // or 5.
    static constexpr bool IsFizzBuzzNumber(int64_t number) {
      return number % 3 != 0 && number % 5 != 0;
    }

    // Increments the number starting at base[line_start].
    // |base| must be aligned to 8 bytes. The caller must guarantee that the
    // number of overflows that occur is |overflow_digits|.
    // For maximum performance, |line_start| should be deducible at compile
    // time.
    __attribute__((always_inline))
    static inline void IncrementNumber(char* base,
                                       int64_t line_start,
                                       int overflow_digits) {
      int64_t right_most_digit_to_update_index =
          line_start + DIGITS - 1 - kSuffixDigits;
      // When overflow_digits is known at compile time, all the IncrementAt
      // calls that affect the same 8-byte integer are combined into 1
      // instruction by the compiler.
      IncrementAt(base, right_most_digit_to_update_index, kIncrementBy);
      #pragma GCC unroll 100
      for (int i = 0; i < overflow_digits; ++i) {
        IncrementAt(base, right_most_digit_to_update_index, -10);
        IncrementAt(base, right_most_digit_to_update_index - 1, 1);
        right_most_digit_to_update_index--;
      }
    }

    // Increments the byte at |index| in |base| by |by|.
    // |base| must by aligned to 8 bytes.
    // For maximum performance, |index| and |by| should be deducible by the
    // compiler to constants.
    __attribute__((always_inline))
    static inline void IncrementAt(char* base, int64_t index, char by) {
      union char_array_int64 {
        char ch[8];
        int64_t int64;
      };
      auto base_as_union = reinterpret_cast<char_array_int64*>(base);
      // The code below only works on little endian systems.
      static_assert(std::endian::native == std::endian::little);
      // Increment the character at index |index| by |by|. This works because
      // we can guarantee that the character won't overflow.
      base_as_union[index / 8].int64 +=
        static_cast<int64_t>(by) << ((index % 8) * 8);
    }

    // Returns the number of digits that will overflow when incrementing
    // |prefix| by |kIncrementBy|.
    // Eg. if kIncrementBy = 3:
    // OverflowDigits(100) = 0 (no digits overflow)
    // OverflowDigits(108) = 1 (8 overflows and 0 is incremented by 1)
    // OverflowDigits(198) = 2 (8 overflows and 9 overflows)
    static int OverflowDigits(int64_t prefix) {
      int incremented = prefix + kIncrementBy;
      #pragma GCC unroll 2
      for (int i = 0; i < 20; ++i) {
        incremented /= 10;
        prefix /= 10;
        if (incremented == prefix) {
          return i;
        }
      }
      return 20;
    }

    Batch<BATCH_ID> batch_;
  };
};

} // namespace

int main() {
  OutputHandler output_handler;

  [&]<std::size_t... I>(std::index_sequence<I...>){
    (Run<I + 1>::Execute(output_handler), ...);
  }(std::make_index_sequence<18>());

  return 0;
}
