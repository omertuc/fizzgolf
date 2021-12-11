public class FizzBuzz {

    public static void main(String[] args) {

        long maxBufLen=4096<<3;

        var sb=new StringBuilder((int)maxBufLen + 10);
        for (long i = 1; ; i+=1) {

            if ((i % 3 == 0) && (i % 5 == 0)) {
                sb.append("FizzBuzz\n");
            } else if (i % 3 == 0) {
                sb.append("Fizz\n");
            } else if (i % 5 == 0) {
                sb.append("Buzz\n");
            } else {
                sb.append(i).append("\n");
            }
            
            if(sb.length()>maxBufLen) {
                System.out.print(sb);
                sb.setLength(0);
            }
        }
        
    }

} 
