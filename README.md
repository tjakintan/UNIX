UNIX
This repository is a C programming project that implements UNIX-like concepts such as storage management, caching, and networking. It contains modular components for simulating disk operations (JBOD), caching strategies, network communication, and testing.
📂 Project Structure
File / Module	Description
jbod.h, jbod_server	- Just a Bunch Of Disks (JBOD) server implementation
cache.c, cache.h	- Caching layer for temporary storage
net.c, net.h	- Networking logic (e.g., sockets, request handling)
util.c, util.h	- Utility functions shared across modules
mdadm.c, mdadm.h	- Disk management functionality (inspired by Linux mdadm)
tester.c, tester.h	- Testing harness for verifying module behavior
Makefile Build automation script
⚙️ Build Instructions
Clone and build with make:
git clone https://github.com/tjakintan/UNIX.git
cd UNIX
make
This will compile the source files and produce the necessary binaries (e.g., jbod_server_binary).
🚀 Usage
Start the JBOD server.
Run the tester program to verify server behavior.
Experiment with caching and networking by modifying or extending modules.
🎯 Summary
This project demonstrates core UNIX principles implemented in C:
Storage simulation via JBOD
Caching for efficient data access
Networking for client–server interaction
Testing framework to validate functionality
It’s a useful learning project for understanding how operating system components like storage managers and caching systems work under the hood.
🤝 Contributing
Fork the repo
Create a feature/bugfix branch
Ensure it compiles and passes tests
Open a pull request

