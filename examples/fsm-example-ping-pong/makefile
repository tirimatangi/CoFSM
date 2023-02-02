# Set the default compiler
CC = g++

INCLUDE_DIR = ../../include

# Compiler flag
CPPFLAGS = --pedantic-errors --std=c++20 -Wall -Wextra -I$(INCLUDE_DIR)

# The build target (i.e. the name of the executable)
TARGET = fsm-example-ping-pong

all: $(TARGET)

# Use gnu compiler
gnu: CC = g++
gnu: $(TARGET)

# Use clang compiler
clang: CC = clang++
clang: $(TARGET)

clean:
	rm -f *.o $(TARGET)

$(TARGET): $(TARGET).o
	$(CC) $(CPPFLAGS) -o $(TARGET) $(TARGET).o

$(TARGET).o: $(TARGET).cc $(INCLUDE_DIR)/CoFSM.h
	$(CC) $(CPPFLAGS) $(EXTRAFLAGS) -c $(TARGET).cc
