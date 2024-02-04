#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <omp.h>
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

// The last kSuffixDigits digits of each number line are untouched when
// iterating.
constexpr int64_t kSuffixDigits = 5;
// Increment the first right-most touched digit by this much in one step. Must
// be divisible by 6. The code currently only handles when this is single-digit.
constexpr int64_t kIncrementBy = 6;
constexpr int64_t kLinesInChunk = PowTen(kSuffixDigits);
constexpr int64_t kChunksInHalfBatch = kIncrementBy / 2;
constexpr int64_t kMaxLinesInHalfBatch = kLinesInChunk * kChunksInHalfBatch;

constexpr int kFizzLength = 4;
constexpr int kBuzzLength = 4;
constexpr int kNewlineLength = 1;

// Implements a double-buffering mechanism for efficient output.
// We work with two buffers, a output buffer and an update buffer.
// GetUpdateBuffer() can be written to. Afterwards, upon calling Output(),
// the buffer is copied or moved to the standard output. Following Output(),
// GetUpdateBuffer() returns the second buffer.
// The class ensures that GetUpdateBuffer() can always be updated (without
// a race condition).
// The current limitation is that Output must be invoked with monotonically
// increasing sizes.
class OutputHandler {
 static constexpr size_t kBufferSize = 4 * 1024 * 1024;

 public:
  OutputHandler() {
    output_buffer =
        static_cast<char*>(aligned_alloc(2 * 1024 * 1024, kBufferSize));
    update_buffer =
        static_cast<char*>(aligned_alloc(2 * 1024 * 1024, kBufferSize));
    madvise(output_buffer, kBufferSize, MADV_HUGEPAGE);
    madvise(update_buffer, kBufferSize, MADV_HUGEPAGE);
  }

  ~OutputHandler() {
    free(output_buffer);
    free(update_buffer);
  }

  // Swaps the buffers and outputs |bytes| number of bytes from
  // |GetUpdateBuffer()|. Must be invoked with monotonically increasing sizes.
  void Output(size_t bytes) {
    std::swap(output_buffer, update_buffer);
    // We use double-buffering: we put |output_buffer| into the pipe and allow
    // the program to update |update_buffer|. We have to ensure that no part of
    // |update_buffer| is in the pipe while updating it. Afaik there is no API
    // to track that, so we have to make the pipe size smaller than bytes to
    // ensure that when OutputWithVmSplice returns, the pipe only contains data
    // from |output_buffer|, therefore it's safe to update |update_buffer|.
    // If the pipe is too small, the program will be slower. The optimum is
    // calculated by TargetPipeSize. Since there is a minimum pipe size which we
    // cannot go below (4kb on Linux), this approach won't work when the
    // size is too small. In these cases we fall back to write() which copies
    // the content into the pipe, therefore there is no risk of overwriting
    // memory that is still being read from the downstream process.
    // However, if in the subsequent call to Output(), a smaller size were
    // passed (and therefore the else branch were executed), the pipe could
    // still end up containing some data from the current iteration and the
    // entire data from the next iteration. We assume that Output() will be
    // invoked with monotonically increasing sizes (which is true in practice
    // but it'd be better not to depend on this assumption).
    SetPipeSize(TargetPipeSize(bytes));
    if (bytes >= pipe_size_) {
      OutputWithVmSplice(bytes);
    } else {
      if (write(STDOUT_FILENO, output_buffer, bytes) < 0) {
        std::cerr << "write error: " << errno;
        std::abort();
      }
    }
  }

  // Calculates the optimal pipe size for outputting |out_bytes|.
  size_t TargetPipeSize(size_t out_bytes) {
    // Pipe sizes must be powers of 2 and >= 4kb on Linux.
    // We want that the entire output fills up the pipe (but still maximize
    // the pipe size), so we round |out_bytes| down to the nearest power or two.
    return std::max(4ul * 1024, std::bit_floor(out_bytes));
  }

