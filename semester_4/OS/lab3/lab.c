#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define MAX_PATH 4096
#define ERR -1
#define IS_DIRECTORY 1
#define DIR_PERMS (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#define CURRENT_DIR "."
#define PARENT_DIR ".."
#define EXPECTED_ARGS_NO_TARGET 2
#define EXPECTED_ARGS_TARGET 3
#define SUCCES 0
#define EQUAL 0



void reverse_string(const char* str, char* result){
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++){
        result[i] = str[len - 1 - i];
    }
    result[len] = '\0';
}


int reverse_file_data(long size, FILE* source, const char* source_path, FILE* dest){
    const long BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    long bytes_read = 0;
    int returned_fseek;

    for (long pos = size; pos > 0; pos -= bytes_read){
        long chunk_size = (pos >= BUFFER_SIZE) ? BUFFER_SIZE : pos;
        long read_pos = pos - chunk_size;

        returned_fseek = fseek(source, read_pos, SEEK_SET);
        if (returned_fseek == ERR){
            fprintf(stderr, "Error positioning in file %s\n", source_path);
            return ERR;
        }

        bytes_read = fread(buffer, 1, chunk_size, source);
        if (bytes_read != chunk_size && ferror(source) != SUCCES){
            fprintf(stderr, "Error reading file: read %ld instead of %ld\n", bytes_read, chunk_size);
            return ERR;
        }

        for (long i = 0; i < bytes_read / 2; i++){
            char tmp = buffer[i];
            buffer[i] = buffer[bytes_read - 1 - i];
            buffer[bytes_read - 1 - i] = tmp;
        }

        long bytes_written = fwrite(buffer, 1, bytes_read, dest);
        if (bytes_written != bytes_read){
            fprintf(stderr, "Write error: wrote %ld bytes instead of %ld\n", bytes_written, bytes_read);
            return ERR;
        }
    }

    return SUCCES;
}

int reverse_text(const char* source_path, const char* dest_path){
    FILE* source = fopen(source_path, "rb");
    if (source == NULL){
        perror("[in reverse_text] Error opening source file");
        return ERR;
    }

    FILE* dest = fopen(dest_path, "wb");
    if (dest == NULL){
        perror("[reverse_text] Error creating target file");
        fclose(source);
        return ERR;
    }

    int returned_fseek = fseek(source, 0, SEEK_END);
    if (returned_fseek == ERR){
        fprintf(stderr, "Error positioning in file %s\n", source_path);
        fclose(source);
        fclose(dest);
        return ERR;
    }

    long size = ftell(source);
    if (size == ERR){
        perror("[reverse_text] Error getting file size");
        fclose(source);
        fclose(dest);
        return ERR;
    }

    
    int reverse_res = reverse_file_data(size, source, source_path, dest);
    if (reverse_res == ERR){
        fclose(source);
        fclose(dest);
        return ERR;
    }
    
    fclose(source);
    fclose(dest);
    return SUCCES;
}


void get_last_path_component(const char* path, char* result){
    const char* last_slash = strrchr(path, '/');

    if (last_slash == NULL){
        strcpy(result, path);
        return;
    }

    if (*(last_slash + 1) == '\0'){
        size_t len = strlen(path);
        while (len > 0 && path[len - 1] == '/'){
            len--;
        }

        char trimmed[MAX_PATH];
        if (len >= MAX_PATH){
            len = MAX_PATH - 1;
        }

        strncpy(trimmed, path, len);
        trimmed[len] = '\0';

        last_slash = strrchr(trimmed, '/');
        if (last_slash == NULL){
            strcpy(result, trimmed);
        } else{
            strcpy(result, last_slash + 1);
        }
        return;
    }

    strcpy(result, last_slash + 1);
}


int build_target_dir_path(const char* source_dir, const char* target_parent_dir, char* target_dir_path){
    char source_dir_name[MAX_PATH];
    char reversed_dir_name[MAX_PATH];

    get_last_path_component(source_dir, source_dir_name);
    reverse_string(source_dir_name, reversed_dir_name);

    int written = snprintf(target_dir_path, MAX_PATH, "%s/%s", target_parent_dir, reversed_dir_name);
    if (written == ERR) {
        perror("Error in build_target_path (func snprintf)");
        return ERR;
    }
    if (written >= MAX_PATH){
        fprintf(stderr, "Error: target dir path is too long\n");
        return ERR;
    }

    return SUCCES;
}



