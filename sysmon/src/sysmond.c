#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

void print_file(const char* path) {
    FILE* file = fopen(path, "r");

    if (file == NULL) {
        perror(path);
        return;
    }

    char line[256];

    while (fgets(line, sizeof(line), file) != NULL) {
        printf("%s", line);
    }

    if (ferror(file)) {
        perror("read file");
    }

    fclose(file);
}

void get_data(void) {
    printf("Memory data:\n");
    print_file("/tmp/sysmon/mem.dat");

    printf("\nCPU load averages:\n");
    print_file("/tmp/sysmon/cpu.dat");

    printf("\nProcesses data:\n");
    print_file("/tmp/sysmon/proc.dat");

    printf("\n------------------------\n");
    fflush(stdout);
}

int make_dir(void) {
    if (mkdir("/tmp/sysmon", 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
        return 1;
    }

    return 0;
}

int main(void) {
    if (make_dir() != 0) {
        return 1;
    }

    while (1) {
        get_data();
        sleep(5);
    }

    return 0;
}
