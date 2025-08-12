# Compiler and flags
CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -Iinclude
LDFLAGS  = -lpthread -lmysqlclient -lmicrohttpd

# Source and object files (main app)
SRC = src/main.c src/http_server.c src/reservation.c src/hashtable.c src/db_interface.c src/utils.c
OBJ = $(SRC:.c=.o)

# Output binary
TARGET = ticketbook

# ---- Default build ----
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile each .c file to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Tests ----
TEST_INC  = -Iinclude
TEST_LIBS = -lpthread
TESTS     = tests/test_hashtable tests/test_reservation

tests/test_hashtable: tests/test_hashtable.c src/hashtable.c src/utils.c
	$(CC) $(CFLAGS) $(TEST_INC) -o $@ $^ $(TEST_LIBS)

tests/test_reservation: tests/test_reservation.c src/reservation.c src/hashtable.c src/utils.c
	$(CC) $(CFLAGS) $(TEST_INC) -o $@ $^ $(TEST_LIBS)

test_hashtable: tests/test_hashtable
	./tests/test_hashtable

test_reservation: tests/test_reservation
	./tests/test_reservation

test: test_hashtable test_reservation

# ---- Convenience ----
run: $(TARGET)
	./$(TARGET)

debug: CFLAGS := -g -O0 -Wall -Wextra -Iinclude
debug: clean $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET) $(TESTS)
