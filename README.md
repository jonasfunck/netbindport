![NetBindPort logo](Logo/NetBindPort_logo.png)

This C++ program is a simple network server implemented using socket programming. It allows users to specify one or more network ports to bind and listen to for incoming connections.

nbp was created out of the necessity to be able to easily bind ports for network/firewall/routing testing purposes.

For current version, refer to the [VERSION](VERSION) file.

## Compilation

To compile nbp you need to install a C++ compiler, for example GNU Compiler Collection (`g++`) or Clang (`clang++`).

### Compile

Build with GCC and local libraries:
```sh
g++ -std=c++17 -pthread -O2 -o nbp nbp.cpp
```

Build with Clang and local libraries:
```sh
clang++ -std=c++17 -pthread -O2 -o nbp nbp.cpp
```

Build with GCC using static linking for portability:
```sh
g++ -std=c++17 -pthread -O2 -static -o nbp nbp.cpp
```

> Note: `-pthread` is recommended when building with `std::thread` and ensures proper thread support.

## Usage

`./nbp -p <port1> [<port2> ...] [-f logfile.txt]`

### Command-line Options
- `-p <port1> [<port2> ...]`: Specify one or more ports to listen on, this option is mandatory.
- `-f logfile.txt`: Enable logging to the specified file.
- `-h`: Display help message showing program usage.
- `-v`, `--version`: Show the current program version.

### Notes

- The current release is stored in the `VERSION` file.
- Use `./nbp -v` or `./nbp --version` to print the version and exit.

### Usage Example
To run the program listening on ports 8080 and 9090 with logging enabled:

`./nbp -p 8080 9090 -f /path/to/nbp.log`

To run the program listening on port 8181 without file logging enabled:

`./nbp -p 8181`

To run the program listening on ports 4000 to 4020 (using bash expansion) without file logging enabled:

`./nbp -p {4000..4020}`

## Program Functionality

### Workflow
1. **Argument Parsing**: Parses command-line arguments to retrieve the ports and log file (if specified).
2. **Socket Creation**: Creates sockets for each specified port and listens for incoming connections.
3. **Handling Connections**: Accepts incoming connections, logs connection details (source IP, destination port, date, and time).
4. **Connection Handling**: Upon connection, sends a success message to the client, including the connection timestamp.
5. **Continuous Operation**: Continuously listens for incoming connections until manually terminated.

### Additional Information
- Retrieves the hostname to display as part of the success message.
- Logs a timestamp with the date and time for each incoming connection.
- The program can run continuously, listening for incoming connections.

## Contribution
Feel free to contribute, report issues, or suggest improvements by opening an issue or pull request.

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
