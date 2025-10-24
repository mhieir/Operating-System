#ifndef COMMON_HPP
#define COMMON_HPP

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

using namespace std;

const int MAX_CLIENTS = 30;
const int BUFFER_SIZE = 1024;
const int TEMP_RESERVATION_TIMEOUT = 30;
const int UN_INITIALIZED_SOCKET = -1;
const int UN_INITIALIZED_RESERVATION_ID = -1;
const string LIST_FLIGHTS_CMD = "LIST_FLIGHTS";
const int LIST_FLIGHTS_CMD_SIZE = 1;
const string LOGIN_CMD = "LOGIN";
const int LOGIN_CMD_SIZE = 3;
const string REGISTER_CMD = "REGISTER";
const int REGISTER_CMD_SIZE = 4;
const string ADD_FLIGHT_CMD = "ADD_FLIGHT";
const int ADD_FLIGHT_CMD_SIZE = 7;
const string RESERVE_CMD = "RESERVE";
const string CONFIRM_CMD = "CONFIRM";
const string CANCEL_CMD = "CANCEL";
const string EXIT_CMD = "EXIT";
const string UNVALID_CMD = "Unvalid command.\n";
const string EMPTY_CMD = "Empty command\n";
const string CUSTOMER_STRING = "CUSTOMER";
const string AIRLINE_STRING = "AIRLINE";

enum class UserRole {
    CUSTOMER,
    AIRLINE
};

enum class ReservationStatus {
    TEMPORARY,
    CONFIRMED
};

enum class SeatStatus {
    FREE,
    RESERVED
};

inline string intToString(int value) {
    char buffer[20];
    int len = 0;
    int temp = value;
    if (temp == 0) return "0";
    if (temp < 0) {
        buffer[len++] = '-';
        temp = -temp;
    }
    char rev_buffer[20];
    int rev_len = 0;
    while (temp > 0) {
        rev_buffer[rev_len++] = '0' + (temp % 10);
        temp /= 10;
    }
    for (int i = rev_len - 1; i >= 0; i--) {
        buffer[len++] = rev_buffer[i];
    }
    buffer[len] = '\0';
    return string(buffer);
}

class User {
private:
    string username, password;
    UserRole role;
    int socket_fd;
    bool logged_in;

public:
    User() : socket_fd(UN_INITIALIZED_SOCKET), logged_in(false) {}
    User(string username, string password, UserRole role) : username(username), password(password), role(role), socket_fd(UN_INITIALIZED_SOCKET), logged_in(false) {}
    const string& getUsername() const { return username; }
    const string& getPassword() const { return password; }
    UserRole getRole() const { return role; }
    int getSocket() const { return socket_fd; }
    bool isLoggedIn() const { return logged_in; }
    void setSocket(int sock) { socket_fd = sock; }
    void setLoggedIn(bool status) { logged_in = status; }
    bool isAirline() const { return role == UserRole::AIRLINE; }
    bool isCustomer() const { return role == UserRole::CUSTOMER; }
    string getRoleString() const {return (role == UserRole::AIRLINE) ? "AIRLINE" : "CUSTOMER";}
};

class Seat {
private:
    string seat_id, reserved_by;
    SeatStatus status;

public:
    Seat(string seat_id) : seat_id(seat_id), status(SeatStatus::FREE) {}
    const string& getId() const { return seat_id; }
    SeatStatus getStatus() const { return status; }
    const string& getReservedBy() const { return reserved_by; }
    bool isFree() const { return status == SeatStatus::FREE; }
    bool isReserved() const { return status == SeatStatus::RESERVED; }
    bool reserve(const string& username) {
        if (isReserved()) return false;
        status = SeatStatus::RESERVED;
        reserved_by = username;
        return true;
    }
    bool free() {
        if (isFree()) return false;
        status = SeatStatus::FREE;
        reserved_by.clear();
        return true;
    }
    bool isValid() const { return !seat_id.empty(); }
};

