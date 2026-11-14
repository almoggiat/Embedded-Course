#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

int get_int_from_str(const char* string) {
    for (size_t i = 0; string[i] != '\0'; i++) {
        if (string[i] >= '0' && string[i] <= '9') {
            return atoi(string + i);
        }
    }

    return 0;
}

void read_mem(void) {
    FILE* mem_file = fopen("/proc/meminfo", "r");

    if (mem_file == NULL) {
        perror("fopen meminfo");
        return;
    }

    int mem_info[5] = {0};

    const char* phrases[5] = {
        "MemTotal:",
        "MemFree:",
        "MemAvailable:",
        "SwapTotal:",
        "SwapFree:"
    };

    int line_numbers[5] = {1, 2, 3, 15, 16};

    char line[256];
    int counter = 0;

    for (int row = 1;
         counter < 5 && fgets(line, sizeof(line), mem_file) != NULL;
         row++) {

        if (row == line_numbers[counter]) {
            mem_info[counter] = get_int_from_str(line);
            counter++;
        }
    }

    fclose(mem_file);

    if (counter < 5) {
        fprintf(stderr, "Could not read all requested rows\n");
        return;
    }

    FILE* outfile = fopen("/tmp/sysmon/mem.dat", "w");

    if (outfile == NULL) {
        perror("fopen mem.dat");
        return;
    }

    for (int i = 0; i < 5; i++) {
        fprintf(outfile, "%s %d kB\n", phrases[i], mem_info[i]);
    }

    fclose(outfile);
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
        read_mem();
        sleep(5);
    }

    return 0;
}
