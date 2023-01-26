# Set the compiler
CC = g++

INCLUDE_DIR = ../../include

# Compiler flag
CPPFLAGS = -O2 --pedantic-errors --std=c++20 -Wall -Wextra -I$(INCLUDE_DIR)

# The build target (i.e. the name of the executable)
TARGET = fsm-example-rgb

all: $(TARGET)

# Use laptop's keyboard LEDs for demonstration. Must be run with "sudo ./fsm-example-rgb".
linux: EXTRAFLAGS = -DLINUX
linux: $(TARGET)

clean:
	rm -f *.o $(TARGET)

$(TARGET): $(TARGET).o fsm-red.o fsm-green.o fsm-blue.o
	$(CC) $(CPPFLAGS) -o $(TARGET) $(TARGET).o fsm-red.o fsm-green.o fsm-blue.o

$(TARGET).o: $(TARGET).cc $(INCLUDE_DIR)/CoFSM.h
	$(CC) $(CPPFLAGS) $(EXTRAFLAGS) -c $(TARGET).cc

fsm-red.o: fsm-red.cc $(INCLUDE_DIR)/CoFSM.h
	$(CC) $(CPPFLAGS) $(EXTRAFLAGS) -c fsm-red.cc

fsm-green.o: fsm-green.cc $(INCLUDE_DIR)/CoFSM.h
	$(CC) $(CPPFLAGS) $(EXTRAFLAGS) -c fsm-green.cc

fsm-blue.o: fsm-blue.cc $(INCLUDE_DIR)/CoFSM.h
	$(CC) $(CPPFLAGS) $(EXTRAFLAGS) -c fsm-blue.cc
