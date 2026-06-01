#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#define WRITERR -1

ssize_t write_wrapper(int fd, const void *buf, size_t count){
    return syscall(SYS_write, fd, buf, count);
}

int main(){
    const char message[] = "Hello, world\n";
    const size_t length = strlen(message);

    ssize_t written_bytes = write_wrapper(STDOUT_FILENO, message, length);
    
    if (written_bytes == WRITERR){
        return WRITERR;
    }
    
    return 0;
}
