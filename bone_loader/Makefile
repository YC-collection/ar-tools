#Compiler to use
CC=gcc
#Compiler Flags
CFLAGS=-Wall
#Linker Flags
LDFLAGS= #-lrt

all: clean logi_loader

logi_loader: serial_fpga_loader.o gpio.o
	$(CC) -o  logi_loader $?

# Makefile rules:
# http://www.cyut.edu.tw/~dywang/linuxProgram/node36.html
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $?

install: logi_loader
	install logi_loader /usr/bin

clean:
	rm -rf *o logi_loader
