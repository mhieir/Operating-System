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
#include <cerrno> // Required for errno

using namespace std;

// The path for the Named Pipe as per assignment instructions
const char *FIFO_PATH = "/tmp/os_ca2_fifo";

// Helper function to split strings
vector<string> split(const string &s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        // Trim whitespace
        size_t first = token.find_first_not_of(" \t");
        if (first == string::npos)
            continue; // Skip empty tokens
        size_t last = token.find_last_not_of(" \t");
        tokens.push_back(token.substr(first, (last - first + 1)));
    }
    return tokens;
}

// ---------------------------------------------------------
// Command Implementations
// ---------------------------------------------------------

void cmd_echo(const vector<string> &args)
{
    // Fix for pipeline: If echo has no arguments (e.g., "print_n | echo"), 
    // it must act as a pass-through.
    // User Update: Read only the "top" (first line) of the input.
    // IMPORTANT FIX: We use read() byte-by-byte instead of cin/getline.
    // 'cin' is buffered and might read more data than we want (stealing it from the pipe),
    // causing subsequent commands to find the pipe empty.
    if (args.size() == 1)
    {
        char c;
        while (read(STDIN_FILENO, &c, 1) > 0)
        {
            if (c == '\n') break;
            cout << c;
        }
        cout << endl;
        return;
    }

    // Standard behavior: print arguments
    for (size_t i = 1; i < args.size(); ++i)
    {
        cout << args[i] << (i == args.size() - 1 ? "" : " ");
    }
    cout << endl;
}

void cmd_print_n(const vector<string> &args)
{
    if (args.size() < 2)
        return;
    int n = stoi(args[1]);
    for (int i = 0; i < n; ++i)
    {
        cout << "hello";
        if (i < n - 1)
            cout << ", ";
    }
    cout << endl;
}

void cmd_len()
{
    string line, content;
    // cin will be connected to the read-end of a pipe if piped
    while (getline(cin, line))
    {
        content += line;
    }
    cout << content.length() << endl;
}

// ---------------------------------------------------------
// Execution Logic
// ---------------------------------------------------------

// Parses a single command string (e.g., "echo hello") and executes it
void execute_single_command(string cmd_str)
{
    // Check for redirection to Named Pipe (> np)
    bool out_to_np = false;
    bool in_from_np = false;

    if (cmd_str.find("> np") != string::npos)
    {
        out_to_np = true;
        cmd_str = cmd_str.substr(0, cmd_str.find("> np")); // Remove redirection part
    }
    if (cmd_str.find("< np") != string::npos)
    {
        in_from_np = true;
        cmd_str = cmd_str.substr(0, cmd_str.find("< np")); // Remove redirection part
    }

    // Split command into args
    vector<string> args = split(cmd_str, ' ');
    if (args.empty())
        exit(0);

    // HANDLE REDIRECTION (Named Pipe)
    if (out_to_np)
    {
        // Open normally (blocking). Since 'main' holds a dummy reader, this won't block.
        int fd = open(FIFO_PATH, O_WRONLY); 
        if (fd == -1)
        {
            perror("open fifo write");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    if (in_from_np)
    {
        // Open with O_NONBLOCK to ensure we don't hang if there are no writers yet
        // (e.g. if the previous command finished writing and exited).
        int fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK); 
        if (fd == -1)
        {
            perror("open fifo read");
            exit(1);
        }
        
        // Remove O_NONBLOCK flag so actual reading behaves normally (blocking for data)
        int flags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    // EXECUTE COMMAND
    if (args[0] == "echo")
    {
        cmd_echo(args);
    }
    else if (args[0] == "print_n")
    {
        cmd_print_n(args);
    }
    else if (args[0] == "len")
    {
        cmd_len();
    }
    else
    {
        cerr << "Unknown command: " << args[0] << endl;
    }

    exit(0); // Child must exit after running command
}

// ---------------------------------------------------------
// Main Pipeline Logic
// ---------------------------------------------------------

int main()
{
    // 1. Create the Named Pipe (FIFO) once at startup
    // If it exists, remove it first to be clean, then recreate
    unlink(FIFO_PATH);
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            return 1;
        }
    }

    // NON-BLOCKING FIX: Open a dummy reader to keep the pipe active.
    // This allows writers (> np) to connect immediately without blocking.
    // O_NONBLOCK prevents this open call itself from blocking.
    int dummy_fd = open(FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (dummy_fd == -1) {
        perror("open dummy fifo");
        return 1;
    }

    string input;
    cout << "Welcome. Type 'exit' to quit." << endl;

    while (true)
    {
        cout << "$ ";
        if (!getline(cin, input))
            break;
        if (input == "exit")
            break;

        // Split input by pipe symbol '|'
        vector<string> pipe_parts = split(input, '|');
        int num_cmds = pipe_parts.size();

        if (num_cmds == 0) continue;

        // --- MULTI-PIPE LOGIC ---
        // We need (num_cmds - 1) pipes.
        // We iterate through commands and wire them up.
        
        int prev_pipe_read = -1; // Holds the read end of the *previous* pipe
        vector<pid_t> child_pids;

        for (int i = 0; i < num_cmds; ++i) {
            int pipefd[2];
            
            // Create a new pipe if this is NOT the last command
            if (i < num_cmds - 1) {
                if (pipe(pipefd) == -1) {
                    perror("pipe");
                    break; 
                }
            }

            pid_t pid = fork();
            if (pid == 0) {
                // --- CHILD PROCESS ---
                
                // 1. Input Wiring:
                // If not the first command, read from the previous pipe
                if (prev_pipe_read != -1) {
                    dup2(prev_pipe_read, STDIN_FILENO);
                    close(prev_pipe_read);
                }

                // 2. Output Wiring:
                // If not the last command, write to the current pipe
                if (i < num_cmds - 1) {
                    close(pipefd[0]); // Child doesn't read from this pipe
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                execute_single_command(pipe_parts[i]);
            } else if (pid < 0) {
                perror("fork");
            } else {
                // --- PARENT PROCESS ---
                child_pids.push_back(pid);

                // Cleanup used file descriptors in parent
                
                // 1. Close the read end of the previous pipe (we are done with it)
                if (prev_pipe_read != -1) {
                    close(prev_pipe_read);
                }

                // 2. Setup for next iteration:
                // If not last command, save the read end of the new pipe
                // and close the write end (parent doesn't write)
                if (i < num_cmds - 1) {
                    prev_pipe_read = pipefd[0];
                    close(pipefd[1]);
                }
            }
        }

        // Wait for ALL children to finish
        for (pid_t pid : child_pids) {
            waitpid(pid, NULL, 0);
        }
    }

    // Cleanup
    if (dummy_fd != -1) close(dummy_fd);
    unlink(FIFO_PATH);
    return 0;
}