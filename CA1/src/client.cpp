#include "common.hpp"

#include <exception>

class AirlineClient {
private:
    int sockfd;
    string server_host;

public:
    AirlineClient(const string& host, int port = 8080) : server_host(host) {
        sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("Socket creation failed");
            exit(1);
        }
        sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
            perror("Invalid address");
            exit(1);
        }

        if (::connect(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection failed");
            exit(1);
        }
        string msg = "Connected to server at " + host + ":" + intToString(port) + "\n";
        write(STDOUT_FILENO, msg.c_str(), msg.length());
    }

    ~AirlineClient() {
        if (sockfd > 0) {
            close(sockfd);
        }
    }

    void sendCommand(const string& command) {
        string full_command = command + "\n";
        writeToSocket(sockfd, full_command);
        string response = readFromSocket(sockfd);
        write(STDOUT_FILENO, response.c_str(), response.length());
    }

    void interactiveMode() {
    const char* help_msg =
        "\n=== Airline Reservation System ===\n"
        "Available commands:\n"
        "  LIST_FLIGHTS                    - List all flights\n"
        "  REGISTER <role> <user> <pass>   - Register user (role: CUSTOMER or AIRLINE)\n"
        "  LOGIN <user> <pass>             - Login\n"
        "  --- Airline Commands ---\n"
        "  ADD_FLIGHT <id> <from> <to> <time> <rows> <cols> - Add flight\n"
        "  --- Customer Commands ---\n"
        "  RESERVE <flight_id> <seat1> [seat2] ... - Reserve seats (e.g., RESERVE F101 A1 B2)\n"
        "  CONFIRM <reservation_id>        - Confirm a temporary reservation\n"
        "  CANCEL <reservation_id>         - Cancel a reservation\n"
        "  --- Client Commands ---\n"
        "  EXIT                            - Exit\n"
        "===================================\n\n";

        write(STDOUT_FILENO, help_msg, strlen(help_msg));
        char input[BUFFER_SIZE];
        while (true) {
            write(STDOUT_FILENO, "> ", 2);
            int n = read(STDIN_FILENO, input, BUFFER_SIZE - 1);
            if (n <= 0) {
                break;
            }
            input[n] = '\0';
            string command(input);
            if (!command.empty() && command.back() == '\n') {
                command.pop_back();
            }
            if (command == "EXIT_CMD") {
                write(STDOUT_FILENO, "Goodbye!\n", 9);
                break;
            }
            sendCommand(command);
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        string usage = "Usage: " + string(argv[0]) + " <server_ip>\n" +
                      "Example: " + string(argv[0]) + " 127.0.0.1\n";
        write(STDERR_FILENO, usage.c_str(), usage.length());
        return 1;
    }
    try {
        AirlineClient client(argv[1]);
        client.interactiveMode();
    } catch (const exception& e) {
        perror("Client error");
        return 1;
    }
    return 0;
}