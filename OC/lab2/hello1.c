#include <unistd.h>
#include <string.h>
#define WRITERR -1

int main(){
    const char message[] = "Hello, world\n";
    const size_t length = strlen(message);
    
    ssize_t written_bytes = write(STDOUT_FILENO, message, length);
    
    if (written_bytes == WRITERR){
        return WRITERR;
    }
    
    return 0;
}
