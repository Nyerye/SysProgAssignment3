# Main Target
bin/shm_manager : obj/shm_manager.o obj/common.o
	$(CC) obj/shm_manager.o obj/common.o -lncurses -o bin/shm_manager

bin/server : obj/server.o obj/common.o
	$(CC) obj/server.o obj/common.o -lncurses -o bin/server

bin/client : obj/client.o obj/common.o
	$(CC) obj/client.o obj/common.o -lncurses -o bin/client

# Object files
obj/shm_manager.o : src/shm_manager.c
	$(CC) -c src/shm_manager.c -I inc -o obj/shm_manager.o

obj/server.o : src/server.c
	$(CC) -c src/server.c -I inc -o obj/server.o

obj/client.o : src/client.c
	$(CC) -c src/client.c -I inc -o obj/client.o

obj/common.o : src/common.c
	$(CC) -c src/common.c -I inc -o obj/common.o

# Default target
all: bin/shm_manager bin/server bin/client

# Cleanup
clean:
	rm -f bin/*
	rm -f obj/*
