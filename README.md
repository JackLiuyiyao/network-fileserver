# 🖧 Multithreaded Network File Server

This project implements a multithreaded network file server that supports concurrent client requests to create, delete, read, and write files and directories over TCP.  
It uses Boost shared mutexes for fine-grained locking, C++ sockets for network communication, and a lightweight in-memory disk simulation.

## 🚀 Features
- Handle multiple clients concurrently using threads.
- Create, delete, read, and write files and directories remotely.
- Fine-grained locking using per-block locks with Boost `shared_mutex`.
- Concurrency control with hand-over-hand (lock-coupling) locking.
- Disk state management with a lightweight in-memory model.
- Robust command parsing and error checking for file system operations.

## 🛠️ Technologies Used
- C++17
- BSD Sockets (TCP networking)
- Boost Threads (`boost::thread`, `boost::shared_mutex`)
- POSIX system calls (`socket()`, `bind()`, `listen()`, `accept()`, `recv()`, `send()`)
- Custom in-memory disk abstraction

## 📂 Key Files
- `fs_server.cpp` — **Main server logic**: handles incoming connections, processes client requests.
- `fs_server.h` — Server function declarations.
- `helpers.h` — Helper functions for disk I/O, parsing, and filesystem operations.
- `fs_param.h` — Constants (e.g., maximum file sizes, pathname limits).
- `Makefile` — Build instructions for compiling the server.

> 📌 **Main execution starts at `main()` inside `fs_server.cpp`.**
> 
> 📌 **Request handling logic is in `handle_connection(int connectionfd)`.**

## 🛤️ Project Structure
