#include "common.hpp"
#include "server.hpp"

AirlineServer::AirlineServer(int port) : reservation_counter(0) {
    client_sockets.resize(MAX_CLIENTS, 0);
    tcp_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        put_error("TCP socket creation failed");
        exit(1);
    }
    udp_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        put_error("UDP socket creation failed");
        exit(1);
    }
    sockaddr_in server_addr_tcp;
    memset(&server_addr_tcp, 0, sizeof(server_addr_tcp));
    server_addr_tcp.sin_family = AF_INET;
    server_addr_tcp.sin_addr.s_addr = INADDR_ANY;
    server_addr_tcp.sin_port = htons(8081);
    int opt = 1;
    if (::setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        put_error("setsockopt failed");
        exit(1);
    }

    sockaddr_in server_addr_udp;
    memset(&server_addr_udp, 0, sizeof(server_addr_udp));
    server_addr_udp.sin_family = AF_INET;
    server_addr_udp.sin_addr.s_addr = INADDR_ANY;
    server_addr_udp.sin_port = htons(8081);

    int broadcast_en = 1;
    if(::setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_en, sizeof(broadcast_en)) < 0) {
        put_error("setsockopt failed");
        exit(1);
    }
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("UDP setsockopt");
        exit(EXIT_FAILURE);
    }
    if (::bind(tcp_socket, (sockaddr*)&server_addr_tcp, sizeof(server_addr_tcp)) < 0) {
        put_error("TCP bind failed");
        exit(1);
    }
    if (::bind(udp_socket, (sockaddr*)&server_addr_udp, sizeof(server_addr_udp)) < 0) {
        put_error("UDPfuihsdfhsdukyfghygf bind failed");
        exit(1);
    }
    if (::listen(tcp_socket, 5) < 0) {
        put_error("listen failed");
        exit(1);
    }
}

AirlineServer::~AirlineServer() {
    close(tcp_socket);
    close(udp_socket);
    for (int sock : client_sockets) {
        if (sock > 0) close(sock);
    }
}

void AirlineServer::run() {
    fd_set readfds;
    int max_sd, activity;

    while (true) {

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

        if (activity < 0) {
            if(errno == EINTR) {
                if (alarm_triggered_flag) {
                    freeExpiredReservations();
                    alarm_triggered_flag = false;
                }
                continue;
            }
            put_error("select error");
        }

        if (FD_ISSET(tcp_socket, &readfds)) {
            handleNewConnection();
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                handleClientMessage(sd, i);
            }
        }
    }
}

void AirlineServer::findExpiredIds(vector<int>& expired_ids) {
    for (const auto& pair : reservations) {
        if (pair.second.isExpired()) {
            expired_ids.push_back(pair.first);
        }
    }
}

void AirlineServer::freeExpiredReservations() {
    vector<int> expired_ids;
    findExpiredIds(expired_ids);
    for (int id : expired_ids) {
        auto it = reservations.find(id);
        if (it != reservations.end()) {
            Flight* flight = getFlight(it->second.getFlightId());
            if (flight) {
                for (const auto& seat_id : it->second.getSeats()) {
                    flight->freeSeat(seat_id);
                }
            }
            reservations.erase(it);
        }
    }
}


User* AirlineServer::getUserBySocket(int socket) {
    for (auto& user_pair : users) {
        if (user_pair.second.getSocket() == socket && user_pair.second.isLoggedIn()) {
            return &user_pair.second;
        }
    }
    return nullptr;
}

Flight* AirlineServer::getFlight(const string& flight_id) {
    auto it = flights.find(flight_id);
    return (it != flights.end()) ? &it->second : nullptr;
}

