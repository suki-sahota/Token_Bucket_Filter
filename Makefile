# Author: Suki Sahota
#
# This is the Makefile that can be used to create the "qdisc" executable
#
# To create all executables, do:
#	make all
#
# To create "qdisc" executable, do:
#	make qdisc
#
# To clean project, do:
#	make clean
#
all: qdisc

qdisc: qdisc.o my_list.o
	gcc -o qdisc -g -pthread qdisc.o my_list.o -lm

qdisc.o: qdisc.c my_list.h
	gcc -g -c -Wall -pthread qdisc.c -lm

my_list.o: my_list.c my_list.h
	gcc -g -c -Wall my_list.c

clean:
	rm -f *.o f?.* qdisc

