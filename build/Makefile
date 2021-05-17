
##  Copyright (c) 2021 Thakee Nathees
##  Licensed under: MIT License

CC          = gcc
CFLAGS      = -lm -fPIC -Wno-int-to-pointer-cast
INCLUDE_DIR = -I../src/include
SOURCES     = ../cli/*.c ../src/*.c

all: debug

init:
	mkdir -p bin/

debug: init
	$(CC) -o bin/pocket $(SOURCES) $(INCLUDE_DIR) $(CFLAGS) -g3 -Og

release: init
	$(CC) -o bin/pocket $(SOURCES) $(INCLUDE_DIR) $(CFLAGS) -g -O3

clean:
	rm -rf bin/
