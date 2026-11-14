#include <stdio.h>  
#include <stdlib.h>
#include <string.h>    
#include <sys/stat.h>
#include <errno.h>   
#include <unistd.h>  
#include <dirent.h>

int is_digits(char *str) {
    if (str[0] == '\0') return 0;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }

    return 1;
}

void read_mem(void) {
  DIR *dir = opendir("/proc");
  if (dir == NULL) {
        perror("opendir");
    }
  struct dirent *entry;
  int count = 0;
  while ((entry = readdir(dir)) != NULL){
    char* name = entry->d_name;
    if (is_digits(name)){
      count++;}
    }

  closedir(dir);
  FILE* outfile = fopen("/tmp/sysmon/proc.dat", "w");

    if (outfile == NULL) {
        perror("fopen proc.dat");
        return;
    }
    fprintf(outfile, "Number of running processes: %d\n", count);
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
