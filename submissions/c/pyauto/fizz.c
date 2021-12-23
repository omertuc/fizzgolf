#include <stdio.h> 
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define SIZE (4096 << 4)

int main(void) {
  
  char buffer[SIZE] = {};

  char *buf = buffer;

  _Bool d3;
  _Bool d5;
  for (int n=1; 1; n++) {
    if ((buf - buffer) >= SIZE-10) {
      write(1, buffer, SIZE);
      buf = buffer;
    }

    d3 = (n%3==0);
    d5 = (n%5==0);

    if (d3 && d5) {
      strcpy(buf, "FizzBuzz\n");
      buf += 9;  
    }
    else if (d3) {
      strcpy(buf, "Fizz\n");
      buf += 5;  
    }
    else if (d5) {
      strcpy(buf, "Buzz\n");
      buf += 5;
    } else {
      buf += sprintf(buf,"%d\n",n);
    }
  }
  return 0;
}
