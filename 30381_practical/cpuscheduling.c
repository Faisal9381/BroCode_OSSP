#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct cpu {
    int pid;
    int at;
    int bt;
    int priority;
    int ct;
    int tat;
    int wt;
    int ret;
    bool proc_status;
} Process;


void display(Process *p, int n) {
    printf("\nProcess data is:\n");
    printf("\nPID\tAT\tBT\tPRI\n");

    for (int i = 0; i < n; i++) {
        printf("P%d\t%d\t%d\t%d\n",
               p[i].pid,
               p[i].at,
               p[i].bt,
               p[i].priority);
    }
}


void scheduling_display(Process *p, int n) {
    int sum_tat = 0;
    int sum_wt = 0;

    printf("\nScheduling data is:\n");
    printf("\nPID\tAT\tBT\tPRI\tCT\tTAT\tWT\n");

    for (int i = 0; i < n; i++) {

        sum_tat += p[i].tat;
        sum_wt += p[i].wt;

        printf("P%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
               p[i].pid,
               p[i].at,
               p[i].bt,
               p[i].priority,
               p[i].ct,
               p[i].tat,
               p[i].wt);
    }

    printf("\nAverage TAT: %.2f\n", (float)sum_tat / n);
    printf("Average WT : %.2f\n", (float)sum_wt / n);
}


void Fcfs(Process p[], int n) {

    // Sort according to Arrival Time
    for (int i = 0; i < n - 1; i++) {

        for (int j = 0; j < n - i - 1; j++) {

            if (p[j].at > p[j + 1].at) {

                Process temp = p[j];
                p[j] = p[j + 1];
                p[j + 1] = temp;
            }
        }
    }


    int cpu_time = 0;

    for (int i = 0; i < n; i++) {

        // CPU is idle until process arrives
        if (cpu_time < p[i].at) {
            cpu_time = p[i].at;
        }

        cpu_time += p[i].bt;

        p[i].ct = cpu_time;

        p[i].tat = p[i].ct - p[i].at;

        p[i].wt = p[i].tat - p[i].bt;
    }

    scheduling_display(p, n);
}


int main() {

    int choice;
    int n;

    Process p[10];

    printf("Enter number of processes: ");
    scanf("%d", &n);


    // Input
    for (int i = 0; i < n; i++) {

        p[i].pid = i + 1;

        printf("\nEnter AT of P%d: ", i + 1);
        scanf("%d", &p[i].at);

        printf("Enter BT of P%d: ", i + 1);
        scanf("%d", &p[i].bt);

        printf("Enter Priority of P%d: ", i + 1);
        scanf("%d", &p[i].priority);

        p[i].ct = 0;
        p[i].tat = 0;
        p[i].wt = 0;
        p[i].ret = 0;
        p[i].proc_status = false;
    }


    display(p, n);


    while (1) {

        printf("\n\nCPU Scheduling Algorithms\n");
        printf("1. FCFS\n");
        printf("2. SJF\n");
        printf("3. SRTF\n");
        printf("4. Priority Non-Preemptive\n");
        printf("5. Priority Preemptive\n");
        printf("6. Round Robin\n");
        printf("7. Exit\n");

        printf("\nSelect one of the CPU Scheduling Algorithms: ");
        scanf("%d", &choice);


        switch (choice) {

            case 1:
                Fcfs(p, n);
                break;

            case 2:
                printf("Operations of SJF\n");
                break;

            case 3:
                printf("Operations of SRTF\n");
                break;

            case 4:
                printf("Operations of Priority Non-Preemptive\n");
                break;

            case 5:
                printf("Operations of Priority Preemptive\n");
                break;

            case 6:
                printf("Operations of Round Robin\n");
                break;

            case 7:
                exit(0);

            default:
                printf("Invalid choice!\n");
        }
    }

    return 0;
}
