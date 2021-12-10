import java.io.FileDescriptor;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.channels.FileChannel;
import java.nio.charset.CharsetEncoder;
import java.nio.charset.StandardCharsets;
import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ForkJoinPool;
import java.util.function.Supplier;
import java.util.stream.IntStream;

public class FizzBuzzIoan {

    static final String NEWLINE = String.format("%n");
    static final CharsetEncoder encoder = StandardCharsets.US_ASCII.newEncoder();

    static void fizzBuzz(long src, StringBuilder destination) {
        
        if (src % 15 == 0) {
            destination.append("FizzBuzz"+NEWLINE);
        } else if (src % 5 == 0) {
            destination.append("Fizz"+NEWLINE);
        } else if (src % 3 == 0) {
            destination.append("Buzz"+NEWLINE);
        } else {
            destination.append(src).append(NEWLINE);
        }

    }

    record Pair<T1, T2>(T1 item1, T2 item2) {
    }

    public static void main(String[] args) throws Exception {

        try (var fis= new FileOutputStream(FileDescriptor.out)) {
            var channel = fis.getChannel();
            doIt(channel);
        }
    }

    static void doIt(FileChannel channel) {

        var max=Math.max(ForkJoinPool.getCommonPoolParallelism(), 1);
        
        final Queue<CompletableFuture<Pair<StringBuilder, ByteBuffer>>> queue =
                new LinkedList<>(
                        IntStream.range(0, max).mapToObj(i ->
                            CompletableFuture.completedFuture(
                                    new Pair<>(new StringBuilder(20 * 1024 * 1024), ByteBuffer.allocateDirect(41 * 1024 * 1024)))).toList()
                        );

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
                        
                        sb.setLength(0);
                        
                        for (long l = start; l < end; l++) {
                            fizzBuzz(l, sb);
                        }
                        bb.clear();
                        
                        try {
                            var cb=CharBuffer.wrap(sb);
                            encoder.encode(cb, bb, true);

                        } catch(Exception ex) {
                            throw new RuntimeException(ex);
                        }
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
