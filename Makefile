CC := cc
CFLAGS := -std=c11 -Wall -Wextra -O2 -pthread
LDFLAGS := -pthread

SRC_DIR := src
BIN := otp
SRCS := $(SRC_DIR)/main.c $(SRC_DIR)/barrier.c $(SRC_DIR)/lcg.c
OBJS := $(SRCS:.c=.o)

.PHONY: all clean run test

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN)

run: $(BIN)
	./$(BIN) --help | cat

test: $(BIN)
	bash scripts/test_roundtrip.sh


