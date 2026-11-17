CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

TARGET = bin/sysmond
SOURCES = src/sysmond.c src/mem_sensor.c src/cpu_sensor.c src/proc_sensor.c

all: $(TARGET)

$(TARGET): $(SOURCES) src/sysmon.h | bin
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

bin:
	mkdir -p bin

clean:
	rm -f $(TARGET)
