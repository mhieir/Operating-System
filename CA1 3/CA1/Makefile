CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20 -g -Iinclude -pthread
LDFLAGS = -lpthread
TARGETS = server client
SERVER_SRCS = src/server.cpp src/main.cpp src/common.cpp
CLIENT_SRCS = src/client.cpp src/common.cpp
SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)
all: $(TARGETS)
server: $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
client: $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -f $(TARGETS) src/*.o
	rm -rf *.dSYM
.PHONY: all clean