int reverse_file(struct dirent* entry, const char* target_dir_path, const char* source_path){
    struct stat statbuf;
    char dest_path[MAX_PATH];

    int lstat_result = lstat(source_path, &statbuf);
    if (lstat_result == ERR){
        perror("[reverse_file] Error getting file info");
        return ERR;
    }

    if (!S_ISREG(statbuf.st_mode)){
        return SUCCES;
    }

    char reversed_filename[MAX_PATH];
    reverse_string(entry->d_name, reversed_filename);

    int written = snprintf(dest_path, MAX_PATH, "%s/%s", target_dir_path, reversed_filename);
    
    if (written == ERR) {
        perror("Error in reverse_file (func snprintf)");
        return ERR;
    }
    if (written >= MAX_PATH){
        fprintf(stderr, "Error: path %s/%s is too long\n", target_dir_path, reversed_filename);
        return ERR;
    }
    
    printf("Copying %s -> %s\n", source_path, dest_path);
    
    int reverse_result = reverse_text(source_path, dest_path);
    if (reverse_result == ERR){
        fprintf(stderr, "Error copying file %s\n", entry->d_name);
        return ERR;
    }

    return SUCCES;
}


int read_directory(DIR* dir, const char* source_dir, const char* target_dir_path){
    char source_path[MAX_PATH];
    struct dirent* entry;
    
    
    while ((entry = readdir(dir)) != NULL){
        if (strcmp(entry->d_name, CURRENT_DIR) == EQUAL || strcmp(entry->d_name, PARENT_DIR) == EQUAL){
            continue;
        }

        int written = snprintf(source_path, MAX_PATH, "%s/%s", source_dir, entry->d_name);
        if (written == ERR) {
            perror("Error in reverse_file (func snprintf)");
            return ERR;
        }
        if (written >= MAX_PATH){
            fprintf(stderr, "Error: file path is too long: %s/%s\n", source_dir, entry->d_name);
            continue;
        }

        int reverse_result = reverse_file(entry, target_dir_path, source_path);
        if (reverse_result == ERR){
            return ERR;
        }
    }

    return SUCCES;
}



int process_directory(const char* source_dir, const char* target_parent_dir){
    DIR* dir;
    char target_dir_path[MAX_PATH];
    struct stat statbuf;
    
    int lstat_result = lstat(source_dir, &statbuf);
    if (lstat_result == ERR){
        perror("[in process_directory] Error getting source directory info");
        return ERR;
    }

    if (!S_ISDIR(statbuf.st_mode)){
        fprintf(stderr, "Error: %s is not a directory\n", source_dir);
        return ERR;
    }

    int build_result = build_target_dir_path(source_dir, target_parent_dir, target_dir_path);
    if (build_result == ERR){
        return ERR;
    }

    dir = opendir(source_dir);
    if (dir == NULL){
        perror("[process_directory] Error opening directory");
        return ERR;
    }

    int mkdir_result = mkdir(target_dir_path, DIR_PERMS);
    if (mkdir_result == ERR){
        if (errno != EEXIST){
            perror("[in process_directory] Error creating target directory");
            closedir(dir);
            return ERR;
        }
    }

    printf("Created directory: %s\n", target_dir_path);

    int read_dir_res = read_directory(dir, source_dir, target_dir_path);
    if (read_dir_res == ERR){
        closedir(dir);
        return ERR;
    }
    
    closedir(dir);
    return SUCCES;
}


int main(int argc, char* argv[]){
    int process_result;
    if (argc != EXPECTED_ARGS_TARGET){
        if (argc != EXPECTED_ARGS_NO_TARGET){
            fprintf(stderr, "Usage: %s <source_directory> <target_parent_directory>\n", argv[0]);
            return EXIT_FAILURE; 
        }
        process_result = process_directory(argv[1], CURRENT_DIR);
    }else{
        process_result = process_directory(argv[1], argv[2]);
    }
    
    if (process_result == ERR){
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}