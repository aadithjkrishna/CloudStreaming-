#include "server.hpp"

std::atomic<bool> Server::shutdownInitiated{false};

Server::Server(int port) : port(port), fs("./audios", "./data/music_library.json", std::chrono::minutes(2))
{
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
}

Server::~Server()
{
    if (socket_fd != -1)
        close(socket_fd);
}

int Server::start()
{
    if (bind(socket_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        std::cerr << "Bind failed\n";
        close(socket_fd);
        return 1;
    }

    if (listen(socket_fd, 10) < 0)
    {
        std::cerr << "Listen failed\n";
        close(socket_fd);
        return 1;
    }

    try
    {
        fs.start();
    }
    catch (const std::exception &e)
    {
        std::cerr << "FileService startup failed: "
                  << e.what() << '\n';
        return 1;
    }

    return 0;
}
int Server::run()
{
    struct pollfd serverPoll{};
    serverPoll.fd = socket_fd;
    serverPoll.events = POLLIN;

    while (!shutdownInitiated)
    {
        int result = poll(&serverPoll, 1, 100);

        if (result < 0)
        {
            if (errno == EINTR)
                continue;

            std::cerr << "poll failed\n";
            break;
        }

        if (result == 0)
        {
            // 100 ms timeout.
            // Check shutdownInitiated again.
            continue;
        }

        if (serverPoll.revents & POLLIN)
        {
            sockaddr_in clientAddress{};
            socklen_t clientLen = sizeof(clientAddress);

            int clientSocket = accept(
                socket_fd,
                (struct sockaddr *)&clientAddress,
                &clientLen);

            if (clientSocket < 0)
            {
                if (errno == EINTR)
                    continue;

                std::cerr << "accept failed\n";
                continue;
            }

            std::thread worker(
                &Server::worker_thread,
                this,
                clientSocket);

            worker.detach();
        }
    }
    std::cout << "Server shutting down...\n";
    fs.stop();
    return 0;

    return 0;
}

HTTP::HTTPResponse Server::handleRequest(const HTTP::HTTPRequest &req)
{
    if (!req.valid)
    {
        return {"400 Bad Request", "text/plain", "Bad Request"};
    }
    if (req.path.find("..") != std::string::npos)
    {
        return {"403 Forbidden", "text/plain", "403 Forbidden"};
    }

    return router.route(req);
}
void Server::worker_thread(int clientSocket)
{
    HTTP::HTTPRequest req = HTTP::receiveData(clientSocket);

    HTTP::HTTPResponse httpResponse = handleRequest(req);

    HTTP::sendData(httpResponse, clientSocket);

    close(clientSocket);
}
void Server::handleSignal(int)
{
    shutdownInitiated = true;
}