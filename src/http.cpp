#include "http.hpp"

HTTP::HTTPRequest HTTP::receiveData(int socket_fd)
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

    // std::cout << "Request: " << fullData << '\n';
    
    HTTPRequest req = parseRequest(fullData);

    return req;
}
void HTTP::sendData(HTTPResponse response, int socket_fd)
{
    std::string httpResponse = buildResponse(response);
    send(socket_fd, httpResponse.c_str(), httpResponse.length(), 0);
}

inline std::string HTTP::buildResponse(const HTTPResponse& response)
{
    return "HTTP/1.1 " + response.statusCode + "\r\n"
                                  "Content-Type: " +
           response.contentType + "; charset=UTF-8\r\n"
                         "Content-Length: " +
           std::to_string(response.body.length()) + "\r\n"
                                           "Connection: close\r\n\r\n" +
           response.body;
}
bool HTTP::readFile(const std::string &filepath, std::string &outContent)
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
std::string HTTP::getMimeType(const std::string &path)
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
HTTP::HTTPRequest HTTP::parseRequest(const std::string &raw)
{
    // Needs Improvement
    HTTPRequest req;
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