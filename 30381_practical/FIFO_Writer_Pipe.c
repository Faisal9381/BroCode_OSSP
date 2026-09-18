#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
int main(){
     char *path="/tmp/Writerpipe_fifo";
     mkfifo(path, 0666);
     char msg[] =" Hello Receiver ";
     printf("Waiting for reader connection\n");
     int fd=open("/tmp/Writerpipe_fifo", O_WRONLY);
     write(fd, msg, strlen(msg));
     printf("writer sent a message\n");
     close(fd);
     return 0;
}
