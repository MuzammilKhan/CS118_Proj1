SHELL := /bin/bash

CC=gcc
#to simply build executable
default:
	gcc webserver.c -o webserver

#deletes files creted in compilation and other processes
clean:
	shopt -s extglob; \
	`rm -f webserver`
