#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>

int main()
{
    int pid, num;

    printf("Enter num: ");
    scanf("%d", &num);

    pid = fork();

    if (pid < 0)
    {
        printf("Fail to create new process\n");
    }
    else if (pid == 0)
    {
        // Child process calculates factorial
        unsigned int fact = 1;
        int temp = num;

        while (temp > 0)
        {
            fact = fact * temp;
            temp--;
        }

        printf("Factorial of %d is %u\n", num, fact);
    }
    else
    {
        // Parent process calculates square
        int sqr;

        sqr = num * num;

        printf("Square of %d is %d\n", num, sqr);

        wait(NULL);
    }

    return 0;
}
