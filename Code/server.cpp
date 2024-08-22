#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <iomanip> // For std::put_time

#define PORT 8080
#define BUFFER_SIZE 1024
#define LOG_FILE "server_log.txt"
#define PASSWORD "pwd123"

// Log message function
void logMessage(const std::string& logFile, const std::string& message) {
    std::ofstream log(logFile, std::ios::app);
    if (!log.is_open()) {
        std::cerr << "Failed to open log file: " << logFile << std::endl;
        return;
    }

    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    log << "[" << std::put_time(localTime, "%Y-%m-%d %H:%M:%S") << "] " << message << std::endl;
    log.close();
}

// Function to list files in the current directory
std::string listFiles() {
    DIR* dir;
    struct dirent* entry;
    std::string fileList;
    
    dir = opendir(".");
    if (dir == nullptr) {
        perror("opendir() error");
        return "Error listing files.";
    }

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) { // Only list regular files
            fileList += entry->d_name;
            fileList += "\n";
        }
    }
    closedir(dir);

    return fileList;
}

// Function to send file to client
void sendFile(int clientSocket, const std::string& filename) {
    char buffer[BUFFER_SIZE] = {0};
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::string error = "File not found.";
        send(clientSocket, error.c_str(), error.size(), 0);
        logMessage(LOG_FILE, "File not found: " + filename);
        return;
    }

    while (file.read(buffer, BUFFER_SIZE)) {
        send(clientSocket, buffer, BUFFER_SIZE, 0);
    }
    send(clientSocket, buffer, file.gcount(), 0); // Send any remaining bytes
    file.close();
    logMessage(LOG_FILE, "File sent: " + filename);
}

// Function to receive file from client
void receiveFile(int clientSocket, const std::string& filename) {
    char buffer[BUFFER_SIZE] = {0};
    std::ofstream file(filename, std::ios::binary);
    int bytesReceived;

    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0) {
        file.write(buffer, bytesReceived);
        if (bytesReceived < BUFFER_SIZE) {
            break; // Done receiving file
        }
    }
    file.close();
    logMessage(LOG_FILE, "File received: " + filename);
}

int main() {
    int serverFd, newSocket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Create socket
    if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to the port
    if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverFd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    logMessage(LOG_FILE, "Server started and listening on port " + std::to_string(PORT));

    while (true) {
        std::cout << "Waiting for a connection..." << std::endl;

        // Accept incoming connection
        if ((newSocket = accept(serverFd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        logMessage(LOG_FILE, "Connection established with client");

        // Authentication
        int valread = read(newSocket, buffer, BUFFER_SIZE);
        std::string clientPassword(buffer, valread);

        if (clientPassword == PASSWORD) {
            std::string successMessage = "Authentication Successful";
            send(newSocket, successMessage.c_str(), successMessage.size(), 0);
            logMessage(LOG_FILE, "Client authenticated successfully");
        } else {
            std::string failMessage = "Authentication Failed";
            send(newSocket, failMessage.c_str(), failMessage.size(), 0);
            logMessage(LOG_FILE, "Client failed authentication");
            close(newSocket);
            continue;
        }

        // Command processing loop
        while (true) {
            memset(buffer, 0, BUFFER_SIZE); // Clear the buffer
            valread = read(newSocket, buffer, BUFFER_SIZE);

            if (valread == 0) {
                logMessage(LOG_FILE, "Client disconnected");
                close(newSocket);
                break;
            }

            std::string command(buffer, valread);
            logMessage(LOG_FILE, "Received command: " + command);

            if (command == "LIST") {
                std::string fileList = listFiles();
                
                // Check if the file list is too large for the buffer
                if (fileList.size() > BUFFER_SIZE) {
                    std::string error = "Error: File list too large to send.";
                    send(newSocket, error.c_str(), error.size(), 0);
                    logMessage(LOG_FILE, "Error: File list too large to send");
                } else {
                    send(newSocket, fileList.c_str(), fileList.size(), 0);
                    logMessage(LOG_FILE, "Sent file list to client");
                }

            } else if (command.find("GET ") == 0) {
                std::string filename = command.substr(4);
                logMessage(LOG_FILE, "Client requested file: " + filename);
                sendFile(newSocket, filename);
            } else if (command.find("PUT ") == 0) {
                std::string filename = command.substr(4);
                logMessage(LOG_FILE, "Client uploading file: " + filename);
                receiveFile(newSocket, filename);
            } else {
                std::string error = "Invalid command.";
                send(newSocket, error.c_str(), error.size(), 0);
                logMessage(LOG_FILE, "Invalid command received");
            }
        }
    }

    return 0;
}
