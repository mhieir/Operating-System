#ifndef AIRLINE_SERVER_HPP
#define AIRLINE_SERVER_HPP

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
    void freeExpiredReservations();
    User* getUserBySocket(int socket);
    Flight* getFlight(const string& flight_id);
    void broadcastToAirlines(const string& message);
    void broadcastToCustomers(const string& message);
    void broadcastNewUser(const string& username, const string& role);
    void broadcastNewFlight(const string& flight_id, const string& origin, const string& destination, const string& time);
    void handleUDPBroadcast();
    bool parseCommand(const string& command, vector<string>& tokens, string& error_msg);
    bool registerUser(const string& role_str, const string& username, const string& password);
    bool loginUser(const string& username, const string& password, int client_socket);
    void handleListFlights(int client_socket);
    void handleRegister(int client_socket, const vector<string>& tokens);
    void handleLogin(int client_socket, const vector<string>& tokens);
    void handleLogout(int client_socket, const vector<string>& tokens);
    void handleAddFlight(int client_socket, const vector<string>& tokens);
    void handleReserve(int client_socket, const vector<string>& tokens);
    void handleConfirm(int client_socket, const vector<string>& tokens);
    void handleCancel(int client_socket, const vector<string>& tokens);
    void handleCommand(int client_socket, const string& command);
    void handleNewConnection();
    void handleClientMessage(int client_socket, int index);
    void findExpiredIds(vector<int>& expired_ids);

public:
    AirlineServer(int port = 8080);
    ~AirlineServer();
    void run();
};

#endif