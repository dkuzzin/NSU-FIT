#include <stdio.h>
#include <stdlib.h>

#define VAR_NAME "MY_VAR"
#define OWERWRITE_FLAG 1
#define ERR -1

int main(void) {
    char *value = getenv(VAR_NAME);
    if (value == NULL) {
        printf("%s is not set\n", VAR_NAME);
    } else {
        printf("Before change: %s=%s\n", VAR_NAME, value);
    }

    if (setenv(VAR_NAME, "BYE", OWERWRITE_FLAG) == ERR) {
        perror("setenv");
        return EXIT_FAILURE;
    }

    value = getenv(VAR_NAME);
    if (value == NULL) {
        printf("After change: %s is not set\n", VAR_NAME);
    } else {
        printf("After change: %s=%s\n", VAR_NAME, value);
    }

    return EXIT_SUCCESS;
}