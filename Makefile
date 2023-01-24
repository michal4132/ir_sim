PROJ=ir
CC=gcc
SRC=main.c
LIBS=-lgpiod -lm
CFLAGS=-D CONSUMER=\"$(PROJ)\"

all:
	$(CC) $(SRC) $(LIBS) $(CFLAGS) -o $(PROJ) -O3 -g
	chown root:root $(PROJ)
	chmod 4755 $(PROJ)

clean:
	rm $(PROJ)
