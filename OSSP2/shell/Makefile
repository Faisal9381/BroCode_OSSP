# ForgeShell — mini Linux shell with background job management
CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -g -std=gnu11
LDFLAGS ?=

SRC  = main.c parser.c execute.c builtins.c jobs.c
OBJ  = $(SRC:.c=.o)
BIN  = forgeshell

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c shell.h parser.h execute.h builtins.h jobs.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
