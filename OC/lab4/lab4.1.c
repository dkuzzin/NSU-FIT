#include <stdio.h>
#include <stdlib.h>

#define EXIT_SUCCESS 0
#define MAX_ADDRESS_SIZE 1024

int global_init = 100;
int global_uninit;
const int global_const = 100;


void print_region_for_address(void *address) {
    FILE *maps_file = fopen("/proc/self/maps", "r");
    if (maps_file == NULL) {
        perror("[in print_region_for_address function] fopen error");
        return;
    }

    char line[MAX_ADDRESS_SIZE];
    unsigned long start;
    unsigned long end;
    unsigned long addr = (unsigned long)address;

    while (fgets(line, sizeof(line), maps_file) != NULL) {
        if (sscanf(line, "%lx-%lx", &start, &end) != 2) {
            continue;
        }

        if (start <= addr && addr < end) {
            printf("%s\n", line);
            fclose(maps_file);
            return;
        }
    }

    fclose(maps_file);
    printf("Unknown region: %p\n", address);
}


void print_addresses(void){
    int local = 10;
    static int local_static = 100;
    const int local_const = 100;
    
    printf("Локальные переменнные\n");

    printf("    local: %p\n", (void *)&local);
    print_region_for_address((void *)&local);

    printf("    local_static: %p\n", (void *)&local_static);
    print_region_for_address((void *)&local_static);

    printf("    local_const: %p\n", (void *)&local_const);
    print_region_for_address((void *)&local_const);

    printf("Глобальные переменные\n");

    printf("    global_init: %p\n", (void *)&global_init);
    print_region_for_address((void *)&global_init);

    printf("    global_uninit: %p\n", (void *)&global_uninit);
    print_region_for_address((void *)&global_uninit);

    printf("    global_const: %p\n", (void *)&global_const);
    print_region_for_address((void *)&global_const);

}


int main(){
    print_addresses();
    return EXIT_SUCCESS;
}