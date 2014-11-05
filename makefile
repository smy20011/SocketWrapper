CC=g++
CFLAGS=-g -std=c++0x
EXECUTABLE=$(patsubst src/executable/%.cpp,bin/%,$(wildcard src/executable/*.cpp))
MODELS=$(patsubst src/%.cpp,lib/%.o, $(wildcard src/*.cpp))
HEADERS=$(wildcard src/%.h)
all: $(EXECUTABLE)
$(EXECUTABLE) : bin/% : src/executable/%.cpp $(MODELS)
	$(CC) -o $@ $^ $(CFLAGS)
$(MODELS) : lib/%.o : src/%.cpp src/%.h
	$(CC) -c -o $@ $< $(CFLAGS)
.PHONY: clean 
clean:
	rm bin/* lib/* 
