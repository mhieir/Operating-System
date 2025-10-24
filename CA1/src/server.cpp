#include "common.hpp"
#include <csignal>

class AirlineServer;
AirlineServer* server_instance = nullptr;

void reservationTimeoutHandler(int signal);

class AirlineServer {
private:
    int tcp_socket;
    int udp_socket;
    vector<int> client_sockets;
    unordered_map<string, User> users;
    unordered_map<string, Flight> flights;
    unordered_map<int, Reservation> reservations;
    int reservation_counter;
    volatile bool alarm_triggered_flag;

public:
    AirlineServer(int port = 8080) : reservation_counter(1000), alarm_triggered_flag(false) {
        server_instance = this;
        struct sigaction sa;
        sa.sa_handler = reservationTimeoutHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (::sigaction(SIGALRM, &sa, NULL) == -1) {
            perror("sigaction failed");
            exit(EXIT_FAILURE);
        }

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
        const char* msg = "Server\n";
        write(STDOUT_FILENO, msg, strlen(msg));
    }

    ~AirlineServer() {
        close(tcp_socket);
        close(udp_socket);
        for (int sock : client_sockets) {
            if (sock > 0) close(sock);
        }
    }

    void handleAlarmSignal() {
        alarm_triggered_flag = true;
    }

    void run() {
        fd_set readfds;
        int max_sd, activity;
        const char* msg = "Server started. Waiting for connections...\n";
        write(STDOUT_FILENO, msg, strlen(msg));

        setNextAlarm();

        while (true) {
            if (alarm_triggered_flag) {
                freeExpiredReservations();
                setNextAlarm();
                alarm_triggered_flag = false;
            }

            FD_ZERO(&readfds);
            FD_SET(tcp_socket, &readfds);
            FD_SET(udp_socket, &readfds);
            max_sd = (tcp_socket > udp_socket) ? tcp_socket : udp_socket;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                int sd = client_sockets[i];
                if (sd > 0) FD_SET(sd, &readfds);
                if (sd > max_sd) max_sd = sd;
            }

            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            activity = ::select(max_sd + 1, &readfds, NULL, NULL, &timeout);

            if (activity < 0 && errno != EINTR) {
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
    // --- CORRECTED ALARM HANDLING ---
    void setNextAlarm() {
        time_t now = time(nullptr);
        time_t earliest_expiry = 0;

        // Find the earliest expiry time among temporary reservations
        for (const auto& pair : reservations) {
            const Reservation& res = pair.second;
            if (res.isTemporary() && !res.isExpired()) {
                time_t expiry_time = res.getTimestamp() + TEMP_RESERVATION_TIMEOUT;
                if (earliest_expiry == 0 || expiry_time < earliest_expiry) {
                    earliest_expiry = expiry_time;
                }
            }
        }

        if (earliest_expiry > 0) {
            unsigned int seconds_to_wait = earliest_expiry - now;
            // Ensure we wait at least 1 second
            if (seconds_to_wait == 0) seconds_to_wait = 1;
            ::alarm(seconds_to_wait);
            string log_msg = "Alarm set for " + intToString(seconds_to_wait) + " seconds\n";
            write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
        } else {
            ::alarm(0); // Cancel any pending alarm
            const char* msg = "No temporary reservations - alarm cancelled\n";
            write(STDOUT_FILENO, msg, strlen(msg));
        }
    }

    void freeExpiredReservations() {
        vector<int> expired_ids;
        
        // Collect IDs of expired reservations
        for (const auto& pair : reservations) {
            if (pair.second.isExpired()) {
                expired_ids.push_back(pair.first);
            }
        }

        // Free seats and remove expired reservations
        for (int id : expired_ids) {
            auto it = reservations.find(id);
            if (it != reservations.end()) {
                Flight* flight = getFlight(it->second.getFlightId());
                if (flight) {
                    for (const auto& seat_id : it->second.getSeats()) {
                        flight->freeSeat(seat_id);
                    }
                }
                string msg = "Reservation " + intToString(id) + " expired and freed\n";
                write(STDOUT_FILENO, msg.c_str(), msg.length());
                reservations.erase(it);
            }
        }
    }


    User* getUserBySocket(int socket) {
        for (auto& user_pair : users) {
            if (user_pair.second.getSocket() == socket && user_pair.second.isLoggedIn()) {
                return &user_pair.second;
            }
        }
        return nullptr;
    }
    Flight* getFlight(const string& flight_id) {
        auto it = flights.find(flight_id);
        return (it != flights.end()) ? &it->second : nullptr;
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
        else if (cmd == "RESERVE") {
            if (tokens.size() < 3) {
                error_msg = "ERROR Invalid RESERVE format. Use: RESERVE <flight_id> <seat1> [seat2] ...\n";
                return false;
            }
            return true;
        }
        else if (cmd == "CONFIRM") {
            if (tokens.size() != 2) {
                error_msg = "ERROR Invalid CONFIRM format. Use: CONFIRM <reservation_id>\n";
                return false;
            }
            return true;
        }
        else if (cmd == "CANCEL") {
            if (tokens.size() != 2) {
                error_msg = "ERROR Invalid CANCEL format. Use: CANCEL <reservation_id>\n";
                return false;
            }
            return true;
        }

        error_msg = "ERROR Unknown command.\n";
        return false;
    }
    bool registerUser(const string& role_str, const string& username, const string& password) {
        if (users.find(username) != users.end()) {
            return false;
        }
        UserRole role = (role_str == "AIRLINE") ? UserRole::AIRLINE : UserRole::CUSTOMER;
        users[username] = User(username, password, role);
        return true;
    }
    bool loginUser(const string& username, const string& password, int client_socket) {
        auto it = users.find(username);
        if (it == users.end() || it->second.getPassword() != password || it->second.isLoggedIn()) {
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
        if (registerUser(tokens[1], tokens[2], tokens[3])) {
            writeToSocket(client_socket, "REGISTERED OK\n");
            string log_msg = "New user registered: " + tokens[2] + " (" + tokens[1] + ")\n";
            write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
            broadcastNewUser(tokens[2], tokens[1]);
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
        (void)tokens;
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
    User* user = getUserBySocket(client_socket);
    if (!user || !user->isAirline()) {
        writeToSocket(client_socket, "ERROR PermissionDenied\n");
        return;
    }
    
    string flight_id = tokens[1];
    if (flights.count(flight_id)) {
        writeToSocket(client_socket, "ERROR DuplicateFlightID\n");
        return;
    }
    flights[flight_id] = Flight(flight_id, tokens[2], tokens[3], tokens[4], stoi(tokens[5]), stoi(tokens[6]));
    writeToSocket(client_socket, "FLIGHT_ADDED OK\n");
    string log_msg = "New flight added: " + flight_id + "\n";
    write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
    broadcastNewFlight(flight_id, tokens[2], tokens[3], tokens[4]);
}

    void handleReserve(int client_socket, const vector<string>& tokens) {
        User* user = getUserBySocket(client_socket);
        if (!user || !user->isCustomer()) {
            writeToSocket(client_socket, "ERROR PermissionDenied\n");
            return;
        }
        Flight* flight = getFlight(tokens[1]);
        if (!flight) {
            writeToSocket(client_socket, "ERROR FlightNotFound\n");
            return;
        }
        vector<string> seats_to_reserve;
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (!flight->isSeatAvailable(tokens[i])) {
                writeToSocket(client_socket, "ERROR SeatNotAvailable: " + tokens[i] + "\n");
                return;
            }
            seats_to_reserve.push_back(tokens[i]);
        }
        for (const auto& seat_id : seats_to_reserve) {
            flight->reserveSeat(seat_id, user->getUsername());
        }
        int res_id = reservation_counter++;
        Reservation res(res_id, flight->getId(), user->getUsername());
        for (const auto& seat_id : seats_to_reserve) {
            res.addSeat(seat_id);
        }
        reservations[res_id] = res;
        string log_msg = "Created temporary reservation " + intToString(res_id) + " for user " + user->getUsername() + "\n";
        write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
        writeToSocket(client_socket, "RESERVED TEMP " + intToString(res_id) + " EXPIRES IN 30\n");
        setNextAlarm();
    }
    void handleConfirm(int client_socket, const vector<string>& tokens) {
        User* user = getUserBySocket(client_socket);
        if (!user) {
            writeToSocket(client_socket, "ERROR NotLoggedIn\n");
            return;
        }
        int res_id = stoi(tokens[1]);
        auto it = reservations.find(res_id);
        if (it == reservations.end()) {
            writeToSocket(client_socket, "ERROR ReservationNotFound\n");
            return;
        }
        Reservation& res = it->second;
        if (res.getUsername() != user->getUsername()) {
            writeToSocket(client_socket, "ERROR PermissionDenied\n");
            return;
        }
        if (res.isExpired()) {
            writeToSocket(client_socket, "ERROR ReservationExpired\n");
            Flight* flight = getFlight(res.getFlightId());
            if (flight) {
                for (const auto& seat_id : res.getSeats()) {
                    flight->freeSeat(seat_id);
                }
            }
            reservations.erase(it);
            setNextAlarm();
        } else if (res.isTemporary()) {
            res.confirm();
            writeToSocket(client_socket, "CONFIRMATION OK\n");
            setNextAlarm();
        } else {
            writeToSocket(client_socket, "ERROR AlreadyConfirmed\n");
        }
    }
    void handleCancel(int client_socket, const vector<string>& tokens) {
        User* user = getUserBySocket(client_socket);
        if (!user) {
            writeToSocket(client_socket, "ERROR NotLoggedIn\n");
            return;
        }
        int res_id = stoi(tokens[1]);
        auto it = reservations.find(res_id);
        if (it == reservations.end()) {
            writeToSocket(client_socket, "ERROR ReservationNotFound\n");
            return;
        }
        if (it->second.getUsername() != user->getUsername()) {
            writeToSocket(client_socket, "ERROR PermissionDenied\n");
            return;
        }
        Flight* flight = getFlight(it->second.getFlightId());
        if (flight) {
            for (const auto& seat_id : it->second.getSeats()) {
                flight->freeSeat(seat_id);
            }
        }
        reservations.erase(it);
        writeToSocket(client_socket, "CANCELLED OK\n");
        setNextAlarm();
    }

    void handleCommand(int client_socket, const string& command) {
        vector<string> tokens;
        string error_msg;
        if (!parseCommand(command, tokens, error_msg)) {
            writeToSocket(client_socket, error_msg);
            return;
        }
        string cmd = tokens[0];
        if (cmd == "LIST_FLIGHTS") handleListFlights(client_socket);
        else if (cmd == "REGISTER") handleRegister(client_socket, tokens);
        else if (cmd == "LOGIN") handleLogin(client_socket, tokens);
        else if (cmd == "LOGOUT") handleLogout(client_socket, tokens);
        else if (cmd == "ADD_FLIGHT") handleAddFlight(client_socket, tokens);
        else if (cmd == "RESERVE") handleReserve(client_socket, tokens);
        else if (cmd == "CONFIRM") handleConfirm(client_socket, tokens);
        else if (cmd == "CANCEL") handleCancel(client_socket, tokens);
        else writeToSocket(client_socket, "ERROR Unknown command\n");
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
                return;
            }
        }
    }
    void handleClientMessage(int client_socket, int index) {
        char buffer[BUFFER_SIZE];
        int n = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (n <= 0) {
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
        } else {
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

void reservationTimeoutHandler(int signal) {
    if (signal == SIGALRM && server_instance) {
        server_instance->handleAlarmSignal();
    }
}

int main() {
    AirlineServer server;
    server.run();
    return 0;
}