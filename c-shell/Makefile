CC      = gcc
CSTD    = -std=c99
DEFINES = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
WARN    = -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS  = $(CSTD) $(DEFINES) $(WARN) -fno-asm -Iinclude

SRC_DIR = src
SRCS    = $(wildcard $(SRC_DIR)/*.c)
OBJS    = $(SRCS:.c=.o)

TARGET  = shell.out

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)