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

    static StringBuilder fizzBuzz(long src, StringBuilder destination) {
        if (src % 15 == 0) {
            destination.append("fizzbuzz");
        } else if (src % 5 == 0) {
            destination.append("fizz");
        } else if (src % 3 == 0) {
            destination.append("buzz");
        } else {
            destination.append(src);
        }
        return destination.append(NEWLINE);
    }

    record Pair<T1, T2>(T1 item1, T2 item2) {
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

        final Queue<CompletableFuture<Pair<StringBuilder, ByteBuffer>>> queue =
                new LinkedList<>
                        (IntStream.range(0, Optional.of(ForkJoinPool.getCommonPoolParallelism() - 1).filter(i -> i > 0).orElse(1))
                                .mapToObj(i ->
                                        CompletableFuture.completedFuture(new Pair<>(new StringBuilder(20 * 1024 * 1024), ByteBuffer.allocateDirect(41 * 1024 * 1024)))).toList());

        CompletableFuture<?> last = CompletableFuture.completedFuture(null);

        final Supplier<Pair<StringBuilder, ByteBuffer>> supplier = () -> {
            try {
                return queue.poll().get();
            } catch (Exception ex) {
                ex.printStackTrace();
                System.exit(1);
                return null;
            }
        };


        var nr = 0L;
        var segment = 1_000_000L;

        while (true) {
            final var start = nr;
            final var end = nr + segment;


            final var finalLast = last;

            var cf = CompletableFuture.completedFuture(supplier.get())
                    .thenApplyAsync(p -> {
                        var sb = p.item1();
                        var bb = p.item2();
                        sb.delete(0, sb.length());
                        for (long l = start; l < end; l++) {
                            fizzBuzz(l, sb);
                        }
                        bb.clear();
                        bb.put(sb.toString().getBytes(StandardCharsets.UTF_8));
                        return p;
                    })
                    .thenCombineAsync(finalLast, (p, v) -> {
                        try {
                            channel.write(p.item2().flip());
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
