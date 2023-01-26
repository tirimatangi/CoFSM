# Set the compiler
CC = g++

INCLUDE_DIR = ../../include

# Compiler flag
CPPFLAGS = -O2 --pedantic-errors --std=c++20 -Wall -Wextra -I$(INCLUDE_DIR)

# The build target (i.e. the name of the executable)
TARGET = fsm-example-morse

all: $(TARGET)

# Use laptop's keyboard LEDs for demonstration. The binary must be run with "sudo ./fsm-example-morse"
linux: EXTRAFLAGS = -DLINUX
linux: $(TARGET)

clean:
	rm -f *.o $(TARGET)

$(TARGET): $(TARGET).o
	$(CC) $(CPPFLAGS) -o $(TARGET) $(TARGET).o

$(TARGET).o: $(TARGET).cc $(INCLUDE_DIR)/CoFSM.h
	$(CC) $(CPPFLAGS) $(EXTRAFLAGS) -c $(TARGET).cc
