CROSS_COMPILE=/mnt/hdd/tmp/test/luckfox-pico/sysdrv/source/buildroot/buildroot-2025.08/output/host/bin/arm-buildroot-linux-musleabihf-

# Set your cross-compiler prefix (e.g., arm-linux-gnueabihf-)
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
AR := $(CROSS_COMPILE)ar

# Source files
SRCS := main.c
OBJS := $(SRCS:.c=.o)

# Output binary
TARGET := bmsd

# Compiler flags
CFLAGS := -O2 -Wall

CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -O2
TARGET = bmsd
SRC = main.c

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean