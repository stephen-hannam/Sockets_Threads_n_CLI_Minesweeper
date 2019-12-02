CC = gcc
CFLAGS = -Wall
LDFLAGS = -lpthread

minefield_objs = game.o helper_funcs.o

all: minefield

minefield: $(minefield_objs)
	$(CC) $(CFLAGS) -o $@ $^

$(minefield_objs): game_incl.h helper_funcs.h
game.o: game.h

clean:
	rm -f minefield $(minefield_objs)

.PHONY: all clean
