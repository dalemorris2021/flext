CC=clang -std=c99
CFLAGS=-g -Wall -Wextra -Wpedantic -Werror

SRC=./src
BIN=./build/bin
OBJ=./build/obj

OBJS=$(OBJ)/main.o

.PHONY: all run clean

all: $(BIN) $(OBJ) $(BIN)/main

run:
	make all
	$(BIN)/main

clean:
	rm -rf $(BIN)/*
	rm -rf $(OBJ)/*

$(BIN):
	mkdir -p $(BIN)

$(OBJ):
	mkdir -p $(OBJ)

$(BIN)/main: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN)/main

$(OBJ)/main.o: $(SRC)/main.c
	$(CC) $(CFLAGS) -c $(SRC)/main.c -o $(OBJ)/main.o
