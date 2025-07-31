CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LDFLAGS =

SRC = src
OBJ = obj
BIN = bin

tmp = tmp
data = data

TARGETS = $(BIN)/dclient $(BIN)/dserver

# Compilação normal
all: CFLAGS += -DDEBUG_MODE=0
all: directories $(TARGETS)

# Compilação em modo debug
debug: CFLAGS += -DDEBUG_MODE=1
debug: directories $(TARGETS)

directories:
	@mkdir -p $(OBJ) $(BIN) $(data) $(tmp)

$(BIN)/dclient: $(OBJ)/dclient.o $(OBJ)/common.o
	$(CC) $(LDFLAGS) $^ -o $@
	@echo "Client built successfully"

$(BIN)/dserver: $(OBJ)/dserver.o $(OBJ)/index.o $(OBJ)/common.o
	$(CC) $(LDFLAGS) $^ -o $@
	@echo "Server built successfully"

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -rf $(OBJ)/*.o $(BIN)/* $(tmp)/*
	@echo "Clean complete"

.PHONY: all debug directories clean
