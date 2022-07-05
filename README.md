# OS_TCP_networking
Operating systems

In this mini-project I implemented a simple Client/Server Architecture: a printable characters counting (PCC) server.
The implementation follows the TCP Networking Protocol (socket, connect, bind, listen, accept) and concepts such as Big/Little Endian (Endianness).

Clients connect to the server and send it a stream of bytes.
The server counts how many of the bytes are printable and returns that number to the client.
The server also maintains overall statistics on the number of printable characters it has received from all clients.
When the server terminates, it prints these statistics to standard output.

Run this to compile:
gcc -O3 -D_DEFAULT_SOURCE -Wall -std=c11 pcc_server.c (or pcc_client.c)

If you wish to run the project, open 2 terminals (one for the client and one for the server):
1. Run the server with desired server port as a command line argument (assumes a 16-bit unsigned integer).
*NOTE: TCP ports below 1024 are reserved and cannot be used by non-root processes.
2. Run the client with the following command line arguments:
    a. Server’s IP address (assumes a valid IP address) - you can use 127.0.0.1 (localhost) if you are running the server and the client on the same machine.
    b. Server’s port (same port from the server's command line argument).
    c. Path of the file you would like to send to the server.
*You can connect several clients to the same server while it is running.
3. In order to terminate the server, hit CTRL-C in the server's terminal. (The statistics will be printed)
