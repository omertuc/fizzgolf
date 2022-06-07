import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;

import static java.nio.charset.StandardCharsets.US_ASCII;

public final class FizzBuzz {

  private static final int BUFFER_SIZE = 1 << 16;
  private static final long STARTING_NUMBER = 10L;
  private static final long SIZE_FOR_30 = 2 * (17 * 8 + 47);
  private static final long FULL_INCREMENT = BUFFER_SIZE / SIZE_FOR_30 * 30;
  private static final long LIGHT_INCREMENT = FULL_INCREMENT - 30;
  private static final int[] BASE_IDX = {-1, 6, 8, 19, 21, 28, 40, 42, 54, 61, 63, 74, 76, 83, 95, 97};
  private static final int[][] IDX = new int[19][16];

  static {
    IDX[1] = BASE_IDX;
    for (var i = 2; i < IDX.length; i++) {
      for (var j = 0; j < 16; ) {
        IDX[i][j] = IDX[i - 1][j] + ++j;
      }
    }
  }


  public static void main(String[] args) {
    var threads = Runtime.getRuntime().availableProcessors();
    var queue = new LinkedBlockingQueue<Future<ByteBuffer>>(threads);
    var executor = Executors.newFixedThreadPool(threads);
    try (var outputStream = new FileOutputStream(FileDescriptor.out);
         var channel = outputStream.getChannel()) {

      var counter = STARTING_NUMBER;
      for (var i = 0; i < threads; i++) {
        var buffer = ByteBuffer.allocateDirect(BUFFER_SIZE);
        queue.offer(executor.submit(new Task(counter, buffer)));
        counter += FULL_INCREMENT;
      }
      channel.write(ByteBuffer.wrap("1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nBuzz\nFizz\n".getBytes(US_ASCII)));
      while (true) {
        var buffer = queue.poll().get();
        channel.write(buffer);
        queue.offer(executor.submit(new Task(counter, buffer)));
        counter += FULL_INCREMENT;
      }
    } catch (Exception e) {
      e.printStackTrace(System.err);
    } finally {
      executor.shutdown();
    }
  }

  record Task(long startingNumber, ByteBuffer buffer) implements Callable<ByteBuffer> {

    @Override
    public ByteBuffer call() {
      buffer.clear();
      var n = startingNumber;
      var nextPowerOf10 = 10L;
      var digitCount = 1;
      for (; nextPowerOf10 <= n; nextPowerOf10 *= 10L) {
        digitCount++;
      }
      var idx = IDX[digitCount];
      var t = n / 10L;
      var s = Long.toString(t);
      var template = (s + "1\nFizz\n" +
        s + "3\n" +
        s + "4\nFizzBuzz\n" +
        s + "6\n" +
        s + "7\nFizz\n" +
        s + "9\nBuzz\nFizz\n" +
        (s = Long.toString(++t)) + "2\n" +
        s + "3\nFizz\nBuzz\n" +
        s + "6\nFizz\n" +
        s + "8\n" +
        s + "9\nFizzBuzz\n" +
        (s = Long.toString(++t)) + "1\n" +
        s + "2\nFizz\n" +
        s + "4\nBuzz\nFizz\n" +
        s + "7\n" +
        s + "8\nFizz\nBuzz\n").getBytes(US_ASCII);
      n += 30;
      buffer.put(template);

      for (var limit = n + LIGHT_INCREMENT; n < limit; n += 30L) {
        if (n == nextPowerOf10) {
          nextPowerOf10 *= 10;
          digitCount++;
          idx = IDX[digitCount];
          t = n / 10L;
          s = Long.toString(t);
          template = (s + "1\nFizz\n" +
            s + "3\n" +
            s + "4\nFizzBuzz\n" +
            s + "6\n" +
            s + "7\nFizz\n" +
            s + "9\nBuzz\nFizz\n" +
            (s = Long.toString(++t)) + "2\n" +
            s + "3\nFizz\nBuzz\n" +
            s + "6\nFizz\n" +
            s + "8\n" +
            s + "9\nFizzBuzz\n" +
            (s = Long.toString(++t)) + "1\n" +
            s + "2\nFizz\n" +
            s + "4\nBuzz\nFizz\n" +
            s + "7\n" +
            s + "8\nFizz\nBuzz\n").getBytes(US_ASCII);
        } else {
          for (var i = 0; i < idx.length; ) {
            var pos = idx[i++];
            template[pos] += 3;
            while (template[pos] > '9') {
              template[pos] -= 10;
              template[--pos]++;
            }
          }
        }
        buffer.put(template);
      }
      return buffer.flip();
    }
  }

}

/*
 * Speed references:
 *   - 270 GiB / min ± 3.95 / min on Macbook Pro 2021
 */
