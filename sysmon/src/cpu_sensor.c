#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

void read_cpu(void) {
    FILE* cpu_file = fopen("/proc/loadavg", "r");

    if (cpu_file == NULL) {
        perror("fopen loadavg");
        return;
    }

    const char* phrases[3] = {
        "1-minute load average: ",
        "5-minute load average: ",
        "15-minute load average: "
    };

    char cpu_info[3][256] = {0};
    char line[256];

    if (fgets(line, sizeof(line), cpu_file) == NULL) {
        fprintf(stderr, "Could not read loadavg\n");
        fclose(cpu_file);
        return;
    }

    fclose(cpu_file);

    int j = 0;
    int k = 0; 

    for (int i = 0; line[i] != '\0' && j < 3; i++) {
        if (line[i] == ' ' || line[i] == '\t' || line[i] == '\n') {
            if (k > 0) {
                cpu_info[j][k] = '\0';
                j++;
                k = 0;
            }
        } else {
            cpu_info[j][k] = line[i];
            k++;
        }
    }

    if (j < 3 && k > 0) {
        cpu_info[j][k] = '\0';
        j++;
    }

    if (j < 3) {
        fprintf(stderr, "Could not read all three load averages\n");
        return;
    }

    FILE* outfile = fopen("/tmp/sysmon/cpu.dat", "w");

    if (outfile == NULL) {
        perror("fopen cpu.dat");
        return;
    }

    for (int i = 0; i < 3; i++) {
        fprintf(outfile, "%s%s\n", phrases[i], cpu_info[i]);
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
        read_cpu();
        sleep(5);
    }

    return 0;
}
