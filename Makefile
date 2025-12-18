# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread
TARGET = ghost_hunt

# Source files
SOURCES = main.c house.c room.c hunter.c ghost.c stack.c helpers.c
# Automatically generate object file names from source files
OBJECTS = $(SOURCES:.c=.o)

# Header files for dependency checking
HEADERS = defs.h helpers.h

# Default target: builds the executable
all: $(TARGET)

# Link object files to create executable
# Requires all object files to be compiled first
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Compile source files to object files
# $< is the first prerequisite (the .c file)
# $@ is the target (the .o file)
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Remove object files and executable
clean:
	rm -f $(OBJECTS) $(TARGET) log_*.csv

# Phony targets
.PHONY: all clean