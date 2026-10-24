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
#include <nlohmann/json.hpp>

#include "http.hpp"
#include "router.hpp"
#include "file_service.hpp"
#include "yt_dlp_backend.hpp"
#include "telegram_backend.hpp"


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
    FileService fs;
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
    void get(const std::string &path, const std::string &filePath)
    {
        router.get(path, filePath);
    }
    void post(const std::string &path, Handler handler)
    {
        router.post(path, std::move(handler));
    }
    void download(const std::string audio_id) {
        YT_DLP::downloadAudio(audio_id, "./audios", fs);
    }
    void serveFolder(const std::string &path)
    {
        router.serveFolder(path);
    }
    void serveFolder(
        const std::string &path,
        const std::string &urlPrefix)
    {
        router.serveFolder(path, urlPrefix);
    }
};