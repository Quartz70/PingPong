args = -std=c99 -pedantic -Wall -Wextra
source = $(wildcard src/*.c)
result = PingPong.exe
libs = -lraylib -lm -lX11

all:
	gcc $(args) $(source) -o $(result) $(libs)
