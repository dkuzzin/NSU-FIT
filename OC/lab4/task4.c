#include <stdio.h>
#define EXIT_SUCCES 0

int *get_local_address(void) {
    int local = 42;
    return &local;
}

int main(void) {
    int *ptr = get_local_address();

    printf("ptr = %p\n", (void *)ptr);
    printf("*ptr = %d\n", *ptr);
    printf("Hello");

    return EXIT_SUCCES;
}