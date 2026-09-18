#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SM_NAME "shmfile"
#define NUM_MARKS 5

int main(){
     key_t key;
     int shmid;
     int *marks;
     int sum=0;
     float average;
     
     /* Generate the same key */
     key=ftok(SM_NAME, 65);
     
     if(key==-1){
         perror("ftock");
         exit(1);
     }
     /*get the existing shared memory*/
     shmid=shmget(key, NUM_MARKS*sizeof(int), 0666);
     
     if(shmid==-1){
        perror("shmget");
        exit(1);
     }
     /*Attach shared memory*/
     marks=(int*)shmat(shmid, NULL,0);
     
     if(marks==(int*)-1){
         perror("shmat");
         exit(1);
     }
     
     printf("Consumer Process\n");
     printf("------------------\n");
     printf("Student marks read from shared memory:\n");
     
     for(int i=0;i<NUM_MARKS;i++){
          printf("%d ",marks[i]);
          sum+=marks[i];
     }
     
     printf("\n");
     
     /*Calculate average*/
     average=(float)sum/NUM_MARKS;
     
     printf("Total Marks=%d\n", sum);
     printf("Average=%.1f\n",average);
     
     /*Detach shared Memory*/
     shmdt(marks);
     /*remove shared memory*/
     shmctl(shmid, IPC_RMID,NULL);
     
     return 0;
}
     
