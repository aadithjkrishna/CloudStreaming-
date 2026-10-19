#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <unordered_map>
#include <thread>

// #include <nlohmann/json.hpp>

// // for convenience
// using json = nlohmann::json;

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

// std::vector<json> users;

struct Request
{
    HTTPMethod method = UNKNOWN;
    std::string path;
    std::string body;
    std::string version;
    size_t content_length = 0;
    std::unordered_map<std::string, std::string> headers;
    bool valid = false;
};

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

Request parseRequest(const std::string &raw)
{
    // Needs Improvement
    Request req;
    std::istringstream stream(raw);
    std::string methodStr;

    if (!(stream >> methodStr >> req.path >> req.version))
    {
        return req;
    }

    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
    {
        return req;
    }
    std::string headerSection = raw.substr(0, headerEnd);

    std::istringstream headers(headerSection);
    std::string line;
    std::getline(headers, line);

    while (std::getline(headers, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        if (!value.empty() && value[0] == ' ')
            value.erase(0, 1);
        if (key == "Content-Length")
        {
            req.content_length = std::stoi(value);
        }
        req.headers[key] = value;
    }

    req.body = raw.substr(headerEnd + 4);

    static const std::unordered_map<std::string, HTTPMethod> methodMap = {
        {"GET", HTTPMethod::GET},
        {"POST", HTTPMethod::POST},
        {"PUT", HTTPMethod::PUT},
        {"PATCH", HTTPMethod::PATCH},
        {"DELETE", HTTPMethod::DELETE},
        {"HEAD", HTTPMethod::HEAD},
        {"OPTIONS", HTTPMethod::OPTIONS},
        {"CONNECT", HTTPMethod::CONNECT},
        {"TRACE", HTTPMethod::TRACE}};

    auto it = methodMap.find(methodStr);
    if (it != methodMap.end())
    {
        req.method = it->second;
        req.valid = true;
    }

    return req;
}

std::string getMimeType(const std::string &path)
{
    size_t dotPos = path.rfind('.');
    if (dotPos == std::string::npos)
    {
        return "text/plain";
    }

    std::string ext = path.substr(dotPos);
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".js", "application/javascript"},
        {".css", "text/css"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".ico", "image/x-icon"},
        {".svg", "image/svg+xml"}};

    auto it = mimeTypes.find(ext);
    return (it != mimeTypes.end()) ? it->second : "application/octet-stream";
}

bool readFile(const std::string &filepath, std::string &outContent)
{
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    outContent = ss.str();
    return true;
}

inline std::string buildResponse(const std::string &status, const std::string &contentType, const std::string &body)
{
    return "HTTP/1.1 " + status + "\r\n"
                                  "Content-Type: " +
           contentType + "; charset=UTF-8\r\n"
                         "Content-Length: " +
           std::to_string(body.length()) + "\r\n"
                                           "Connection: close\r\n\r\n" +
           body;
}

std::string handleGETRequests(const Request &req)
{
    std::string requestedPath;
    std::string filePath = "./public";
    std::string fileContent;

    if (req.path == "/" || req.path == "/home")
    {
        requestedPath = "/index.html";
    }
    else if (req.path == "/gallery")
    {
        requestedPath = "/gallery.html";
    }
    else if (req.path == "/users")
    {
        // Send data as json
        return buildResponse("200 OK", "application/json", "[\"Aadith\",\"Athul\",\"Rahul\", \"Clara\"]");
    }
    else
    {
        requestedPath = req.path;
    }

    filePath += requestedPath;

    if (readFile(filePath, fileContent))
    {
        std::string mime = getMimeType(requestedPath);
        return buildResponse("200 OK", mime, fileContent);
    }
    else
    {
        return buildResponse("404 Not Found", "text/html",
                             "<h1>404 Not Found</h1><p>File does not exist.</p>");
    }
}

std::string handlePOSTRequest(const Request &req)
{
    if (req.path == "/login")
    {
        std::string responsePayload = "{\"status\" : \"success\",\"message\" : \"User added\", \"user\": " + req.body + "}";
        std::cout << "POST body: " << req.body << '\n';

        // users.push_back(json::parse(req.body));

        return buildResponse("201 Created", "application/json", responsePayload);
    }
    return buildResponse(
        "404 Not Found",
        "application/json",
        "{\"error\":\"Route not found\"}");
}

std::string handleRequest(const Request &req)
{
    if (!req.valid)
    {
        return buildResponse("400 Bad Request", "text/plain", "Bad Request");
    }
    if (req.path.find("..") != std::string::npos)
    {
        return buildResponse("403 Forbidden", "text/plain", "403 Forbidden");
    }

    if (req.method == GET)
    {
        return handleGETRequests(req);
    }
    else if (req.method == POST)
    {
        return handlePOSTRequest(req);
    }
    return buildResponse("405 Method Not Allowed", "text/plain", "Method Not Allowed");
}

std::string receiveData(int socket_fd)
{
    // I do receive until i have the \r\n\r\n and if it has body then go until content-length
    std::string fullData;
    char buffer[BUFFER_SIZE];
    while (fullData.find("\r\n\r\n") == std::string::npos)
    {
        ssize_t bytesReceived =
            recv(socket_fd, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0)
            return {};

        fullData.append(buffer, bytesReceived);
    }
    size_t contentLengthPos =
        fullData.find("Content-Length:");

    size_t contentLength = 0;

    if (contentLengthPos != std::string::npos)
    {
        size_t valueStart =
            contentLengthPos + strlen("Content-Length:");

        while (valueStart < fullData.size() &&
               fullData[valueStart] == ' ')
        {
            valueStart++;
        }

        size_t valueEnd =
            fullData.find("\r\n", valueStart);

        contentLength = std::stoul(
            fullData.substr(
                valueStart,
                valueEnd - valueStart));
    }
    size_t headerEnd =
        fullData.find("\r\n\r\n");

    size_t bodyStart = headerEnd + 4;

    size_t bodyReceived =
        fullData.size() - bodyStart;

    while (bodyReceived < contentLength)
    {
        ssize_t bytesReceived =
            recv(socket_fd, buffer, sizeof(buffer), 0);

        if (bytesReceived <= 0)
            return {};

        fullData.append(buffer, bytesReceived);

        bodyReceived += bytesReceived;
    }

    return fullData;
}

// Files to be send and their url
// All files in public/scripts with their name like home.js etc..
// All files in public/styles with their name like home.css etc..
// Files in src/home.html, src/about.html with /home /about etc..

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

    int count = 0;

    while (true)
    {
        sockaddr_in clientAddress{};
        socklen_t clientLen = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientLen);
        if (clientSocket < 0)
        {
            continue;
        }

        std::thread worker([clientSocket, &count]()
                           {
            char buffer[BUFFER_SIZE] = {0};

            std::string raw = receiveData(clientSocket);
            if (raw.empty())
                {
                    close(clientSocket);
                    return;
                }
                Request req = parseRequest(raw);
                
                std::string httpResponse = handleRequest(req);
                // std::cout << "Client " << ++count << " : \n" << buffer << '\n';
                // std::cout << "Server : \n" << httpResponse << '\n';

                send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
            // }

            close(clientSocket); });
        worker.detach();
    }

    close(serverSocket);
    return 0;
}