class Flight {
private:
    string flight_id, origin, destination, time;
    vector<Seat> seats;
    int rows, cols;

public:
    Flight() : rows(0), cols(0) {}
    Flight(string flight_id, string origin, string destination, string time, int rows, int cols) : flight_id(flight_id), origin(origin), destination(destination), time(time), rows(rows), cols(cols) {
        seats.clear();
        for (int col = 0; col < cols; col++) {
            char column_char = 'A' + col;
            for (int row = 1; row <= rows; row++) {
                string seat_id = string(1, column_char) + intToString(row);
                seats.emplace_back(seat_id);
            }
        }
    }

    const string& getId() const { return flight_id; }
    const string& getOrigin() const { return origin; }
    const string& getDestination() const { return destination; }
    const string& getTime() const { return time; }
    int getRows() const { return rows; }
    int getCols() const { return cols; }
    const vector<Seat>& getSeats() const { return seats; }
    int getAvailableSeats() const {
        int count = 0;
        for (const auto& seat : seats) {
            if (seat.isFree()) count++;
        }
        return count;
    }
    int getTotalSeats() const { return seats.size(); }
    bool reserveSeat(const string& seat_id, const string& username) {
        for(auto& seat : seats) {
            if(seat.getId() == seat_id) {
                if(seat.isFree()) {
                    seat.reserve(username);
                    return true;
                }
                return false;
            }
        }
    return false;
    }
    bool freeSeat(const string& seat_id) {
        for(auto& seat : seats) {
            if(seat.getId() == seat_id) {
                if(seat.isReserved()) {
                    seat.free();
                    return true;
                }
                return false;
            }
        }
        return false;
    }

    bool isSeatAvailable(const string& seat_id) const {
        for(const Seat& seat : seats) {
            if(seat.getId() == seat_id) {
                return seat.isFree();
            }
        }
        return false;
    }

    bool isValid() const {
        return !flight_id.empty() && !origin.empty() && !destination.empty() && !time.empty() && rows > 0 && cols > 0;
    }

    vector<string> getAvailableSeatList() const {
        vector<string> available;
        for (const auto& seat : seats) {
            if (seat.isFree()) {
                available.push_back(seat.getId());
            }
        }
        return available;
    }
};

class Reservation {
private:
    int reservation_id;
    string flight_id, username;
    vector<string> seats;
    ReservationStatus status;
    time_t timestamp;
    bool alarm_set;

public:
    Reservation(int reservation_id, string flight_id, string user) : reservation_id(reservation_id), flight_id(flight_id), username(user), status(ReservationStatus::TEMPORARY), alarm_set(false) {
        timestamp = time(nullptr);
    }
    Reservation() : reservation_id(-1), status(ReservationStatus::TEMPORARY), timestamp(0), alarm_set(false) {}


    int getId() const { return reservation_id; }
    const string& getFlightId() const { return flight_id; }
    const string& getUsername() const { return username; }
    const vector<string>& getSeats() const { return seats; }
    ReservationStatus getStatus() const { return status; }
    time_t getTimestamp() const { return timestamp; }
    bool isAlarmSet() const { return alarm_set; }
    void setStatus(ReservationStatus new_status) { status = new_status; }
    void setAlarm(bool set) { alarm_set = set; }
    void addSeat(string seat_id) {
        seats.push_back(seat_id);
    }
    bool containsSeat(const string& seat_id) const {
        for (const auto& seat : seats) {
            if (seat == seat_id) return true;
        }
        return false;
    }
    bool isTemporary() const { return status == ReservationStatus::TEMPORARY; }
    bool isConfirmed() const { return status == ReservationStatus::CONFIRMED; }
    bool isExpired() const {
        return isTemporary() && (time(nullptr) - timestamp >= TEMP_RESERVATION_TIMEOUT);
    }
    void confirm() {
        status = ReservationStatus::CONFIRMED;
        alarm_set = false;
    }
};



inline void writeToSocket(int socket, const string& message) {
    write(socket, message.c_str(), message.length());
}

inline string readFromSocket(int socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        return string(buffer);
    }
    return "";
}

inline vector<string> tokenize(const string& str) {
    vector<string> tokens;
    string token;
    for (char c : str) {
        if (c == ' ' || c == '\t' || c == '\n') {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) {
        tokens.push_back(token);
    }
    return tokens;
}

#endif
