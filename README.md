# Operating Systems — Course Projects

Three assignments completed for the **Operating System** course at the **University of Tehran**, using **C, C++, and Bash**.

The projects cover socket programming, process creation and inter-process communication, POSIX threads, synchronization, and CPU-scheduling simulation. Each assignment is independent and has its own source files and build workflow.

## Projects

| Assignment | Project | Main concepts |
| --- | --- | --- |
| [CA1](CA1/) | Airline reservation client–server application | TCP/UDP sockets, I/O multiplexing with `select`, command handling, reservations, and timeout handling |
| [CA2](CA2/) | Command shell and pipe benchmarks | `fork`, command execution, file-descriptor redirection, unnamed pipes, named FIFOs, and process waiting |
| [CA3](CA3/) | Threading, synchronization, and scheduling experiments | POSIX threads, parallel array summation, race conditions, mutex protection, FCFS, SJF, and Round Robin |

## CA1: Airline reservation system

A C++ client–server application with airline and customer roles. The command protocol supports registration, login, adding flights, listing flights, reserving seats, confirming reservations, and cancellation. Reservation expiration is handled using timer-related logic.

The implementation uses TCP and UDP sockets, with `select` to multiplex input. The server and client use port **8081**.

**Source:** [`server.cpp`](CA1/src/server.cpp), [`client.cpp`](CA1/src/client.cpp), [`common.hpp`](CA1/include/common.hpp).

### Build and run

From the repository root:

```bash
cd CA1
make clean
make
./server
```

In a second terminal, enter the same directory and start a client:

```bash
./client 127.0.0.1
```

The client accepts the server host as its command-line argument. Open additional terminals for additional clients. Use the command handlers and constants in the source as the reference for command syntax and argument order.

## CA2: Shell and inter-process communication

A C++ command shell that launches processes and connects commands through pipelines. It also supports communication through a named FIFO at `/tmp/os_ca2_fifo`, using the special redirection forms `> np` and `< np`.

The assignment includes Bash scripts that compare named and unnamed pipe operations and collect timing measurements.

**Source:** [`main.cpp`](CA2/main.cpp) · [report](CA2/report.pdf).

### Build and run

From the repository root:

```bash
cd CA2
make clean
make
./main
```

At the shell prompt, try a simple pipeline:

```text
ls | wc -l
```

Enter `exit` to leave the shell. It is a coursework shell with its own parser, so it should not be treated as a full Bash replacement.

### Pipe benchmarks

On Linux, with GNU `/usr/bin/time` available:

```bash
bash benchmark.sh named.sh results_named.txt
bash benchmark.sh unnamed.sh results_unnamed.txt
```

Each benchmark repeats the selected workload and writes timing statistics to the specified file. The named-pipe workload uses `/tmp/f`; avoid running multiple copies simultaneously. The scripts use Linux/GNU command options and are not directly portable to the default macOS utilities.

## CA3: Threads and scheduling

| File | Experiment |
| --- | --- |
| [`task_1a.c`](CA3/task_1a.c) | Create threads, pass per-thread data, and wait for completion with `pthread_join` |
| [`task_1b.c`](CA3/task_1b.c) | Compare sequential and multithreaded array summation |
| [`task_1c1.c`](CA3/task_1c1.c) | Increment a shared counter without synchronization to demonstrate a race condition |
| [`task_1c2.c`](CA3/task_1c2.c) | Protect shared-counter updates with a POSIX mutex |
| [`task_2.c`](CA3/task_2.c) | Simulate FCFS, SJF, and Round Robin on predefined process scenarios and print scheduling metrics |

See the [CA3 report](CA3/report.pdf) for explanations and recorded outputs.

### Compile and run

From the repository root:

```bash
cd CA3
gcc -Wall -Wextra -pthread task_1a.c -o task_1a
gcc -Wall -Wextra -pthread task_1b.c -o task_1b
gcc -Wall -Wextra -pthread task_1c1.c -o task_1c1
gcc -Wall -Wextra -pthread task_1c2.c -o task_1c2
gcc -Wall -Wextra task_2.c -o task_2
```

Run the experiments individually:

```bash
./task_1a
./task_1b
./task_1c1
./task_1c2
./task_2
```

Follow any interactive prompts. The unsynchronized counter is intentionally unsafe: its output can vary between runs and may occasionally match the expected count. Scheduling results are generated from the scenarios defined in `task_2.c`.

## Requirements

- A POSIX environment; Linux is recommended for the complete set of assignments and benchmark scripts.
- GCC and G++ with support for **C++20** for CA1 and **C++17** for CA2.
- GNU Make and POSIX threads.
- Bash, standard command-line utilities, and GNU `/usr/bin/time` for the CA2 benchmarks.

Clone the repository with:

```bash
git clone https://github.com/mhieir/Operating-System.git
cd Operating-System
```

Some directories contain prebuilt executables and object files. Rebuild from source for your own platform instead of relying on those artifacts. `make clean` is particularly useful when switching between platforms.

## Author

[Mahdis Mirzaei](https://github.com/mhieir) · Computer Engineering, University of Tehran
