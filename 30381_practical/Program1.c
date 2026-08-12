#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int num, i;
    long long fact = 1;

    if (argc != 2) {
        printf("Usage: ./factorial <number>\n");
        return 1;
    }

    num = atoi(argv[1]);

    for (i = 1; i <= num; i++)
        fact *= i;

    printf("\n--- Child Process (exec) ---\n");
    printf("Factorial of %d = %lld\n", num, fact);

    return 0;
}
