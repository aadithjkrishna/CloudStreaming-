#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <atomic>
#include <poll.h>
#include <cerrno>

#include "http.hpp"
#include "router.hpp"

class Server
{
private:
    int socket_fd = -1;
    int port;
    sockaddr_in address{};
    static std::atomic<bool> shutdownInitiated;
    HTTP::HTTPResponse handleRequest(const HTTP::HTTPRequest &req);
    Router router;

public:
    Server(int port = 3000);
    ~Server();
    int start();
    int run();
    void worker_thread(int clientSocket);
    static void handleSignal(int signal);
    void get(const std::string &path, Handler handler)
    {
        router.get(path, std::move(handler));
    }
    void get(const std::string &path, const std::string &filePath) {
        router.get(path, filePath);
    }
    void post(const std::string &path, Handler handler)
    {
        router.post(path, std::move(handler));
    }
    void serveFolder(const std::string &path)
    {
        router.serveFolder(path);
    }
};