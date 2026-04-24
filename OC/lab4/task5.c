#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 100
#define EXIT_SUCCES 0
void heap_demo(void) {
    char *buffer1 = (char *)malloc(BUFFER_SIZE);
    if (buffer1 == NULL) {
        perror("malloc");
        return;
    }

    strcpy(buffer1, "hello world");
    printf("1) buffer1 after write: %s\n", buffer1);

    free(buffer1);
    printf("2) buffer1 after free: %s\n", buffer1);

    char *buffer2 = (char *)malloc(BUFFER_SIZE);
    if (buffer2 == NULL) {
        perror("malloc");
        return;
    }

    strcpy(buffer2, "hello world");
    printf("3) buffer2 after write: %s\n", buffer2);

    char *middle = buffer2 + BUFFER_SIZE / 2;
    free(middle);

    printf("4) buffer2 after free(middle): %s\n", buffer2);

    free(buffer2);
}

int main(void) {
    heap_demo();
    return EXIT_SUCCES;
}