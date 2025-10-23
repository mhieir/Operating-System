CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -g
SRCS := $(wildcard *.cpp)
BINS := $(SRCS:.cpp=)
all: $(BINS)
%: %.cpp
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f $(BINS) *.o
run-server: server
	./server

run-client: client
	./client
