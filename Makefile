CC=gcc
CFLAGS=-Wall -Wextra -g

SRC=src
OBJ=$(SRC)/main.o $(SRC)/filesystem.o $(SRC)/block_manager.o
DEPS=$(SRC)/filesystem.h $(SRC)/block_manager.h $(SRC)/data_structures.h

all: myfs

myfs: $(OBJ)
	$(CC) $(CFLAGS) -o myfs $(OBJ)

$(SRC)/%.o: $(SRC)/%.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC)/*.o myfs.disk