  void OutputWithVmSplice(size_t bytes) {
    iovec iov;
    iov.iov_base = output_buffer;
    iov.iov_len = bytes;
    while (true) {
      int64_t ret = vmsplice(STDOUT_FILENO, &iov, 1, SPLICE_F_NONBLOCK);
      if (ret >= 0) {
        iov.iov_len -= ret;
        iov.iov_base = static_cast<char*>(iov.iov_base) + ret;
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

  char* GetUpdateBuffer() {
    return update_buffer;
  }

 private:
  char* output_buffer;
  char* update_buffer;
  size_t pipe_size_ = 0;
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

// Returns whether this number is printed as-is ie. it's not a multiply of 3
// or 5.
constexpr bool IsFizzBuzzNumber(int64_t number) {
  return number % 3 != 0 && number % 5 != 0;
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

  // One fifteener takes this many bytes.
  static constexpr size_t FifteenerBytes() {
    size_t size = 0;
    for (int i = 0; i < 15; ++i) {
      size += FizzBuzzLineLength(i);
    }
    return size;
  }

  // The entire fizz-buzz output for this run takes this many lines.
  static constexpr int64_t RunLines() {
    return PowTen(DIGITS) - PowTen(DIGITS - 1);
  }

  // The entire fizz-buzz output for this run takes this many bytes.
  static constexpr size_t RunBytes() {
    if constexpr(DIGITS == 1)
    {
      return 5 + 3 * kFizzLength + 1 * kBuzzLength + 9 * kNewlineLength;
    } else {
      return RunLines() / 15 * FifteenerBytes();
    }
  }

  // One half-batch needs this many bytes. If the run does not fill a full
  // half-batch, returns the run size.
  static constexpr size_t HalfBatchBytes() {
    if constexpr (LinesInHalfBatch() < kMaxLinesInHalfBatch) {
      return RunBytes();
    } else {
       static_assert(LinesInHalfBatch() % 15 == 0);
       return LinesInHalfBatch() / 15 * FifteenerBytes();
    }
  }

  static constexpr int64_t LinesInHalfBatch() {
    return std::min(kMaxLinesInHalfBatch, RunLines());
  }

  static constexpr int64_t HalfBatchesInRun() {
    return RunLines() / LinesInHalfBatch();
  }

  // Fills |out| with the nth half-batch (0-indexed). This is a relatively
  // slow operation so we do this only at the beginning of each run.
  static char* InitHalfBatch(char* out, int64_t n) {
    int64_t start = PowTen(DIGITS - 1) + n * LinesInHalfBatch();
    int64_t end = std::min(PowTen(DIGITS), start + LinesInHalfBatch());
    for (int64_t line = start; line < end; ++line) {
      out = InsertFizzBuzzLine(out, line);
    }
    return out;
  }

 public:
  // Executes this run by using the given |output_handler|.
  // The code outputs all lines for this run, using the double-buffering
  // mechanism of |output_handler|.
  static void Execute(OutputHandler& output_handler) {
    InitHalfBatch(output_handler.GetUpdateBuffer(), 0);
    output_handler.Output(HalfBatchBytes());
    if constexpr (HalfBatchBytes() < RunBytes()) {
      static_assert(RunBytes() % HalfBatchBytes() == 0);
      InitHalfBatch(output_handler.GetUpdateBuffer(), 1);
      output_handler.Output(HalfBatchBytes());

      int64_t prefix = PowTen(DIGITS - kSuffixDigits - 1);

      for (int64_t half_batch = 2; half_batch < HalfBatchesInRun();
           ++half_batch) {

        // This part of the code assumes that kChunksInHalfBatch == 3, if this
        // changes, this part needs to be rewritten.
        static_assert(kChunksInHalfBatch == 3);

        // We update the 3 chunks of the half batch in 3 threads. For this to
        // be efficient, each chunk has to be "big enough", otherwise the
        // threading overhead outweighs the performance gains we get from
        // parallelism.
        #pragma omp parallel num_threads(3)
        {
          switch (omp_get_thread_num()) {
            case 0:
              Chunk<0>(output_handler.GetUpdateBuffer(), prefix)
                .IncrementNumbers();
              break;
            case 1:
              Chunk<1>(output_handler.GetUpdateBuffer(), prefix + 1)
                .IncrementNumbers();
              break;
            case 2:
              Chunk<2>(output_handler.GetUpdateBuffer(), prefix + 2)
                .IncrementNumbers();
              break;
          }
        }

        output_handler.Output(HalfBatchBytes());
        prefix += kChunksInHalfBatch;
      }
    }
  }

  // Represents a chunk within this run with chunk id |CHUNK_ID|.
  // |CHUNK_ID| ∈ [0, kChunksInHalfBatch)
  // Since numbers in each chunk need to be incremented at different indexes,
  // we specialize this class so the indexes can be precomputed at compile time.
  // Each chunk represents a section of the output belonging to line numbers
  // 10^kSuffixDigits
  template<int CHUNK_ID>
  class Chunk {
    static_assert(CHUNK_ID < kChunksInHalfBatch);

    public:
    // Initializes a chunk that resides in the half-batch pointed to by
    // and has prefix |prefix|.
    Chunk(char* half_batch, int64_t prefix) : half_batch_(half_batch),
                                              prefix_(prefix) {}

    // Returns the index of the beginning of this chunk within a half-batch.
    static constexpr int64_t FirstLineNumberMod15() {
      return (PowTen(DIGITS - 1) + CHUNK_ID * kLinesInChunk) % 15;
    }

    // Length of this chunk in bytes.
    static constexpr int64_t ChunkBytes() {
      size_t size = kLinesInChunk / 15 * FifteenerBytes();
      for (int64_t i = FirstLineNumberMod15() + kLinesInChunk / 15 * 15;
           i < FirstLineNumberMod15() + kLinesInChunk; ++i) {
        size += FizzBuzzLineLength(i);
      }
      return size;
    }

    // Returns the index of the beginning of this chunk within a half-batch.
    static constexpr int64_t StartIndexInHalfBatch() {
      if constexpr (CHUNK_ID == 0) {
        return 0;
      } else {
        return Chunk<CHUNK_ID - 1>::StartIndexInHalfBatch() +
               Chunk<CHUNK_ID - 1>::ChunkBytes();
      }
    }

    // Increments all the numbers in the chunk.
    // |half_batch| must be aligned to 8 bytes.
    // This function wraps IncrementNumbersImpl for efficiently dispatching to
    // specialized versions based on |prefix|.
    void IncrementNumbers() {
      // If DIGITS < kSuffixDigits, it means that all the numbers within a run
      // will fit into a single half-batch, so we should not use
      // IncrementNumbers(). The below implementation would not even work.
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

      increment_chunk_impls[OverflowDigits(prefix_)](half_batch_);
    }

    private:
    // Increments this chunk in the half-batch starting at |half_batch|.
    // |half_batch| must be aligned to 8 bytes.
    //
    // Each number line is incremented by |kIncrementBy| * 10^kSuffixDigits.
    // If OVERFLOW_DIGITS > 0, we assume that the increment will be > 9,
    // therefore, we need to increment this many digits beforehand. It's the
    // caller's responsibility to calculate the number of digits that will need
    // to be updated in this chunk.

    // For example, the chunk if kIncrementBy = 6 and kSuffixDigits = 5, the
    // chunk [10000000, 10099999] can be incremented to [10600000; 10699999]
    // with OVERFLOW_DIGITS = 0 (no overflow).
    // When incrementing [10800000, 10899999] to [11400000; 11499999],
    // OVERFLOW_DIGITS = 1 (one-digit overflow).
    // When incrementing [19800000, 19899999] to [20400000, 20499999],
    // OVERFLOW_DIGITS = 2 (two-digit overflow)
    template<int OVERFLOW_DIGITS>
    static void IncrementNumbersImpl(char* half_batch) {
      char* out = half_batch;
      constexpr int64_t start_index = StartIndexInHalfBatch();
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

      #pragma GCC unroll 1
      for (int64_t i = 0; i < kLinesInChunk / 120; ++i) {
        increment(120);
      }
      increment(kLinesInChunk % 120);
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

    // Increments the byte at |index| in |chunk_start| by |by|.
    // |base| must by aligned to 8 bytes.
    // For maximum performance, |index| and |by| should be deducible by the
    // compiler to constants.
    __attribute__((always_inline))
    static inline void IncrementAt(char* base, int64_t index, char by) {
      // The code below only works on little endian systems.
      static_assert(std::endian::native == std::endian::little);
      *(((int64_t * )(base)) + (index / 8)) += ((int64_t) by) << ((index % 8) * 8);
    }

    // Returns the number of digits that will overflow when incrementing
    // |prefix_| by |kIncrementBy|.
    // Eg. if kIncrementBy = 6:
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

    char* half_batch_;
    int64_t prefix_;
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
