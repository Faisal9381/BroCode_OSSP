#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "file_ops.h"
#include "dir_ops.h"

static void print_escaped_json_string(const char *str)
{
    putchar('"');
    for (const char *p = str; *p; p++)
    {
        switch (*p)
        {
            case '"': printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\b': printf("\\b"); break;
            case '\f': printf("\\f"); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:
                if ((unsigned char)*p < 0x20)
                    printf("\\u%04x", (unsigned char)*p);
                else
                    putchar(*p);
                break;
        }
    }
    putchar('"');
}

void list_directory_json(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir)
    {
        fprintf(stderr, "Error: Cannot open directory '%s'\n", path);
        exit(1);
    }

    printf("[");
    struct dirent *entry;
    int first = 1;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char full_path[4096];
        if (strcmp(path, "/") == 0)
        {
            snprintf(full_path, sizeof(full_path), "/%s", entry->d_name);
        }
        else
        {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        }

        struct stat st;
        int has_stat = (stat(full_path, &st) == 0);

        if (!first)
        {
            printf(",");
        }
        first = 0;

        printf("{\"name\":");
        print_escaped_json_string(entry->d_name);
        printf(",\"path\":");
        print_escaped_json_string(full_path);

        int is_dir = 0;
        long long size = 0;
        int mode = 0;

        if (has_stat)
        {
            is_dir = S_ISDIR(st.st_mode);
            size = (long long)st.st_size;
            mode = (int)(st.st_mode & 0777);
        }
        else
        {
            is_dir = (entry->d_type == DT_DIR);
        }

        printf(",\"isDirectory\":%s", is_dir ? "true" : "false");
        printf(",\"size\":%lld", size);
        printf(",\"permissions\":\"%03o\"", mode);
        printf("}");
    }

    closedir(dir);
    printf("]\n");
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <command> [args...]\\n", argv[0]);
        fprintf(stderr, "Commands:\\n");
        fprintf(stderr, "  list-json <path>\\n");
        fprintf(stderr, "  list <path>\\n");
        fprintf(stderr, "  create-file <path>\\n");
        fprintf(stderr, "  read-file <path>\\n");
        fprintf(stderr, "  update-file <path>\\n");
        fprintf(stderr, "  create-dir <path>\\n");
        fprintf(stderr, "  delete-file <path>\\n");
        fprintf(stderr, "  delete-dir <path>\\n");
        fprintf(stderr, "  delete <path>\\n");
        fprintf(stderr, "  rename <oldpath> <newpath>\\n");
        fprintf(stderr, "  info <path>\\n");
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "list-json") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        list_directory_json(argv[2]);
        return 0;
    }
    else if (strcmp(cmd, "list") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        list_directory(argv[2]);
        return 0;
    }
    else if (strcmp(cmd, "create-file") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        create_file(argv[2]);
        return 0;
    }
    else if (strcmp(cmd, "read-file") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        read_file(argv[2]);
        return 0;
    }
    else if (strcmp(cmd, "update-file") == 0 || strcmp(cmd, "write-file") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        int fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            perror("Error opening file for writing");
            return 1;
        }
        char buf[4096];
        ssize_t bytes_read;
        while ((bytes_read = read(STDIN_FILENO, buf, sizeof(buf))) > 0)
        {
            if (write(fd, buf, bytes_read) == -1)
            {
                perror("Error writing to file");
                close(fd);
                return 1;
            }
        }
        close(fd);
        printf("File updated successfully!\n");
        return 0;
    }
    else if (strcmp(cmd, "create-dir") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        create_directory(argv[2]);
        return 0;
    }
    else if (strcmp(cmd, "delete-file") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        delete_file(argv[2]);
        return 0;
    }
    else if (strcmp(cmd, "delete-dir") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        remove_directory(argv[2]);
        return 0;
    }
    else if (strcmp(cmd, "delete") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        struct stat st;
        if (stat(argv[2], &st) == 0 && S_ISDIR(st.st_mode))
        {
            remove_directory(argv[2]);
        }
        else
        {
            delete_file(argv[2]);
        }
        return 0;
    }
    else if (strcmp(cmd, "rename") == 0)
    {
        if (argc < 4) { fprintf(stderr, "Missing old or new path\n"); return 1; }
        if (rename(argv[2], argv[3]) == -1)
        {
            perror("Error renaming");
            return 1;
        }
        printf("Renamed successfully from '%s' to '%s'\n", argv[2], argv[3]);
        return 0;
    }
    else if (strcmp(cmd, "info") == 0)
    {
        if (argc < 3) { fprintf(stderr, "Missing path\n"); return 1; }
        show_file_info(argv[2]);
        return 0;
    }
    else
    {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return 1;
    }

    return 0;
}
