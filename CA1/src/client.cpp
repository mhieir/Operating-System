#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char message[1024];

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));

    while (true) {
        std::cout << "Enter message: ";
        std::cin.getline(message, sizeof(message));
        if (strcmp(message, "exit") == 0) break;
        send(client_socket, message, strlen(message), 0);
    }

    close(client_socket);
    return 0;
}
