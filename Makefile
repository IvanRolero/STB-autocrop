CC = gcc
CFLAGS = -O2 -Wall -Wextra -pedantic
LDFLAGS =

SRC = autocrop.c
OUT = autocrop

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT) *.o

.PHONY: all clean
