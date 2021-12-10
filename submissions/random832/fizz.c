// sample outputs [space is null]
//                    1\n
// Fizz                \n
//     Buzz            \n
// FizzBuzz            \n
// 18446744073709551614\n ULLONG_MAX-1 [ULLONG_MAX is FizzBuzz]
#define LEN 21
//#define NUM (200*15)
#define NUM (200*15)

#if defined(_POSIX_C_SOURCE)
#include <unistd.h>
#define WRITE(b,l) write(1,b,l)
#elif defined(_MSC_VER)
#include <io.h>
#define WRITE(b,l) write(1,b,l)
#else
#include <stdio.h>
#define WRITE(b,l) fwrite(b,1,l,stdout)
#endif

char buf[NUM*LEN] = {0}; // 63000
char buf2[NUM*LEN];
int flags[NUM];

int main() {
    // pre-populate buffer
    for(int i=0; i<NUM; i++) {
        char *t = buf+i*LEN;
        int f = flags[i] = i%3==0 | ((i%5==0)<<1);
        if(f&1) { t[0]='F'; t[1]='i'; t[2]='z'; t[3]='z';}
        if(f&2) { t[4]='B'; t[5]='u'; t[6]='z'; t[7]='z';}
        t[LEN-1] = '\n';
    }
    int j=1; // position of current output within buffer
    for(unsigned long long i=1;;i++) {
        if(!flags[j]) {
                char *t = buf+j*LEN+(LEN-2); // position of last digit
                unsigned long long k = i;
                while(k) {
                    *t-- = '0'+k%10;
                    k/=10;
                }
        }
        if(++j == NUM) {
            j=0;
            char *p=buf;
            char *q=buf2;
            if(i == NUM-1) p+=LEN; // skip zero, there's probably a better way
            while(p < buf+NUM*LEN) {
                if(*p) *q++=*p;
                p++;
            }
            WRITE(buf2, q-buf2);
        }
    }
}
