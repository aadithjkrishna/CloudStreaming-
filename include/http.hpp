#pragma once

#include <string>
#include <cstring>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>

#include <iostream>

#define BUFFER_SIZE 4096

enum HTTPMethod
{
    GET,
    POST,
    PUT,
    PATCH,
    DELETE,
    HEAD,
    OPTIONS,
    CONNECT,
    TRACE,
    UNKNOWN
};

class HTTP
{
public:
    struct HTTPRequest
    {
        HTTPMethod method = UNKNOWN;
        std::string path;
        std::string body;
        std::string version;
        size_t content_length = 0;
        std::unordered_map<std::string, std::string> headers;
        bool valid = false;
    };
    struct HTTPResponse {
        std::string statusCode;
        std::string contentType;
        std::string body;
    };
    static HTTPRequest receiveData(int socket_fd);
    static void sendData(HTTPResponse, int socket_fd);
    static std::string handleRequest(const HTTPRequest &req);
    static bool readFile(const std::string &filepath, std::string &outContent);
    static std::string getMimeType(const std::string &path);

private:
    static std::string handleGETRequests(const HTTPRequest &req);
    static std::string handlePOSTRequest(const HTTPRequest &req);
    static inline std::string buildResponse(const HTTPResponse& response);
    static HTTPRequest parseRequest(const std::string &raw);
};