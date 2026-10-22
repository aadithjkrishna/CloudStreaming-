#pragma once

#include <string>
#include <cstring>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <cctype>
#include <algorithm>
#include <limits>
#include <optional>
#include <utility>
#include <cstdint>

#include <iostream>

#define BUFFER_SIZE 4096

#define DEBUG_HTTP

enum HTTPMethod
{
    INVALID,
    GET,
    POST,
    PUT,
    PATCH,
    DELETE,
    HEAD,
    OPTIONS,
    CONNECT,
    TRACE,
};

class HTTP
{
public:
    struct HTTPRequest
    {
        HTTPMethod method = HTTPMethod::INVALID;
        std::string path;
        std::string body;
        std::string version;
        size_t content_length = 0;
        std::unordered_map<std::string, std::string> headers;
        std::unordered_map<std::string, std::string> params;
        std::unordered_map<std::string, std::string> query;

        bool valid = false;
    };
    struct HTTPResponse
    {
        std::string statusCode;
        std::string contentType;
        std::string body;

        bool partialContent = false;
        uint64_t rangeStart = 0;
        uint64_t rangeEnd = 0;
        uint64_t totalSize = 0;
    };
    static HTTPRequest receiveData(int socket_fd);
    static void sendData(HTTPResponse, int socket_fd);
    static std::string handleRequest(const HTTPRequest &req);
    static bool readFile(const std::string &filepath, std::string &outContent);
    static bool readFile(const std::string &filepath, std::string &outContent, uint64_t start, uint64_t end);
    static std::string getMimeType(const std::string &path);
    static std::string convertPathToURL(const std::string &file_name);
    static std::string convertURLToPath(const std::string &file_name);

private:
    static std::string handleGETRequests(const HTTPRequest &req);
    static std::string handlePOSTRequest(const HTTPRequest &req);
    static inline std::string buildResponse(const HTTPResponse &response);
    static HTTPRequest parseRequest(const std::string &raw);
};