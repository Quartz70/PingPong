CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -Wextra
SRC = $(wildcard src/*.c)
TARGET = PingPong.exe
LDLIBS = -lraylib -lm -lX11

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDLIBS)
