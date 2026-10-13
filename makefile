compiler = gcc
args = -std=c99 -pedantic -Wall -Wextra
source = $(wildcard src/*.c)
result = PingPong.exe
libs = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: compile run

compile:
	$(compiler) $(args) $(source) -o $(result) $(libs)
run:
	./$(result)
