#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
int main(){
    char bfr[100];
    printf("Receiving Message from writer\n");
    int fd=open("/tmp/Writerpipe_fifo", O_RDONLY);
    read(fd,bfr,sizeof(bfr));
    close(fd);
    printf("Received Message: %s", bfr);
    return 0;
}
