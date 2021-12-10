import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

import static java.nio.charset.StandardCharsets.US_ASCII;

public class FizzBuzz {
  public static void main(String[] args) throws IOException {
    try (var out = Files.newOutputStream(Paths.get("/proc/self/fd/1"))) {
      var capacity = 1 << 16;
      var limit = capacity;
      var buffer = new byte[capacity];

      int size;
      var init = "1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nFizz\nBuzz\n".getBytes();
      System.arraycopy(init, 0, buffer, 0, size = init.length);

      var template = new byte[0];
      var idx = new int[] { -1, 6, 8, 19, 21, 28, 40, 42, 54, 61, 63, 74, 76, 83, 95, 97 };

      for (long n = 10, nextPow10 = 10; ; size = 0) {
        for (; size < limit; n += 30) {
          if (n == nextPow10) {
            nextPow10 *= 10;
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
                (n + 28) + "\nFizz\nBuzz\n").getBytes();
            limit = capacity - template.length;
            for (var i = 0; i < idx.length; ) {
              idx[i] += ++i;
            }
          } else {
            for (var i = 0; i < idx.length; i++) {
              var pos = idx[i];
              template[pos] += 3;
              while (template[pos] > '9') {
                template[pos] -= 10;
                template[--pos]++;
              }
            }
          }
          System.arraycopy(template, 0, buffer, size, template.length);
          size += template.length;
        }
        out.write(buffer, 0, size);
      }
    }
  }
}
