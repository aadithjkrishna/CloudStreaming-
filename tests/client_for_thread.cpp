#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <string>

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(3000);

    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid Address\n";
        close(clientSocket);
        return 1;
    }

    std::cout << "Connecting to server on port 3000...\n";
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "Connection failed!\n";
        close(clientSocket);
        return 1;
    }

    std::cout << "Connected successfully\n";

    std::string request = "Ping from client!";
    send(clientSocket, request.c_str(), request.length(), 0);
    std::cout << "Send: " << request << "\n";

    char buffer[2048] = {0};
    ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived > 0) {
        std::cout << "Receieved from server: " << buffer << '\n';
    } else if (bytesReceived == 0) {
        std::cout << "Server closed the connection.\n";
    } else {
        std::cerr << "Receive failed.\n";
    }
    close(clientSocket);
    return 0;
}