#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <iomanip> // For std::put_time

#define PORT 8080
#define BUFFER_SIZE 1024
#define LOG_FILE "client_log.txt"

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

// Function to receive a file from the server
void receiveFile(int sock, const std::string& filename) {
    char buffer[BUFFER_SIZE] = {0};
    std::ofstream file(filename, std::ios::binary);
    int bytesRead;

    while ((bytesRead = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        file.write(buffer, bytesRead);
        if (bytesRead < BUFFER_SIZE) {
            break; // End of file transmission
        }
    }

    file.close();
    logMessage(LOG_FILE, "File received: " + filename);
}

// Function to send a file to the server
void sendFile(int sock, const std::string& filename) {
    char buffer[BUFFER_SIZE] = {0};
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "File " << filename << " could not be opened" << std::endl;
        return;
    }

    while (file.read(buffer, BUFFER_SIZE)) {
        send(sock, buffer, BUFFER_SIZE, 0);
    }

    // Send any remaining bytes
    send(sock, buffer, file.gcount(), 0);
    file.close();
    logMessage(LOG_FILE, "File sent: " + filename);
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported" << std::endl;
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return -1;
    }

    // Authentication
    std::string password;
    std::cout << "Enter password: ";
    std::cin >> password;
    send(sock, password.c_str(), password.size(), 0);

    // Receive authentication result
    recv(sock, buffer, BUFFER_SIZE, 0);
    if (std::string(buffer).find("Authentication Failed") != std::string::npos) {
        std::cerr << "Authentication Failed" << std::endl;
        close(sock);
        return -1;
    }

    logMessage(LOG_FILE, "Connected to server");

    std::cin.ignore(); // Clear the newline left in the input buffer

    while (true) {
        std::string command;
        std::cout << "Enter command (LIST, GET <filename>, PUT <filename>, EXIT): ";
        std::getline(std::cin, command);

        // Send command to server
        send(sock, command.c_str(), command.size(), 0);

        // Process the command
        if (command == "LIST") {
            // Receive the file list from the server
            memset(buffer, 0, BUFFER_SIZE); // Clear buffer
            recv(sock, buffer, BUFFER_SIZE, 0);
            std::cout << "Files:\n" << buffer << std::endl;
            logMessage(LOG_FILE, "Listed files on server");
        } 
        else if (command.find("GET ") == 0) {
            // Receive the specified file from the server
            std::string filename = command.substr(4);
            logMessage(LOG_FILE, "Requested file: " + filename);
            receiveFile(sock, filename);
        } 
        else if (command.find("PUT ") == 0) {
            // Send the specified file to the server
            std::string filename = command.substr(4);
            logMessage(LOG_FILE, "Uploading file: " + filename);
            sendFile(sock, filename);
        } 
        else if (command == "EXIT") {
            // Exit the loop and close the connection
            logMessage(LOG_FILE, "Client disconnected.");
            break;
        } 
        else {
            std::cout << "Invalid command." << std::endl;
        }
    }

    close(sock);
    return 0;
}
