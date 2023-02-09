# Set the compiler
CC = g++

INCLUDE_DIR = ../../include

# Compiler flag
CPP_COMMON_FLAGS = --pedantic-errors --std=c++20 -Wall -Wextra -I$(INCLUDE_DIR)
CPP_DEBUG_FLAGS = -g -fsanitize=address
CPP_OPTIMIZATION_FLAGS = -O2

# The build target (i.e. the name of the executable)
TARGET = fsm-example-morse

all: CPPFLAGS = $(CPP_OPTIMIZATION_FLAGS) $(CPP_COMMON_FLAGS)
all: $(TARGET)

# Use laptop's keyboard LEDs for demonstration. The binary must be run with "sudo ./fsm-example-morse"
linux: EXTRAFLAGS = -DLINUX
linux: CPPFLAGS = $(CPP_OPTIMIZATION_FLAGS) $(CPP_COMMON_FLAGS)
linux: $(TARGET)

debug: CPPFLAGS = $(CPP_DEBUG_FLAGS) $(CPP_COMMON_FLAGS)
debug: $(TARGET)

clean:
	rm -f *.o $(TARGET)

$(TARGET): $(TARGET).o
	$(CC) $(CPPFLAGS) -o $(TARGET) $(TARGET).o

$(TARGET).o: $(TARGET).cc $(INCLUDE_DIR)/CoFSM.h
	$(CC) $(CPPFLAGS) $(EXTRAFLAGS) -c $(TARGET).cc
