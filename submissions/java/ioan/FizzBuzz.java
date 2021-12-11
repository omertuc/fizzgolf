import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.LinkedList;
import java.util.Optional;
import java.util.Queue;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ForkJoinPool;
import java.util.function.Consumer;
import java.util.function.Supplier;
import java.util.stream.IntStream;

public class FizzBuzz {

    static final String NEWLINE = String.format("%n");

    static byte[] fizzbuzz = "fizzbuzz\n".getBytes(StandardCharsets.ISO_8859_1);
    static byte[] fizz = "fizz\n".getBytes(StandardCharsets.ISO_8859_1);
    static byte[] buzz = "buzz\n".getBytes(StandardCharsets.ISO_8859_1);
    static byte newline = "\n".getBytes(StandardCharsets.ISO_8859_1)[0];
    static byte zero = (byte) '0';

    static int fizzBuzz(long src, byte[] destination, int destinationPos, byte[] digitsHelper, int digitsHelperLast) {
        if (src % 15 == 0) {
            System.arraycopy(fizzbuzz, 0, destination, destinationPos, fizzbuzz.length);
            return destinationPos + fizzbuzz.length;
        } else if (src % 5 == 0) {
            System.arraycopy(fizz, 0, destination, destinationPos, fizz.length);
            return destinationPos + fizz.length;
        } else if (src % 3 == 0) {
            System.arraycopy(buzz, 0, destination, destinationPos, buzz.length);
            return destinationPos + buzz.length;
        } else {
            do {
                digitsHelper[digitsHelperLast--] = (byte) (zero + src % 10);
                src /= 10;
            } while (src != 0);
            int len = digitsHelper.length - digitsHelperLast;
            System.arraycopy(digitsHelper, digitsHelperLast, destination, destinationPos, len);
            return destinationPos + len;
        }

    }

    record Pair(byte[] item1, ByteBuffer item2) {
    }


    public static void main(String[] argv) throws Exception {

        final var channel = new FileOutputStream(FileDescriptor.out).getChannel();

        final Consumer<ByteBuffer> writeToChannel = bb -> {
            try {
                while (bb.hasRemaining()) {
                    channel.write(bb);
                }
            } catch (Exception ex) {
                ex.printStackTrace();
                System.exit(1);
            }
        };

        final var segment = 10_000_000;
        final var arralloc = 20 * segment;

        final Queue<CompletableFuture<Pair>> queue =
                new LinkedList<>
                        (IntStream.range(0, Optional.of(ForkJoinPool.getCommonPoolParallelism() - 1).filter(i -> i > 0).orElse(1))
                                .mapToObj(i ->
                                        CompletableFuture.completedFuture(new Pair(new byte[arralloc], ByteBuffer.allocateDirect(arralloc)))).toList());

        CompletableFuture<?> last = CompletableFuture.completedFuture(null);

        final Supplier<Pair> supplier = () -> {
            try {
                return queue.poll().get();
            } catch (Exception ex) {
                ex.printStackTrace();
                System.exit(1);
                return null;
            }
        };


        var nr = 0L;


        while (true) {
            final var start = nr;
            final var end = nr + segment;


            final var finalLast = last;

            var cf = CompletableFuture.completedFuture(supplier.get())
                    .thenApplyAsync(p -> {
                        var arr = p.item1();
                        var bb = p.item2();
                        var pos = 0;
                        var digitsHelper = new byte[20];
                        digitsHelper[19] = newline;

                        for (long l = start; l < end; l++) {
                            pos = fizzBuzz(l, arr, pos, digitsHelper, 18);
                        }
                        bb.clear();
                        bb.put(arr, 0, pos);
                        bb.flip();
                        return p;
                    })
                    .thenCombineAsync(finalLast, (p, v) -> {
                        try {
                            channel.write(p.item2());
                        } catch (Exception ex) {
                            ex.printStackTrace();
                            System.exit(1);
                        }
                        return p;
                    });

            queue.add(cf);
            last = cf;

            nr = end;


        }

    }

}
