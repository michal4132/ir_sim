PROJ=ir
CC=gcc
SRC=main.c
LIBS=-lgpiod -lm
CFLAGS=-D CONSUMER=\"$(PROJ)\"

all: build perm

build:
	$(CC) $(SRC) $(LIBS) $(CFLAGS) -o $(PROJ) -O3 -g
perm:
	chown root:root $(PROJ)
	chmod 4755 $(PROJ)

clean:
	rm $(PROJ)
