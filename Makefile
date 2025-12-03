CC := clang 
CFLAGS := -g -fsanitize=address

all: server client

clean:
	rm -rf server client

server: server.c message.h message.c socket.h user.h
	$(CC) $(CFLAGS) -o server server.c message.c -lpthread

client: client.c message.h message.c user.h
	$(CC) $(CFLAGS) -o client client.c message.c