void AirlineServer::broadcastToAirlines(const string& message) {
    sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast_addr.sin_port = htons(8081);
    sendto(udp_socket, message.c_str(), message.length(), 0, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    string log_msg = BROADCAST_AIRLINES + message + NEXT_LINE;
    write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
}

void AirlineServer::broadcastToCustomers(const string& message) {
    sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast_addr.sin_port = htons(8081);
    sendto(udp_socket, message.c_str(), message.length(), 0, (sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    string log_msg = BROADCAST_CUSTOMERS + message + NEXT_LINE;
    write(STDOUT_FILENO, log_msg.c_str(), log_msg.length());
}

void AirlineServer::broadcastNewUser(const string& username, const string& role) {
    string message = NEW_USER + username + SPACE + role;
    broadcastToAirlines(message);
}

void AirlineServer::broadcastNewFlight(const string& flight_id, const string& origin, const string& destination, const string& time) {
    string message = NEW_FLIGHT + flight_id + SPACE + origin + SPACE + destination + SPACE + time;
    broadcastToCustomers(message);
}

bool AirlineServer::parseCommand(const string& command, vector<string>& tokens, string& error_msg) {
    tokens = tokenize(command);
    if (tokens.empty()) {
        error_msg = EMPTY_CMD;
        return false;
    }
    string cmd = tokens[0];
    if(cmd == REGISTER_CMD && tokens.size() == REGISTER_CMD_SIZE && (tokens[1] == AIRLINE_STRING || tokens[1] == CUSTOMER_STRING)) {
        return true;
    }
    else if (cmd == LOGIN_CMD && tokens.size() == LOGIN_CMD_SIZE) {
        return true;
    }
    else if (cmd == LIST_FLIGHTS_CMD && tokens.size() == LIST_FLIGHTS_SIZE) {
        return true;
    }
    else if (cmd == ADD_FLIGHT_CMD && tokens.size() == ADD_FLIGHT_SIZE) {
        return true;
    }
    else if (cmd == RESERVE_CMD && tokens.size() >= RESERVE_SIZE) {
        return true;
    }
    else if (cmd == CONFIRM_CMD && tokens.size() == CONFIRM_SIZE) {
        return true;
    }
    else if (cmd == CANCEL_CMD && tokens.size() == CANCEL_SIZE) {
        return true;
    }
    error_msg = INVALID_CMD;
    return false;
}

bool AirlineServer::registerUser(const string& role_str, const string& username, const string& password) {
    if (users.find(username) != users.end()) {
        return false;
    }
    UserRole role = (role_str == AIRLINE_STRING) ? UserRole::AIRLINE : UserRole::CUSTOMER;
    users[username] = User(username, password, role);
    return true;
}

bool AirlineServer::loginUser(const string& username, const string& password, int client_socket) {
    auto it = users.find(username);
    if (it == users.end() || it->second.getPassword() != password || it->second.isLoggedIn()) {
        return false;
    }
    it->second.setLoggedIn(true);
    it->second.setSocket(client_socket);
    return true;
}

void AirlineServer::handleListFlights(int client_socket) {
    string response;
    if (flights.empty()) {
        response = NEXT_LINE;
    } else {
        for (const auto& pair : flights) {
            const Flight& flight = pair.second;
            response += FLIGHT_STRING + flight.getId() + SPACE + flight.getOrigin() + SPACE + flight.getDestination() + SPACE + flight.getTime() + SEAT_AVAILABLE + intToString(flight.getAvailableSeats()) + BACK_SLASH + intToString(flight.getTotalSeats()) + NEXT_LINE;
        }
        writeToSocket(client_socket, response);
    }
}

void AirlineServer::handleRegister(int client_socket, const vector<string>& tokens) {
    User* my_user = getUserBySocket(client_socket);
    if(my_user != nullptr && my_user->isLoggedIn()) {
        writeToSocket(client_socket, USER_ALREADY_LOGGED_IN);
    }
    else if (registerUser(tokens[1], tokens[2], tokens[3])) {
        writeToSocket(client_socket, REGISTERED_OK);
        broadcastNewUser(tokens[2], tokens[1]);
    } else {
        writeToSocket(client_socket, USERNAME_ALREADY_EXIST);
    }
}

void AirlineServer::handleLogin(int client_socket, const vector<string>& tokens) {
    User* my_user = getUserBySocket(client_socket);
    string username = tokens[1];
    string password = tokens[2];
    if(my_user != nullptr && my_user->isLoggedIn()) {
        writeToSocket(client_socket, USER_ALREADY_LOGGED_IN);
    }
    else if (loginUser(username, password, client_socket)) {
        writeToSocket(client_socket, LOGIN_OK);
    } else {
        auto it = users.find(username);
        if (it == users.end()) {
            writeToSocket(client_socket, USER_NOT_FOUND);
        } else if (it->second.getPassword() != password) {
            writeToSocket(client_socket, INVALID_PASSWORD);
        } else if (it->second.isLoggedIn()) {
            writeToSocket(client_socket, USER_ALREADY_LOGGED_IN);
        } else {
            writeToSocket(client_socket, INVALID_CMD);
        }
    }
}

void AirlineServer::handleAddFlight(int client_socket, const vector<string>& tokens) {
    User* user = getUserBySocket(client_socket);
    string flight_id = tokens[1];
    if ((!user || !user->isAirline()) || flights.count(flight_id)) {
        writeToSocket(client_socket, INVALID_CMD);
        return;
    }
    flights[flight_id] = Flight(flight_id, tokens[2], tokens[3], tokens[4], stoi(tokens[5]), stoi(tokens[6]));
    writeToSocket(client_socket, FLIGHT_ADDED_OK);
    broadcastNewFlight(flight_id, tokens[2], tokens[3], tokens[4]);
}

void AirlineServer::handleReserve(int client_socket, const vector<string>& tokens) {
    User* user = getUserBySocket(client_socket);
    Flight* flight = getFlight(tokens[1]);
    vector<string> seats_to_reserve;
    if ((!user || !user->isCustomer()) || (!flight)) {
        writeToSocket(client_socket, INVALID_CMD);
        return;
    }
    for (size_t i = 2; i < tokens.size(); ++i) {
        if (!flight->isSeatAvailable(tokens[i])) {
            writeToSocket(client_socket, INVALID_CMD);
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
    writeToSocket(client_socket, RESERVED_TEMP + intToString(res_id) + EXPIRES_IN_30);
}

void AirlineServer::handleConfirm(int client_socket, const vector<string>& tokens) {
    User* user = getUserBySocket(client_socket);
    if (!user || !isNumber(tokens[1])) {
        writeToSocket(client_socket, INVALID_CMD);
        return;
    }
    auto it = reservations.find(stoi(tokens[1]));
    if (it == reservations.end()) {
        writeToSocket(client_socket, INVALID_CMD);
        return;
    }
    Reservation& res = it->second;
    if (res.getUsername() != user->getUsername()) {
        writeToSocket(client_socket, INVALID_CMD);
    }
    else if (res.isExpired()) {
        writeToSocket(client_socket, RESERVATION_EXPIRED);
        Flight* flight = getFlight(res.getFlightId());
        if (flight) {
            for (const auto& seat_id : res.getSeats()) {
                flight->freeSeat(seat_id);
            }
        }
        reservations.erase(it);
    }
    else if (res.isTemporary()) {
        res.confirm();
        writeToSocket(client_socket, CONFIRMATION_OK);
    }
    else {
        writeToSocket(client_socket, INVALID_CMD);
    }
}

void AirlineServer::handleCancel(int client_socket, const vector<string>& tokens) {
    User* user = getUserBySocket(client_socket);
    if (!user || !isNumber(tokens[1])) {
        writeToSocket(client_socket, INVALID_CMD);
        return;
    }
    auto it = reservations.find(stoi(tokens[1]));
    if (it == reservations.end() || it->second.getUsername() != user->getUsername()) {
        writeToSocket(client_socket,INVALID_CMD);
        return;
    }
    Flight* flight = getFlight(it->second.getFlightId());
    if (flight) {
        for (const auto& seat_id : it->second.getSeats()) {
            flight->freeSeat(seat_id);
        }
    }
    reservations.erase(it);
    writeToSocket(client_socket, CANCELLED_OK);
}

void AirlineServer::handleCommand(int client_socket, const string& command) {
    vector<string> tokens;
    string error_msg;
    if (!parseCommand(command, tokens, error_msg)) {
        writeToSocket(client_socket, error_msg);
        return;
    }
    if (tokens[0] == LIST_FLIGHTS_CMD) handleListFlights(client_socket);
    else if (tokens[0] == REGISTER_CMD) handleRegister(client_socket, tokens);
    else if (tokens[0] == LOGIN_CMD) handleLogin(client_socket, tokens);
    else if (tokens[0] == ADD_FLIGHT_CMD) handleAddFlight(client_socket, tokens);
    else if (tokens[0] == RESERVE_CMD) handleReserve(client_socket, tokens);
    else if (tokens[0] == CONFIRM_CMD) handleConfirm(client_socket, tokens);
    else if (tokens[0] == CANCEL_CMD) handleCancel(client_socket, tokens);
    else writeToSocket(client_socket, INVALID_CMD);
}

void AirlineServer::handleNewConnection() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int new_socket = ::accept(tcp_socket, (sockaddr*)&client_addr, &client_len);
    if (new_socket < 0) {
        put_error("accept failed");
        return;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == 0) {
            client_sockets[i] = new_socket;
            return;
        }
    }
}
void AirlineServer::handleClientMessage(int client_socket, int index) {
    char buffer[BUFFER_SIZE];
    int n = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (n <= 0) {
        for (auto& user_pair : users) {
            if (user_pair.second.getSocket() == client_socket) {
                user_pair.second.setLoggedIn(false);
                user_pair.second.setSocket(UN_INITIALIZED_SOCKET);
                break;
            }
        }
        close(client_socket);
        client_sockets[index] = 0;
    } else {
        buffer[n] = '\0';
        string command(buffer);
        if (!command.empty() && command.back() == '\n') {
            command.pop_back();
        }
        handleCommand(client_socket, command);
    }
}