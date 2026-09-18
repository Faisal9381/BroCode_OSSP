#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void myhandler(int signo){
    printf("\nsignal recieved\n");
    printf("signal no:%d\n", signo);
    exit(1);
    }
    
int main(){
   signal(SIGSEGV, myhandler);
       printf("handling interupts\n");
       printf("\n");
   return 0;
}
