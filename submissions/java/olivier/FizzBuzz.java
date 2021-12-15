import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.concurrent.Callable;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.stream.IntStream;

import static java.nio.charset.StandardCharsets.US_ASCII;

public final class FizzBuzz {

  private static final long STARTING_NUMBER = 10L;
  private static final long INCREMENT = 5_400L;
  private static final long SIZE_FOR_30 = 2 * (17 * 8 + 47);
  private static final int[] BASE_IDX = {-1, 6, 8, 19, 21, 28, 40, 42, 54, 61, 63, 74, 76, 83, 95, 97};

  public static void main(String[] args) {
    new FizzBuzz().run();
  }

  private void run() {

    var threads = Runtime.getRuntime().availableProcessors();
    var queue = new LinkedBlockingQueue<Future<ByteBuffer>>(threads);
    var executor = Executors.newFixedThreadPool(threads);
    try (var channel = new FileOutputStream(FileDescriptor.out).getChannel()) {

      var counter = STARTING_NUMBER;
      System.err.println(INCREMENT * SIZE_FOR_30 / 30L);
      for (var buffer : IntStream.range(0, threads).mapToObj(x -> ByteBuffer.allocateDirect((int) (INCREMENT * SIZE_FOR_30 / 30L))).toList()) {
        queue.offer(executor.submit(new Task(counter, buffer)));
        counter += INCREMENT;
      }
      channel.write(ByteBuffer.wrap("1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nBuzz\nFizz\n".getBytes(US_ASCII)));
      while (true) {
        var buffer = queue.poll().get();
        buffer.flip();
        channel.write(buffer);
        buffer.clear();
        queue.offer(executor.submit(new Task(counter, buffer)));
        counter += INCREMENT;
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
      var n = startingNumber;
      var idx = Arrays.copyOf(BASE_IDX, BASE_IDX.length);
      var nextPowerOf10 = 10L;
      for (; nextPowerOf10 < n; nextPowerOf10 *= 10L) {
        for (var i = 0; i < idx.length; ) {
          idx[i] += ++i;
        }
      }
      byte[] template = null;

      for (var limit = n + INCREMENT; n < limit; n += 30) {
        if (template == null | n == nextPowerOf10) {
          if (n == nextPowerOf10) {
            nextPowerOf10 *= 10;
            for (var i = 0; i < idx.length; ) {
              idx[i] += ++i;
            }
          }
          template = ((n + 1) + "\nFizz\n" +
              (n + 3) + "\n" +
              (n + 4) + "\nFizzBuzz\n" +
              (n + 6) + "\n" +
              (n + 7) + "\nFizz\n" +
              (n + 9) + "\nBuzz\nFizz\n" +
              (n + 12) + "\n" +
              (n + 13) + "\nFizz\nBuzz\n" +
              (n + 16) + "\nFizz\n" +
              (n + 18) + "\n" +
              (n + 19) + "\nFizzBuzz\n" +
              (n + 21) + "\n" +
              (n + 22) + "\nFizz\n" +
              (n + 24) + "\nBuzz\nFizz\n" +
              (n + 27) + "\n" +
              (n + 28) + "\nFizz\nBuzz\n").getBytes(US_ASCII);
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
      return buffer;
    }
  }

}
