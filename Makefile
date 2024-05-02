TARGETS=homework5 thread_example server

CFLAGS=-Wall -g -O0

all: $(TARGETS)

homework5: homework5.c
	gcc $(CFLAGS) -o homework5 homework5.c -lpthread

server: server.c
	gcc $(CFLAGS) -o server server.c -lpthread

thread_example: thread_example.c
	gcc $(CFLAGS) -o thread_example thread_example.c -lpthread

clean:
	rm -f $(TARGETS)
