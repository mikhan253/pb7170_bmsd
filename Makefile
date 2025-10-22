CROSS_COMPILE=/mnt/hdd/tmp/test/luckfox-pico/sysdrv/source/buildroot/buildroot-2025.08/output/host/bin/arm-buildroot-linux-musleabihf-
PUSH_MACHINE ?= root@192.168.10.163


# Set your cross-compiler prefix (e.g., arm-linux-gnueabihf-)
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
AR := $(CROSS_COMPILE)ar

# Source files
SRC = main.c spi.c dataobjects.c bms.c
OBJS := $(SRC:.c=.o)

# Output binary
TARGET := bmsd

# Compiler flags
CFLAGS := -O2 -Wall -lgpiod

CFLAGS += -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -ffast-math -ftree-vectorize -fomit-frame-pointer

CC ?= $(CROSS_COMPILE)gcc
TARGET = bmsd

UNITTESTFLAGS := -fsanitize=address -fsanitize=undefined -fsanitize=leak
UNITTESTFLAGS += -fsanitize=pointer-subtract -fsanitize=pointer-compare -fsanitize=undefined -fsanitize=bounds -fsanitize=bounds-strict
UNITTESTFLAGS += -fstack-protector-all -fstack-clash-protection -D_FORTIFY_SOURCE=2 -ftrapv

all:
	python _dataobjectshelper.py
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

push:
	tar czf - bmsd conf webserver | ssh $(PUSH_MACHINE) "tar xzf - -C /tmp"

unittest:
	@gcc -o unittest-$(TARGET) -O2 $(UNITTESTFLAGS) unit-test.c
	@./unittest-$(TARGET)
	@rm unittest-$(TARGET)

.PHONY: all clean