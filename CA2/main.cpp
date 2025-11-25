#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <algorithm>
#include <cerrno> 

using namespace std;

// Constants for configuration
const char* FIFO_PATH = "/tmp/os_ca2_fifo";
const string TOKEN_REDIRECT_OUT = "> np";
const string TOKEN_REDIRECT_IN = "< np";
const string CMD_EXIT = "exit";
const string PROMPT = "$ ";
const string MSG_HELLO = "hello";
const mode_t FIFO_MODE = 0666;

vector<string> split(const string &s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        size_t first = token.find_first_not_of(" \t");
        if (first == string::npos) continue;
        size_t last = token.find_last_not_of(" \t");
        tokens.push_back(token.substr(first, (last - first + 1)));
    }
    return tokens;
}

void cmd_echo(const vector<string> &args) {
    if (args.size() == 1) {
        char c;
        while (read(STDIN_FILENO, &c, 1) > 0) {
            if (c == '\n') break;
            cout << c;
        }
        cout << endl;
        return;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        cout << args[i] << (i == args.size() - 1 ? "" : " ");
    }
    cout << endl;
}

void cmd_print_n(const vector<string> &args) {
    if (args.size() < 2) return;
    int n = stoi(args[1]);
    for (int i = 0; i < n; ++i) {
        cout << MSG_HELLO;
        if (i < n - 1) cout << ", ";
    }
    cout << endl;
}

void cmd_len() {
    string line, content;
    while (getline(cin, line)) {
        content += line;
    }
    cout << content.length() << endl;
}

void setup_fifo_writer() {
    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open fifo write");
        exit(1);
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

void setup_fifo_reader() {
    int fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("open fifo read");
        exit(1);
    }

    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    dup2(fd, STDIN_FILENO);
    close(fd);
}

int initialize_named_pipe() {
    unlink(FIFO_PATH);
    if (mkfifo(FIFO_PATH, FIFO_MODE) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            return -1;
        }
    }
    int fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (fd == -1) perror("open dummy fifo");
    return fd;
}

void cleanup_named_pipe(int dummy_fd) {
    if (dummy_fd != -1) close(dummy_fd);
    unlink(FIFO_PATH);
}

void execute_single_command(string cmd_str) {
    bool out_to_np = false;
    bool in_from_np = false;

    size_t out_pos = cmd_str.find(TOKEN_REDIRECT_OUT);
    if (out_pos != string::npos) {
        out_to_np = true;
        cmd_str = cmd_str.substr(0, out_pos);
    }

    size_t in_pos = cmd_str.find(TOKEN_REDIRECT_IN);
    if (in_pos != string::npos) {
        in_from_np = true;
        cmd_str = cmd_str.substr(0, in_pos);
    }

    vector<string> args = split(cmd_str, ' ');
    if (args.empty()) exit(0);

    if (out_to_np) setup_fifo_writer();
    if (in_from_np) setup_fifo_reader();

    if (args[0] == "echo") cmd_echo(args);
    else if (args[0] == "print_n") cmd_print_n(args);
    else if (args[0] == "len") cmd_len();
    else cerr << "Unknown command: " << args[0] << endl;

    exit(0);
}

void execute_pipeline(const string &input) {
    vector<string> pipe_parts = split(input, '|');
    int num_cmds = pipe_parts.size();

    if (num_cmds == 0) return;

    int prev_pipe_read = -1;
    vector<pid_t> child_pids;

    for (int i = 0; i < num_cmds; ++i) {
        int pipefd[2];
        if (i < num_cmds - 1) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                break;
            }
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (prev_pipe_read != -1) {
                dup2(prev_pipe_read, STDIN_FILENO);
                close(prev_pipe_read);
            }

            if (i < num_cmds - 1) {
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }

            execute_single_command(pipe_parts[i]);
        } else if (pid < 0) {
            perror("fork");
        } else {
            child_pids.push_back(pid);

            if (prev_pipe_read != -1) {
                close(prev_pipe_read);
            }

            if (i < num_cmds - 1) {
                prev_pipe_read = pipefd[0];
                close(pipefd[1]);
            }
        }
    }

    for (pid_t pid : child_pids) waitpid(pid, NULL, 0);
}

int main() {
    int dummy_fd = initialize_named_pipe();
    if (dummy_fd == -1) return 1;

    string input;
    cout << "Welcome. Type 'exit' to quit." << endl;

    while (true)
    {
        cout << PROMPT;
        if (!getline(cin, input))
            break;
        if (input == CMD_EXIT)
            break;

        execute_pipeline(input);
    }

    cleanup_named_pipe(dummy_fd);
    return 0;
}