#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fstream>
#include <vector>
#include <ctime>
#include <thread>
#include <mutex>
#include <csignal>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

std::mutex logMutex; // Mutex for thread-safe logging
bool shouldTerminate = false; // Global variable to signal program termination

// Function to check if a key has been pressed
bool kbhit() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    // Save old terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Set the new terminal mode
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Set non-blocking mode
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    // Read a character from the input
    ch = getchar();

    // Restore old terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    // Restore non-blocking mode
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    // Check if a character was read
    if(ch != EOF) {
        ungetc(ch, stdin);
        return true;
    }

    return false;
}

void signalHandler(int signal) {
    if (signal == SIGINT) {
        shouldTerminate = true;
    }
}

void logConnection(std::ostream &output, const char *clientIp, int port) {
    std::time_t now = std::time(nullptr);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    // Log connection information (thread-safe)
    {
        std::lock_guard<std::mutex> lock(logMutex);
        output << "[" << timestamp << "] Connection established with: " << clientIp
               << " on port " << port << std::endl;
    }
}

void handleConnection(int clientSocket, const char *clientIp, int port, std::ostream &output) {
    std::time_t now = std::time(nullptr);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    struct sockaddr_in localAddr;
    socklen_t localAddrLen = sizeof(localAddr);
    getsockname(clientSocket, (struct sockaddr *)&localAddr, &localAddrLen);

    char hostIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(localAddr.sin_addr), hostIp, INET_ADDRSTRLEN);

    std::string successMessage = "Connection established with " + std::string(hostIp) +
                                 " on port " + std::to_string(port) + " at " + std::string(timestamp) + "\n";

    send(clientSocket, successMessage.c_str(), successMessage.length(), 0);

    close(clientSocket);
}

void acceptConnections(int serverSocket, int port, std::ostream &output, std::ofstream &logfile) {
    while (!shouldTerminate) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSocket == -1) {
            std::cerr << "Accept failed for port " << port << std::endl;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIp, INET_ADDRSTRLEN);

        // Log connection information to stdout and handle the connection
        logConnection(output, clientIp, port);

        // Log connection information to the file
        if (logfile.is_open()) {
            logConnection(logfile, clientIp, port);
        }

        std::thread(handleConnection, clientSocket, clientIp, port, std::ref(output)).detach();
    }
}

void printHelpMessage(const char *programName) {
    std::cout << "Usage: " << programName << " -p <port1> [<port2> ...] [-f logfile.txt]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -p <port1> [<port2> ...] : Specify port(s) to listen on" << std::endl;
    std::cout << "  -f logfile.txt            : Enable logging to the specified file" << std::endl;
    std::cout << "  -h                        : Display this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "nbp binds to specified port(s) and responds to incoming TCP connections." << std::endl;
}

int main(int argc, char *argv[]) {
    bool loggingEnabled = false;
    std::ofstream logfile;

    if (argc < 3 || strcmp(argv[1], "-p") != 0) {
        printHelpMessage(argv[0]);
        return 1;
    }

    if (argc >= 4 && strcmp(argv[argc - 2], "-f") == 0) {
        loggingEnabled = true;
        logfile.open(argv[argc - 1], std::ofstream::out | std::ofstream::app);
        if (!logfile.is_open()) {
            std::cerr << "Error opening log file" << std::endl;
            return 1;
        }
    }

    // Set up signal handler for SIGINT (Ctrl+C)
    std::signal(SIGINT, signalHandler);

    std::vector<int> serverSockets;
    std::vector<int> serverPorts;

    // Create sockets for specified ports
    for (int i = 1; i < argc - (loggingEnabled ? 2 : 0); ++i) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "-f") == 0) {
            continue;
        }

        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            std::cerr << "Error creating socket for port " << argv[i] << std::endl;
            return 1;
        }

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(atoi(argv[i]));

        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
            std::cerr << "Binding failed for port " << argv[i] << std::endl;
            close(serverSocket);
            return 1;
        }

        if (listen(serverSocket, 10) == -1) {
            std::cerr << "Listen failed for port " << argv[i] << std::endl;
            close(serverSocket);
            return 1;
        }

        std::cout << "nbp Server is listening on port " << argv[i] << std::endl;

        serverSockets.push_back(serverSocket);
        serverPorts.push_back(atoi(argv[i]));

        std::thread(acceptConnections, serverSocket, atoi(argv[i]), std::ref(std::cout), std::ref(logfile)).detach();
    }

    // Keep the main thread running until termination signal is received or 'q' is pressed
    while (!shouldTerminate && !kbhit()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Close the server sockets
    for (size_t i = 0; i < serverSockets.size(); ++i) {
        close(serverSockets[i]);
    }

    // Close the log file if it was opened
    if (logfile.is_open()) {
        logfile.close();
    }

    return 0;
}
