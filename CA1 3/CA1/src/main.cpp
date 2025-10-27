#include "common.hpp"
#include "server.hpp"

int main() {
    signal(SIGALRM, set_alarm_flag);
    alarm(1);
    AirlineServer server;
    server.run();
    return 0;
}