#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "file_ops.h"

void create_file(const char *filename)
{
    int fd = open(filename, O_CREAT | O_WRONLY, 0644);

    if (fd == -1)
    {
        perror("Error creating file");
        return;
    }

    printf("File created successfully!\n");
    printf("File descriptor: %d\n", fd);

    close(fd);
}

void read_file(const char *filename)
{
    int fd = open(filename, O_RDONLY);

    if (fd == -1)
    {
        perror("Error opening file");
        return;
    }

    char buffer[1024];
    ssize_t bytes_read;

    printf("\n--- File Contents ---\n");

    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    if (bytes_read == -1)
    {
        perror("Error reading file");
    }

    printf("\n---------------------\n");

    close(fd);
}

void write_file(const char *filename)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (fd == -1)
    {
        perror("Error opening file");
        return;
    }

    char buffer[1024];

    printf("Enter content to write: ");
    getchar();
    fgets(buffer, sizeof(buffer), stdin);

    if (write(fd, buffer, strlen(buffer)) == -1)
    {
        perror("Error writing to file");
    }
    else
    {
        printf("Content written successfully!\n");
    }

    close(fd);
}

void update_file(const char *filename)
{
    int fd = open(filename, O_WRONLY | O_TRUNC);

    if (fd == -1)
    {
        perror("Error opening file");
        return;
    }

    char buffer[1024];

    printf("Enter updated content: ");
    getchar();
    fgets(buffer, sizeof(buffer), stdin);

    if (write(fd, buffer, strlen(buffer)) == -1)
    {
        perror("Error updating file");
    }
    else
    {
        printf("File updated successfully!\n");
    }

    close(fd);
}

void delete_file(const char *filename)
{
    if (unlink(filename) == -1)
    {
        perror("Error deleting file");
        return;
    }

    printf("File deleted successfully!\n");
}

void show_file_info(const char *filename)
{
    struct stat file_info;

    if (stat(filename, &file_info) == -1)
    {
        perror("Error getting file information");
        return;
    }

    printf("\n--- File/Directory Information ---\n");
    printf("Name: %s\n", filename);
    printf("Size: %ld bytes\n", file_info.st_size);

    if (S_ISREG(file_info.st_mode))
    {
        printf("Type: Regular File\n");
    }
    else if (S_ISDIR(file_info.st_mode))
    {
        printf("Type: Directory\n");
    }
    else
    {
        printf("Type: Other\n");
    }

    printf("Permissions: %o\n", file_info.st_mode & 0777);
    printf("----------------------------------\n");
}
