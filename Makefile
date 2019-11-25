CC = gcc
CFLAGS = -Wall 
LDFLAGS = -lpthread 

server_objs = game_server.o game_engine.o packets.o helper_funcs.o 

client_objs = game_client.o packets.o helper_funcs.o

all: server client

server: $(server_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: $(client_objs)
	$(CC) $(CFLAGS) -o $@ $^

$(server_objs) $(client_objs): game_incl.h packets.h helper_funcs.h
game_server.o: game_server.h game_engine.h
game_client.o: game_client.h

clean_server:
	rm -f server $(server_objs)
	
clean_client:
	rm -f client $(client_objs)

clean:
	rm -f server client $(server_objs) $(client_objs)

.PHONY: all server client clean_server clean_client clean