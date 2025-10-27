#ifndef COMMON_HPP
#define COMMON_HPP

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <signal.h>
#include <exception>
#include <csignal>

using namespace std;

const int MAX_CLIENTS = 10;
const int BUFFER_SIZE = 1024;
const int UN_INITIALIZED_SOCKET = -1;
const int RESERVATION_EXPIRY_SECONDS = 30;
const string NEW_FLIGHT = "NEW_FLIGHT";
const string NEW_USER = "NEW_USER";
const string BROADCAST_AIRLINES = "BROADCAST to AIRLINES: ";
const string BROADCAST_CUSTOMERS = "BROADCAST to CUSTOMERS: ";
const string REGISTER_CMD = "REGISTER";
const string LOGIN_CMD = "LOGIN";
const string LIST_FLIGHTS_CMD = "LIST_FLIGHTS";
const int LIST_FLIGHTS_SIZE = 1;
const string ADD_FLIGHT_CMD = "ADD_FLIGHT";
const int ADD_FLIGHT_SIZE = 7;
const string RESERVE_CMD = "RESERVE";
const int RESERVE_SIZE = 3;
const string CONFIRM_CMD = "CONFIRM";
const int CONFIRM_SIZE = 2;
const string CANCEL_CMD = "CANCEL";
const int CANCEL_SIZE = 2;
const string AIRLINE_STRING = "AIRLINE";
const string CUSTOMER_STRING = "CUSTOMER";
const int REGISTER_CMD_SIZE = 4;
const int LOGIN_CMD_SIZE = 3;
const string SPACE = " ";
const string NEXT_LINE = "\n";
const string BACK_SLASH =  "/";
const string FLIGHT_STRING = "FLIGHT ";
const string SEAT_AVAILABLE = " SEATS_AVAILABLE=";
const string USER_ALREADY_LOGGED_IN = "ERROR UserAlreadyLoggedIn\n";
const string EMPTY_CMD = "ERROR Empty command\n";
const string INVALID_CMD = "ERROR Invalid.\n";
const string REGISTERED_OK = "REGISTERED OK\n";
const string USERNAME_ALREADY_EXIST = "ERROR UsernameAlreadyExists\n";
const string LOGIN_OK = "LOGIN OK\n";
const string USER_NOT_FOUND = "ERROR UserNotFound\n";
const string INVALID_PASSWORD = "ERROR InvalidPassword\n";
const string FLIGHT_ADDED_OK = "FLIGHT_ADDED OK\n";
const string RESERVED_TEMP = "RESERVED TEMP ";
const string EXPIRES_IN_30 = " EXPIRES IN 30\n";
const string RESERVATION_EXPIRED = "ERROR ReservationExpired\n";
const string CONFIRMATION_OK = "CONFIRMATION OK\n";
const string CANCELLED_OK = "CANCELLED OK\n";
const string EXIT_CMD = "EXIT_CMD";

enum class UserRole {
    CUSTOMER,
    AIRLINE
};

class User {
private:
    string username, password;
    UserRole role;
    int socket_fd;
    bool logged_in;

public:
    User(string uname = "", string pwd = "", UserRole r = UserRole::CUSTOMER) : username(uname), password(pwd), role(r), socket_fd(UN_INITIALIZED_SOCKET), logged_in(false) {}
    bool isLoggedIn() const { return logged_in; }
    void setLoggedIn(bool status) { logged_in = status; }
    int getSocket() const { return socket_fd; }
    void setSocket(int sd) { socket_fd = sd; }
    string getUsername() const { return username; }
    string getPassword() const { return password; }
    bool isCustomer() const { return role == UserRole::CUSTOMER; }
    bool isAirline() const { return role == UserRole::AIRLINE; }
};

class Flight {
private:
    string id, origin, destination, time;
    int rows, cols;
    unordered_map<string, string> seats;

public:
    Flight(string fid = "", string orig = "", string dest = "", string t = "", int r = 0, int c = 0) : id(fid), origin(orig), destination(dest), time(t), rows(r), cols(c) {}
    string getId() const { return id; }
    string getOrigin() const { return origin; }
    string getDestination() const { return destination; }
    string getTime() const { return time; }
    int getAvailableSeats() const {
        int count = 0;
        for (const auto& seat : seats) {
            if (seat.second.empty()) count++;
        }
        return rows * cols - seats.size() + count;
    }
    int getTotalSeats() const { return rows * cols; }
    bool isSeatAvailable(const string& seat_id) {
        auto it = seats.find(seat_id);
        return it == seats.end() || it->second.empty();
    }
    void reserveSeat(const string& seat_id, const string& username) {
        seats[seat_id] = username;
    }
    void freeSeat(const string& seat_id) {
        seats.erase(seat_id);
    }
};

class Reservation {
private:
    int id;
    string flight_id, username;
    vector<string> seats;
    time_t created_time;
    bool confirmed;

public:
    Reservation(int rid = 0, string fid = "", string uname = "") : id(rid), flight_id(fid), username(uname), created_time(time(nullptr)), confirmed(false) {}
    int getId() const { return id; }
    string getFlightId() const { return flight_id; }
    string getUsername() const { return username; }
    vector<string> getSeats() const { return seats; }
    bool isTemporary() const { return !confirmed; }
    bool isConfirmed() const { return confirmed; }
    void addSeat(const string& seat_id) { seats.push_back(seat_id); }
    void confirm() { confirmed = true; }
    bool isExpired() const {
        return !confirmed && (time(nullptr) - created_time) > RESERVATION_EXPIRY_SECONDS;
    }
};



extern volatile sig_atomic_t alarm_triggered_flag;

vector<string> tokenize(const string& str);
string intToString(int value);
void writeToSocket(int socket, const string& message);
void set_alarm_flag(int sig);
string readFromSocket(int socket);
bool isNumber(string s);
void put_error(const char *prefix);

#endif