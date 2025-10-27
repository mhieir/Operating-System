#include "common.hpp"

volatile sig_atomic_t alarm_triggered_flag = 0;

vector<string> tokenize(const string& str) {
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

string intToString(int value) {
    return to_string(value);
}

void writeToSocket(int socket, const string& message) {
    write(socket, message.c_str(), message.length());
}

void set_alarm_flag(int sig) {
    alarm_triggered_flag = true;
    alarm(1);
}

string readFromSocket(int socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = read(socket, buffer, BUFFER_SIZE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        return string(buffer);
    }
    return "";
}

bool isNumber(string s) {
    for(char x : s) {
        if( x - '0' < 0 || '9' - x < 0) return false;
    }
    return true;
}

void put_error(const char *prefix)
{
    const char *msg = strerror(errno);
    write(STDERR_FILENO, prefix, strlen(prefix));
    write(STDERR_FILENO, ": ", 2);
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, "\n", 1);
}