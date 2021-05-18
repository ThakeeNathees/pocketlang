
##  Copyright (c) 2021 Thakee Nathees
##  Licensed under: MIT License

CC          = gcc
CFLAGS      = -lm -fPIC -Wno-int-to-pointer-cast
INCLUDE_DIR = -I../src/include
SOURCES     = ../cli/*.c ../src/*.c

all: debug

init:
	@mkdir -p debug/
	@mkdir -p release/

debug: init
	$(CC) -o debug/pocket $(SOURCES) $(INCLUDE_DIR) $(CFLAGS) -D DEBUG -g3 -Og

release: init
	$(CC) -o release/pocket $(SOURCES) $(INCLUDE_DIR) $(CFLAGS) -g -O3

clean:
	rm -rf debug/
	rm -rf release/
