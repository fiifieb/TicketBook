# Compiler and flags
CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -Iinclude
LDFLAGS  = -lpthread

# Detect architecture (basic)
ARCH := $(shell uname -m)
ifeq ($(ARCH),riscv64)
  RV_SRC = src/riscv_inline.s
else
  RV_SRC =
endif

# Source and object files (main app)
SRC = src/reservation.c src/hashtable.c src/db_interface.c src/utils.c
OBJ = $(SRC:.c=.o)

# ---- Default build ----
all: $(TESTS)

# Compile each .c file to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Tests ----
TEST_INC  = -Iinclude
TEST_LIBS = -lpthread
TESTS     = tests/test_hashtable tests/test_reservation tests/test_db_interface

tests/test_hashtable: tests/test_hashtable.c src/hashtable.c src/utils.c $(RV_SRC)
	$(CC) $(CFLAGS) $(TEST_INC) -o $@ $^ $(TEST_LIBS)

tests/test_reservation: tests/test_reservation.c src/reservation.c src/hashtable.c src/db_interface.c src/utils.c $(RV_SRC)
	$(CC) $(CFLAGS) $(TEST_INC) -o $@ $^ $(TEST_LIBS)

tests/test_db_interface: tests/test_db_interface.c src/db_interface.c $(RV_SRC)
	$(CC) $(CFLAGS) $(TEST_INC) -o $@ $^ $(TEST_LIBS)

test_hashtable: tests/test_hashtable
	./tests/test_hashtable

test_reservation: tests/test_reservation
	./tests/test_reservation

test_db_interface: tests/test_db_interface
	./tests/test_db_interface

test: test_hashtable test_db_interface test_reservation

# ---- Convenience ----
run: test_reservation
	./tests/test_reservation

debug: CFLAGS := -g -O0 -Wall -Wextra -Iinclude
debug: clean all

clean:
	rm -f $(OBJ) $(TESTS)

.PHONY: all test test_hashtable test_db_interface test_reservation run debug clean
