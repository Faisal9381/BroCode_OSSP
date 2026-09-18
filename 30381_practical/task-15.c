#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main()
{
    int pipefd[2];
    pid_t producer, consumer;

    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    producer = fork();

    if (producer == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (producer == 0)
    {
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        close(pipefd[1]);

        execlp("ls", "ls", "-l", (char *)NULL);

        perror("execlp");
        exit(EXIT_FAILURE);
    }

    consumer = fork();

    if (consumer == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (consumer == 0)
    {
        close(pipefd[1]);

        if (dup2(pipefd[0], STDIN_FILENO) == -1)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        close(pipefd[0]);

        execlp("grep", "grep", ".c", (char *)NULL);

        perror("execlp");
        exit(EXIT_FAILURE);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(producer, NULL, 0);
    waitpid(consumer, NULL, 0);

    return 0;
}
