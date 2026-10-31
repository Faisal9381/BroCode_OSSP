#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dir_ops.h"

void list_directory(const char *path)
{
    DIR *directory;
    struct dirent *entry;

    directory = opendir(path);

    if (directory == NULL)
    {
        perror("Error opening directory");
        return;
    }

    printf("\n========== Directory Contents ==========\n");

    while ((entry = readdir(directory)) != NULL)
    {
        printf("%s\n", entry->d_name);
    }

    printf("========================================\n");

    closedir(directory);
}

void create_directory(const char *path)
{
    if (mkdir(path, 0755) == -1)
    {
        perror("Error creating directory");
        return;
    }

    printf("Directory '%s' created successfully.\n", path);
}

void remove_directory(const char *path)
{
    if (rmdir(path) == -1)
    {
        perror("Error removing directory");
        return;
    }

    printf("Directory '%s' removed successfully.\n", path);
}
