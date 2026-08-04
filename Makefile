# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -pthread -D_DEFAULT_SOURCE -g
LDFLAGS = -pthread
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
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# Compile source files to object files
# $< is the first prerequisite (the .c file)
# $@ is the target (the .o file)
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Remove object files and executable
clean:
	rm -f $(OBJECTS) $(TARGET) ghost_tests log_*.csv

test: ghost_tests all
	./ghost_tests
	python3 tests/integration.py
	node tests/session_integration.mjs

stress: all
	python3 scripts/stress.py

ghost_tests: tests/test_runner.c house.c room.c hunter.c ghost.c stack.c helpers.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ $(filter %.c,$^) $(LDFLAGS)

tsan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fsanitize=thread -O1" LDFLAGS="$(LDFLAGS) -fsanitize=thread" all

asan:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined" all

# Phony targets
.PHONY: all clean test stress tsan asan
