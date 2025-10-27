#include "common.hpp"

class AirlineClient {
private:
    int sockfd;
    int udp_socket;
    string server_host;

public:
    AirlineClient(const string& host, int port = 8081) : server_host(host) {
        // --- TCP Socket Setup (unchanged) ---
        sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            put_error("Socket creation failed");
            exit(1);
        }
        sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
            put_error("Invalid address");
            exit(1);
        }

        if (::connect(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            put_error("Connection failed");
            exit(1);
        }

        // --- UDP Socket Setup (modified for Broadcast) ---
        udp_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0) {
            put_error("UDP socket creation failed");
            exit(1);
        }

        // Allow multiple sockets to use the same port
        int reuse = 1;
        if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            perror("setsockopt(SO_REUSEADDR) failed");
        }

        // *** CHANGE START: Enable broadcast listening ***
        int broadcast_enable = 1;
        if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
            perror("setsockopt(SO_BROADCAST) failed");
            exit(1);
        }
        // *** CHANGE END ***

        // Bind the UDP socket to receive messages on the specified port from any address
        sockaddr_in server_addr_udp;
        memset(&server_addr_udp, 0, sizeof(server_addr_udp));
        server_addr_udp.sin_family = AF_INET;
        server_addr_udp.sin_addr.s_addr = INADDR_ANY;
        server_addr_udp.sin_port = htons(port);

        if (bind(udp_socket, (sockaddr*)&server_addr_udp, sizeof(server_addr_udp)) < 0) {
            put_error("UDP bind failed");
            exit(1);
        }
        
        string msg = "Connected to server at " + host + ":" + intToString(port) + "\n";
        write(STDOUT_FILENO, msg.c_str(), msg.length());
    }

    ~AirlineClient() {
        if (sockfd > 0) {
            close(sockfd);
        }
        if (udp_socket > 0) {
            close(udp_socket);
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
        
        string current_line = "";

        while (true) {
            cout << "> " << flush;

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            FD_SET(sockfd, &readfds);
            FD_SET(udp_socket, &readfds);
            
            int max_sd = max(sockfd, udp_socket);
            int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

            if (activity < 0) {
                perror("select error");
                break;
            }

            // Activity on standard input (user typing a command)
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                string line;
                getline(cin, line);
                if (cin.eof() || line == "EXIT") {
                     write(STDOUT_FILENO, "Goodbye!\n", 9);
                     break;
                }
                line += "\n"; // Add newline to match server expectation
                send(sockfd, line.c_str(), line.length(), 0);
                current_line = line; // Store what user typed
            }

            // Activity on TCP socket (response from server)
            if (FD_ISSET(sockfd, &readfds)) {
                memset(input, 0, BUFFER_SIZE);
                int valread = read(sockfd, input, BUFFER_SIZE - 1);
                if (valread <= 0) {
                    cout << "\nServer disconnected." << endl;
                    break;
                }
                cout << "\r" << string(input); // Overwrite prompt and show message
            }
            
            // Activity on UDP socket (broadcast message from server)
            if (FD_ISSET(udp_socket, &readfds)) {
                memset(input, 0, BUFFER_SIZE);
                int valread = recvfrom(udp_socket, input, BUFFER_SIZE - 1, 0, NULL, NULL);
                if (valread > 0) {
                    // Clear the current line, print the broadcast message, then reprint the prompt
                    cout << "\r" << string(80, ' ') << "\r"; // Clear line
                    cout << "[BROADCAST] " << input << endl;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <server_host>" << endl;
        return 1;
    }
    AirlineClient client(argv[1]);
    client.interactiveMode();

    return 0;
}