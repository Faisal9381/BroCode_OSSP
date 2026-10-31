CC = gcc
CFLAGS = -Wall -Wextra -std=c11

TARGET = file_explorer

SOURCES = main.c file_ops.c dir_ops.c

all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)
