#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
void myhandler(int signo) {
 printf("\nSignal Recieved\n");
 if(signo == SIGSEGV){
   printf("\nhandling Segmentation fault\n");
   printf("\nSignal Number = %d", signo);
}
if(signo == SIGINT){
 printf("\nhandling Segmentation fault\n");
 printf("\nSignal Number = %d", signo);
 }
 exit(1);
 }
int main(){
 signal(SIGSEGV, myhandler);
 signal(SIGINT, myhandler);
 int x, *p = NULL;
 printf("Enter the number");
 scanf("%d", &x);
 p = &x;
 raise(SIGINT);
 p = (int *)100;
 printf("The entered number is: %d\n", x);
 printf("address of x is: %u\n", &x);
 printf("The entered number is: %d \n", *p);
 printf("value o p: %u\n", p);
 printf("address of p is: %u\n", &p);
 return 0;
}
