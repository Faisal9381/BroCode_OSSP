#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
int num;
pid_t pid;
char str[20];
printf("Enter a number: ");
scanf("%d", &num);

pid = fork();

if (pid < 0) {
    printf("Fork Failed\n");
    exit(1);
}

if (pid == 0) {
    // Child Process
    sprintf(str, "%d", num);

execl("./FC", "FC", str, NULL);

        // Executes only if execl fails
perror("execl failed");
}
else {
     // Parent Process
    wait(NULL);
   printf("\n--- Parent Process ---\n");
   printf("Square of %d = %d\n", num, num * num);
}

    return 0;
}
