CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lm

SRC = new.c mmh3.c bloom.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean

all: new

new: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf new $(OBJ)
