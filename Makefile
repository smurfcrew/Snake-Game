CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lpthread

# For Unix/Linux/macOS, add fcntl.h
UNAME_S := $(shell uname -s)
ifneq ($(UNAME_S),Windows_NT)
    LIBS += -lncurses
endif

all: main 

main: main.o encrypt.o
	$(CC) $(CFLAGS) -o main main.o encrypt.o $(LIBS)

main.o: main.c encrypt.h
	$(CC) $(CFLAGS) -c main.c

encrypt.o: encrypt.c encrypt.h
	$(CC) $(CFLAGS) -c encrypt.c

clean:
	rm -f main *.o input_record.txt input_record.enc
    
run: main 
	./main