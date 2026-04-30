#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#define ERR -1
#define CHILD_PID 0
#define EXIT_CODE 5
#define SUCCESS 0
#define TIME_FOR_SLEEP 30


int global_var = 10;

#define BUF_SIZE 1024
#define CHILD_OUTPUT_PATH "child_maps.txt"
#define PARENT_OUTPUT_PATH "parent_maps.txt"


void save_maps(const char *filename) {
    FILE *src = fopen("/proc/self/maps", "r");
    if (src == NULL) {
        perror("fopen /proc/self/maps");
        return;
    }

    FILE *dest = fopen(filename, "w");
    if (dest == NULL) {
        perror("fopen output");
        fclose(src);
        return;
    }

    char line[BUF_SIZE];

    while (fgets(line, sizeof(line), src) != NULL) {
        if (fputs(line, dest) == EOF) {
            perror("fputs");
            break;
        }
    }

    if (ferror(src)) {
        perror("fgets");
    }

    fclose(src);
    fclose(dest);
}

int main(void) {
    int local_var = 20;

    printf("Before fork:\n");
    printf("    pid = %d\n", getpid());
    printf("    global_var: addr = %p, value = %d\n", (void*)&global_var, global_var);
    printf("    local_var:  addr = %p, value = %d\n", (void*)&local_var, local_var);

    pid_t child_pid = fork();

    if (child_pid == ERR) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    
    if (child_pid == CHILD_PID) {
        printf("\nChild process:\n");
        printf("    pid = %d, parent pid = %d\n", getpid(), getppid());

        printf("    Before change:\n");
        printf("        global_var: addr = %p, value = %d\n", (void*)&global_var, global_var);
        printf("        local_var:  addr = %p, value = %d\n", (void*)&local_var, local_var);

        global_var = 100;
        local_var = 200;

        printf("    After change:\n");
        printf("        global_var: addr = %p, value = %d\n", (void*)&global_var, global_var);
        printf("        local_var:  addr = %p, value = %d\n", (void*)&local_var, local_var);
        save_maps(CHILD_OUTPUT_PATH);
        exit(EXIT_CODE);
    }else{
        printf("\nParent process:\n");
        printf("    pid = %d, child pid = %d\n", getpid(), child_pid);

    
        printf("        ps -o pid,ppid,state -p %d,%d\n", getpid(), child_pid);

        save_maps(PARENT_OUTPUT_PATH);
        
        sleep(TIME_FOR_SLEEP);
    
        int status;
        pid_t ended_pid = wait(&status);
        if (ended_pid == ERR) {
            perror("wait");
            exit(EXIT_FAILURE);
        }

        printf("\nChild finished:\n");
        printf("    ended pid = %d\n", ended_pid);

        if (WIFEXITED(status)) {
            printf("    reason: NORMAL\n");
            printf("    exit code = %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("    reason: KILLED BY SIGNAL\n");
            printf("    signal = %d\n", WTERMSIG(status));
        } else {
            printf("    reason: UNKNOWN\n");
        }

        printf("\n\nParent variables after child change:\n");
        printf("        global_var: addr = %p, value = %d\n", (void*)&global_var, global_var);
        printf("        local_var:  addr = %p, value = %d\n", (void*)&local_var, local_var);

        return SUCCESS;
    }
}