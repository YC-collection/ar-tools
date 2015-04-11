#Compiler to use
CC=gcc
#Compiler Flags
CFLAGS=-Wall
#Linker Flags
LDFLAGS= #-lrt

all: clean ar_loader

ar_loader: serial_fpga_loader.o gpio.o
	$(CC) -o  ar_loader $?

# Makefile rules:
# http://www.cyut.edu.tw/~dywang/linuxProgram/node36.html
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $?

install: ar_loader
	install ar_loader /usr/bin

clean:
	rm -rf *o ar_loader
