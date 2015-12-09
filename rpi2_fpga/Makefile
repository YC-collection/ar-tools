#Compiler to use
CC=gcc
#Compiler Flags
CFLAGS=-Wall
#Linker Flags
LDFLAGS= -lpigpio -lrt -lpthread

all: clean ar_loader

ar_loader: rpi2_fpga.o
	$(CC) $(CFLAGS) -o  ar_loader $? $(LDFLAGS)
	# sudo chown root:artek ar_loader
	# sudo chmod 4750 ar_loader

# Makefile rules:
# http://www.cyut.edu.tw/~dywang/linuxProgram/node36.html
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $?

install: ar_loader
	install ar_loader /usr/bin
	chown root:artek /usr/bin/ar_loader
	chmod 4750 /usr/bin/ar_loader

clean:
	rm -rf *o ar_loader
