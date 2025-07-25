# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
LDFLAGS = -lpthread -lmysqlclient

# Source and object files
SRC = src/main.c src/reservation.c src/hashtable.c src/db_interface.c src/utils.c
OBJ = $(SRC:.c=.o)

# Output binary
TARGET = ticketbook

# Default build
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each .c file to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJ) $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)