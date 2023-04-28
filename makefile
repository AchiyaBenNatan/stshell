CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

SRC = stshell.c
OBJ = $(SRC:.c=.o)
EXEC = stshell

.PHONY: all clean

all: $(EXEC)
defult : $(EXEC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)