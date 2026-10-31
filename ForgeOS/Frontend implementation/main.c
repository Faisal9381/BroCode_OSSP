#include <stdio.h>
#include <unistd.h>

#include "file_ops.h"
#include "dir_ops.h"

int main()
{
    int choice;
    char path[256];

    while (1)
    {
        printf("\n");
        printf("=============================================\n");
        printf("       LINUX FILE SYSTEM EXPLORER\n");
        printf("=============================================\n");

        printf("1.  List Directory\n");
        printf("2.  Create File\n");
        printf("3.  Read File\n");
        printf("4.  Write File\n");
        printf("5.  Update File\n");
        printf("6.  Delete File\n");
        printf("7.  Create Directory\n");
        printf("8.  Remove Directory\n");
        printf("9.  File/Directory Information\n");
        printf("10. Show Current Directory\n");
        printf("11. Exit\n");

        printf("=============================================\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice)
        {
            case 1:
                printf("Enter directory path: ");
                scanf("%255s", path);
                list_directory(path);
                break;

            case 2:
                printf("Enter file name: ");
                scanf("%255s", path);
                create_file(path);
                break;

            case 3:
                printf("Enter file name: ");
                scanf("%255s", path);
                read_file(path);
                break;

            case 4:
                printf("Enter file name: ");
                scanf("%255s", path);
                write_file(path);
                break;

            case 5:
                printf("Enter file name: ");
                scanf("%255s", path);
                update_file(path);
                break;

            case 6:
                printf("Enter file name: ");
                scanf("%255s", path);
                delete_file(path);
                break;

            case 7:
                printf("Enter directory name: ");
                scanf("%255s", path);
                create_directory(path);
                break;

            case 8:
                printf("Enter directory name: ");
                scanf("%255s", path);
                remove_directory(path);
                break;

            case 9:
                printf("Enter file/directory path: ");
                scanf("%255s", path);
                show_file_info(path);
                break;

            case 10:
                if (getcwd(path, sizeof(path)) != NULL)
                {
                    printf("Current Directory: %s\n", path);
                }
                else
                {
                    perror("Error getting current directory");
                }
                break;

            case 11:
                printf("Exiting Linux File System Explorer...\n");
                return 0;

            default:
                printf("Invalid choice! Please try again.\n");
        }
    }

    return 0;
}
