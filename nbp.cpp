/**
 * NetBindPort (nbp) - A simple TCP port binding utility
 *
 * This program binds to one or more specified TCP ports and listens for incoming connections.
 * When a connection is established, it logs the connection details and sends a confirmation
 * message back to the client before closing the connection. Useful for testing port availability
 * or simulating services that acknowledge connections.
 *
 * Features:
 * - Bind to multiple ports simultaneously
 * - Thread-safe connection logging to console and/or file
 * - Graceful shutdown via 'q' key or Ctrl+C
 * - Command-line interface with help and version options
 */

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

// Global mutex for thread-safe logging operations
std::mutex logMutex;

// Global flag to signal program termination across threads
bool shouldTerminate = false;

// Program version constant
const char *const VERSION = "0.1.1";

// Terminal configuration variables to restore original settings
static struct termios originalTermios;
static int originalStdinFlags = 0;
static bool terminalConfigured = false;

// Configure the terminal for non-blocking, raw input mode to detect key presses without echoing
void configureTerminal() {
    if (terminalConfigured) {
        return;
    }

    tcgetattr(STDIN_FILENO, &originalTermios);
    struct termios raw = originalTermios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    originalStdinFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, originalStdinFlags | O_NONBLOCK);

    terminalConfigured = true;
}

// Restore the original terminal settings
void restoreTerminal() {
    if (!terminalConfigured) {
        return;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &originalTermios);
    fcntl(STDIN_FILENO, F_SETFL, originalStdinFlags);
    terminalConfigured = false;
}

// Read a single character from stdin without blocking
bool readInputChar(char &ch) {
    int c = getchar();
    if (c == EOF) {
        return false;
    }

    ch = static_cast<char>(c);
    return true;
}

// Check if the quit key ('q' or 'Q') has been pressed
bool isQuitKeyPressed() {
    char ch;
    if (!readInputChar(ch)) {
        return false;
    }

    return ch == 'q' || ch == 'Q';
}

// Signal handler for SIGINT (Ctrl+C) to set the termination flag
void signalHandler(int signal) {
    if (signal == SIGINT) {
        shouldTerminate = true;
    }
}

// Log connection information with timestamp to the specified output stream (thread-safe)
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

// Handle an incoming client connection by sending a success message and closing the socket
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

// Accept incoming connections on the server socket and handle each in a separate thread
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

// Print the help message with usage instructions
void printHelpMessage(const char *programName) {
    std::cout << "Usage: " << programName << " -p <port1> [<port2> ...] [-f logfile.txt] [-v|--version] [-h]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -p <port1> [<port2> ...]  : Specify port(s) to listen on" << std::endl;
    std::cout << "  -f logfile.txt            : Enable logging to the specified file" << std::endl;
    std::cout << "  -v, --version             : Show program version" << std::endl;
    std::cout << "  -h                        : Display this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "nbp binds to specified port(s) and responds to incoming TCP connections." << std::endl;
}

// Parse and validate a port number from a string argument
static bool parsePort(const char *arg, int &port) {
    try {
        int value = std::stoi(arg);
        if (value < 1 || value > 65535) {
            return false;
        }
        port = value;
        return true;
    } catch (...) {
        return false;
    }
}

// Main function: parse command-line arguments, set up servers, and run the event loop
int main(int argc, char *argv[]) {
    bool loggingEnabled = false;
    std::ofstream logfile;
    std::vector<int> serverPorts;

    // Parse command-line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            printHelpMessage(argv[0]);
            return 0;
        }

        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            std::cout << "NetBindPort (nbp)" << std::endl;
            std::cout << "Version: " << VERSION << std::endl;
            return 0;
        }

        if (strcmp(argv[i], "-p") == 0) {
            ++i;
            if (i >= argc) {
                std::cerr << "Error: Missing port after -p" << std::endl;
                printHelpMessage(argv[0]);
                return 1;
            }
            while (i < argc && argv[i][0] != '-') {
                int port = 0;
                if (!parsePort(argv[i], port)) {
                    std::cerr << "Invalid port: " << argv[i] << std::endl;
                    return 1;
                }
                serverPorts.push_back(port);
                ++i;
            }
            --i;
            continue;
        }

        if (strcmp(argv[i], "-f") == 0) {
            ++i;
            if (i >= argc) {
                std::cerr << "Error: Missing logfile after -f" << std::endl;
                printHelpMessage(argv[0]);
                return 1;
            }
            loggingEnabled = true;
            logfile.open(argv[i], std::ofstream::out | std::ofstream::app);
            if (!logfile.is_open()) {
                std::cerr << "Error opening log file" << std::endl;
                return 1;
            }
            continue;
        }

        std::cerr << "Unknown option: " << argv[i] << std::endl;
        printHelpMessage(argv[0]);
        return 1;
    }

    if (serverPorts.empty()) {
        std::cerr << "Error: At least one port must be specified with -p" << std::endl;
        printHelpMessage(argv[0]);
        return 1;
    }

    // Set up signal handler for SIGINT (Ctrl+C)
    std::signal(SIGINT, signalHandler);

    configureTerminal();
    std::cout << "Press 'q' to quit safely, or Ctrl+C to exit." << std::endl;

    std::vector<int> serverSockets;

    // Create sockets for specified ports
    for (int port : serverPorts) {
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            std::cerr << "Error creating socket for port " << port << std::endl;
            return 1;
        }

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
            std::cerr << "Binding failed for port " << port << std::endl;
            close(serverSocket);
            return 1;
        }

        if (listen(serverSocket, 10) == -1) {
            std::cerr << "Listen failed for port " << port << std::endl;
            close(serverSocket);
            return 1;
        }

        std::cout << "nbp Server is listening on port " << port << std::endl;

        serverSockets.push_back(serverSocket);
        std::thread(acceptConnections, serverSocket, port, std::ref(std::cout), std::ref(logfile)).detach();
    }

    // Keep the main thread running until termination signal is received or 'q' is pressed
    while (!shouldTerminate) {
        if (isQuitKeyPressed()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << std::endl;
    restoreTerminal();

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
