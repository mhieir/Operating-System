#include "common.hpp"

class AirlineServer {
private:
    int tcp_socket;
    int udp_socket;
    vector<int> client_sockets;
    unordered_map<string, User> users;
    unordered_map<string, Flight> flights;
    unordered_map<int, Reservation> reservations;
    int reservation_counter;

public:
    AirlineServer(int port = 8080) : reservation_counter(1000) {
        client_sockets.resize(MAX_CLIENTS, 0);
        tcp_socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (tcp_socket < 0) {
            perror("TCP socket creation failed");
            exit(1);
        }
        udp_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket < 0) {
            perror("UDP socket creation failed");
            exit(1);
        }
        sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        int opt = 1;
        if (::setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt failed");
            exit(1);
        }
        if (::bind(tcp_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("TCP bind failed");
            exit(1);
        }
        if (::bind(udp_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("UDP bind failed");
            exit(1);
        }
        if (::listen(tcp_socket, 5) < 0) {
            perror("listen failed");
            exit(1);
        }
        initializeSampleFlights();
        const char* msg = "Server initialized with LIST_FLIGHTS, REGISTER, LOGIN, ADD_FLIGHT\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
    ~AirlineServer() {
        close(tcp_socket);
        close(udp_socket);
        for (int sock : client_sockets) {
            if (sock > 0) close(sock);
        }
    }
    void run() {
        fd_set readfds;
        int max_sd, activity;
        const char* msg = "Server started. Waiting for connections...\n";
        write(STDOUT_FILENO, msg, strlen(msg));
        while (true) {
            FD_ZERO(&readfds);
            FD_SET(tcp_socket, &readfds);
            FD_SET(udp_socket, &readfds);
            max_sd = (tcp_socket > udp_socket) ? tcp_socket : udp_socket;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                int sd = client_sockets[i];
                if (sd > 0) {
                    FD_SET(sd, &readfds);
                }
                if (sd > max_sd) {
                    max_sd = sd;
                }
            }
            activity = ::select(max_sd + 1, &readfds, NULL, NULL, NULL);
            if (activity < 0) {
                perror("select error");
                continue;
            }
            if (FD_ISSET(tcp_socket, &readfds)) {
                handleNewConnection();
            }
            if (FD_ISSET(udp_socket, &readfds)) {
                handleUDPBroadcast();
            }
            for (int i = 0; i < MAX_CLIENTS; i++) {
                int sd = client_sockets[i];
                if (sd > 0 && FD_ISSET(sd, &readfds)) {
                    handleClientMessage(sd, i);
                }
            }
        }
    }

private:
    void initializeSampleFlights() {
        flights["F101"] = Flight("F101", "Tehran", "Paris", "08:30", 10, 6);
        flights["F205"] = Flight("F205", "Mashhad", "Dubai", "12:15", 8, 5);
        flights["F310"] = Flight("F310", "Shiraz", "Istanbul", "15:45", 12, 5);
        flights["F101"].reserveSeat("A1", "user1");
        flights["F101"].reserveSeat("B2", "user2");
        flights["F205"].reserveSeat("C3", "user3");
    }
    void broadcastToAirlines(const string& message) {
        sockaddr_in broadcast_addr;
        memset(&broadcast_addr, 0, sizeof(broadcast_addr));
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
        broadcast_addr.sin_port = htons(8081);
        for (int port = 8082; port <= 8090; port++) {
            broadcast_addr.sin_port = htons(port);
            sendto(udp_socket, message.c_str(), message.length(), 0, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0) {
                for (auto& user_pair : users) {
                    if (user_pair.second.getSocket() == client_sockets[i] && user_pair.second.isLoggedIn() && user_pair.second.isAirline()) {
                        writeToSocket(client_sockets[i], "BROADCAST: " + message + "\n");
                        break;
                    }
                }
            }
        }
        string log_msg = "BROADCAST to AIRLINES: " + message + "\n";
        write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
    }
    void broadcastToCustomers(const string& message) {
        sockaddr_in broadcast_addr;
        memset(&broadcast_addr, 0, sizeof(broadcast_addr));
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
        broadcast_addr.sin_port = htons(8081);
        for (int port = 8082; port <= 8090; port++) {
            broadcast_addr.sin_port = htons(port);
            sendto(udp_socket, message.c_str(), message.length(), 0, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0) {
                for (auto& user_pair : users) {
                    if (user_pair.second.getSocket() == client_sockets[i] && user_pair.second.isLoggedIn() && user_pair.second.isCustomer()) {
                        writeToSocket(client_sockets[i], "BROADCAST: " + message + "\n");
                        break;
                    }
                }
            }
        }
        string log_msg = "BROADCAST to CUSTOMERS: " + message + "\n";
        write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
    }
    void broadcastNewUser(const string& username, const string& role) {
        string message = "NEW_USER " + username + " " + role;
        broadcastToAirlines(message);
    }
    void broadcastNewFlight(const string& flight_id, const string& origin, const string& destination, const string& time) {
        string message = "NEW_FLIGHT " + flight_id + " " + origin + " " + destination + " " + time;
        broadcastToCustomers(message);
    }
    void handleUDPBroadcast() {
        char buffer[BUFFER_SIZE];
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int n = recvfrom(udp_socket, buffer, BUFFER_SIZE, 0, (sockaddr*)&client_addr, &client_len);
        if (n > 0) {
            buffer[n] = '\0';
            string msg = "UDP received: " + string(buffer) + "\n";
            write(STDOUT_FILENO, msg.c_str(), msg.length());
        }
    }
    bool parseCommand(const string& command, vector<string>& tokens, string& error_msg) {
        tokens = tokenize(command);
        if (tokens.empty()) {
            error_msg = "ERROR Empty command\n";
            return false;
        }
        string cmd = tokens[0];
        if (cmd == "REGISTER") {
            if (tokens.size() != 4) {
                error_msg = "ERROR Invalid REGISTER format. Use: REGISTER <role> <username> <password>\n";
                return false;
            }
            if (tokens[1] != "AIRLINE" && tokens[1] != "CUSTOMER") {
                error_msg = "ERROR Invalid role. Must be AIRLINE or CUSTOMER\n";
                return false;
            }
            return true;
        }
        else if (cmd == "LOGIN") {
            if (tokens.size() != 3) {
                error_msg = "ERROR Invalid LOGIN format. Use: LOGIN <username> <password>\n";
                return false;
            }
            return true;
        }
        else if (cmd == "LOGOUT") {
            if (tokens.size() != 1) {
                error_msg = "ERROR Invalid LOGOUT format. Use: LOGOUT\n";
                return false;
            }
            return true;
        }
        else if (cmd == "LIST_FLIGHTS") {
            if (tokens.size() != 1) {
                error_msg = "ERROR Invalid LIST_FLIGHTS format. Use: LIST_FLIGHTS\n";
                return false;
            }
            return true;
        }
        else if (cmd == "ADD_FLIGHT") {
            if (tokens.size() != 7) {
                error_msg = "ERROR Invalid ADD_FLIGHT format. Use: ADD_FLIGHT <flight_id> <origin> <destination> <time> <rows> <cols>\n";
                return false;
            }
            return true;
        }
        error_msg = "ERROR Unknown command. Available: LIST_FLIGHTS, REGISTER, LOGIN, LOGOUT, ADD_FLIGHT\n";
        return false;
    }
    bool registerUser(const string& role_str, const string& username, const string& password) {
        if (users.find(username) != users.end()) {
            return false;
        }
        UserRole role;
        if (role_str == "AIRLINE") {
            role = UserRole::AIRLINE;
        } else {
            role = UserRole::CUSTOMER;
        }
        users[username] = User(username, password, role);
        return true;
    }
    bool loginUser(const string& username, const string& password, int client_socket) {
        auto it = users.find(username);
        if (it == users.end()) {
            return false;
        }
        if (it->second.getPassword() != password) {
            return false;
        }
        if (it->second.isLoggedIn()) {
            return false;
        }
        it->second.setLoggedIn(true);
        it->second.setSocket(client_socket);
        return true;
    }
    void handleListFlights(int client_socket) {
        string response;
        if (flights.empty()) {
            response = "No flights available\n";
        } else {
            for (const auto& pair : flights) {
                const Flight& flight = pair.second;
                response += "FLIGHT " + flight.getId() + " " + flight.getOrigin() + " " + flight.getDestination() + " " + flight.getTime() + " SEATS_AVAILABLE=" + intToString(flight.getAvailableSeats()) + "/" + intToString(flight.getTotalSeats()) + "\n";
            }
        }
        writeToSocket(client_socket, response);
    }
    void handleRegister(int client_socket, const vector<string>& tokens) {
        string role = tokens[1];
        string username = tokens[2];
        string password = tokens[3];
        if (registerUser(role, username, password)) {
            writeToSocket(client_socket, "REGISTERED OK\n");
            string log_msg = "New user registered: " + username + " (" + role + ")\n";
            write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
            broadcastNewUser(username, role);
        } else {
            writeToSocket(client_socket, "ERROR UsernameAlreadyExists\n");
        }
    }
    void handleLogin(int client_socket, const vector<string>& tokens) {
        string username = tokens[1];
        string password = tokens[2];
        if (loginUser(username, password, client_socket)) {
            writeToSocket(client_socket, "LOGIN OK\n");
            string log_msg = "User logged in: " + username + "\n";
            write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
        } else {
            auto it = users.find(username);
            if (it == users.end()) {
                writeToSocket(client_socket, "ERROR UserNotFound\n");
            } else if (it->second.getPassword() != password) {
                writeToSocket(client_socket, "ERROR InvalidPassword\n");
            } else if (it->second.isLoggedIn()) {
                writeToSocket(client_socket, "ERROR UserAlreadyLoggedIn\n");
            } else {
                writeToSocket(client_socket, "ERROR InvalidCredentials\n");
            }
        }
    }
    void handleLogout(int client_socket, const vector<string>& tokens) {
        string logged_in_user;
        for (auto& user_pair : users) {
            if (user_pair.second.getSocket() == client_socket && user_pair.second.isLoggedIn()) {
                logged_in_user = user_pair.first;
                user_pair.second.setLoggedIn(false);
                user_pair.second.setSocket(UN_INITIALIZED_SOCKET);
                break;
            }
        }
        if (!logged_in_user.empty()) {
            writeToSocket(client_socket, "LOGOUT OK\n");
            string log_msg = "User logged out: " + logged_in_user + "\n";
            write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
        } else {
            writeToSocket(client_socket, "ERROR NotLoggedIn\n");
        }
    }
    void handleAddFlight(int client_socket, const vector<string>& tokens) {
        string flight_id = tokens[1];
        string origin = tokens[2];
        string destination = tokens[3];
        string time = tokens[4];
        int rows = stoi(tokens[5]);
        int cols = stoi(tokens[6]);
        if (flights.find(flight_id) != flights.end()) {
            writeToSocket(client_socket, "ERROR DuplicateFlightID\n");
            return;
        }
        flights[flight_id] = Flight(flight_id, origin, destination, time, rows, cols);
        writeToSocket(client_socket, "FLIGHT_ADDED OK\n");
        string log_msg = "New flight added: " + flight_id + " " + origin + "->" + destination + " at " + time + "\n";
        write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
        broadcastNewFlight(flight_id, origin, destination, time);
    }
    void handleCommand(int client_socket, const string& command) {
        vector<string> tokens;
        string error_msg;
        if (!parseCommand(command, tokens, error_msg)) {
            writeToSocket(client_socket, error_msg);
            return;
        }
        string cmd = tokens[0];
        if (cmd == "LIST_FLIGHTS") {
            handleListFlights(client_socket);
        } 
        else if (cmd == "REGISTER") {
            handleRegister(client_socket, tokens);
        }
        else if (cmd == "LOGIN") {
            handleLogin(client_socket, tokens);
        }
        else if (cmd == "LOGOUT") {
            handleLogout(client_socket, tokens);
        }
        else if (cmd == "ADD_FLIGHT") {
            handleAddFlight(client_socket, tokens);
        }
        else {
            writeToSocket(client_socket, "ERROR Unknown command\n");
        }
    }
    void handleNewConnection() {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int new_socket = ::accept(tcp_socket, (sockaddr*)&client_addr, &client_len);
        if (new_socket < 0) {
            perror("accept failed");
            return;
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = new_socket;
                string conn_msg = "New client connected. Socket: " + intToString(new_socket) + "\n";
                write(STDOUT_FILENO, conn_msg.c_str(), conn_msg.length());
                break;
            }
        }
    }
    void handleClientMessage(int client_socket, int index) {
        char buffer[BUFFER_SIZE];
        int n = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (n == 0) {
            for (auto& user_pair : users) {
                if (user_pair.second.getSocket() == client_socket) {
                    user_pair.second.setLoggedIn(false);
                    user_pair.second.setSocket(UN_INITIALIZED_SOCKET);
                    string logout_msg = "User logged out due to disconnect: " + user_pair.first + "\n";
                    write(STDOUT_FILENO, logout_msg.c_str(), logout_msg.length());
                    break;
                }
            }
            string msg = "Client disconnected. Socket: " + intToString(client_socket) + "\n";
            write(STDOUT_FILENO, msg.c_str(), msg.length());
            close(client_socket);
            client_sockets[index] = 0;
        } else if (n > 0) {
            buffer[n] = '\0';
            string command(buffer);
            if (!command.empty() && command.back() == '\n') {
                command.pop_back();
            }
            string log_msg = "Received from client " + intToString(client_socket) + ": " + command + "\n";
            write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
            handleCommand(client_socket, command);
        }
    }
};

int main() {
    AirlineServer server;
    server.run();
    return 0;
}