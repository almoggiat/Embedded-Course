
CC = gcc
CFLAGS = -Wall -Wextra -g -I./inc -pthread

SRC_DIR = src
BIN_DIR = bin

TARGETS = $(BIN_DIR)/sysmond $(BIN_DIR)/mem_sensor \
          $(BIN_DIR)/cpu_sensor $(BIN_DIR)/proc_sensor

all: $(TARGETS)

$(BIN_DIR)/%: $(SRC_DIR)/%.c inc/sysmon_ipc.h | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
