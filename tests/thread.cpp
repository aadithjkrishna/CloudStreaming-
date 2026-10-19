#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <iostream>
#include <thread>

int serverSocket = -1;

void handleSignal(int signal)
{
    if (serverSocket != -1)
    {
        std::cout << "\nShutting down server...\n";
        close(serverSocket);
    }
    _exit(0);
}

void handleClient(int clientSocket)
{
    char buffer[2048] = {0};
    ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesReceived > 0)
    {
        std::cout << "Client sent: " << buffer << '\n';

        std::string response = "Hello from server";

        send(clientSocket, response.c_str(), response.length(), 0);
    }
    close(clientSocket);
}

int main()
{
    std::signal(SIGINT, handleSignal);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0)
    {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(3000);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        std::cerr << "Bind failed\n";
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 10) < 0)
    {
        std::cerr << "Listen failed\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "Server running on http://localhost:3000 (Serving from ./public/)\n";

    while (true)
    {
        sockaddr_in clientAddress{};
        socklen_t clientLen = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientLen);
        if (clientSocket < 0)
            break;

        std::thread c1(handleClient, clientSocket);
        c1.detach();
    }
    if (serverSocket != -1)
        close(serverSocket);
    return 0;
}