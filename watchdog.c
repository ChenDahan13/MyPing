#include <stdio.h>
#include <sys/time.h>


int main() {
   struct timeval start, end;
   long time;
   gettimeofday(&start, NULL);
   while(1) {
        gettimeofday(&end, NULL);
        time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
        if(time >= 10000) {
            break;
        }
   }
   printf("Time was passed\n");
   return 0;
}