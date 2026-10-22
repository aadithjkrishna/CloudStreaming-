#include "http.hpp"

namespace
{
    std::string toLower(std::string str)
    {
        std::transform(
            str.begin(),
            str.end(),
            str.begin(),
            [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

        return str;
    }
}

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

    HTTPRequest req = parseRequest(fullData);

#ifdef DEBUG_HTTP
    std::cout << fullData << '\n';
#endif

    return req;
}
void HTTP::sendData(HTTPResponse response, int socket_fd)
{
    std::string httpResponse = buildResponse(response);
#ifdef DEBUG_HTTP
    // std::cout << httpResponse << '\n';
#endif
    send(socket_fd, httpResponse.c_str(), httpResponse.length(), 0);
}

std::string HTTP::buildResponse(const HTTPResponse &response)
{
    std::string result;

    const bool partial =
        response.partialContent &&
        response.statusCode == "206 Partial Content";

    result = "HTTP/1.1 " + response.statusCode + "\r\n";

    result += "Content-Type: " + response.contentType + "\r\n";
    result += "Content-Length: " +
              std::to_string(response.body.size()) + "\r\n";

    if (response.contentType.rfind("audio/", 0) == 0 ||
        response.contentType.rfind("video/", 0) == 0)
    {
        result += "Accept-Ranges: bytes\r\n";
    }

    if (partial)
    {
        result += "Content-Range: bytes " +
                  std::to_string(response.rangeStart) + "-" +
                  std::to_string(response.rangeEnd) + "/" +
                  std::to_string(response.totalSize) + "\r\n";
    }

    if (response.statusCode == "416 Range Not Satisfiable")
{
    result += "Content-Range: bytes */" +
              std::to_string(response.totalSize) + "\r\n";
}

    result += "Connection: close\r\n";
    result += "\r\n";
    result += response.body;

    return result;
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

bool HTTP::readFile(
    const std::string& filepath,
    std::string& outContent,
    uint64_t start,
    uint64_t end)
{
    if (start < 0 || end < start)
        return false;

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
        return false;

    // Determine the file size.
    file.seekg(0, std::ios::end);
    std::streamoff fileSize = file.tellg();

    if (fileSize < 0 ||
        static_cast<uint64_t>(start) >=
            static_cast<uint64_t>(fileSize) ||
        static_cast<uint64_t>(end) >=
            static_cast<uint64_t>(fileSize))
    {
        return false;
    }

    // Inclusive range: start=100, end=199 reads 100 bytes.
    const uint64_t length =
        static_cast<uint64_t>(end) -
        static_cast<uint64_t>(start) + 1;

    outContent.resize(static_cast<size_t>(length));

    file.seekg(static_cast<std::streamoff>(start), std::ios::beg);

    if (!file)
        return false;

    file.read(
        outContent.data(),
        static_cast<std::streamsize>(length));

    if (static_cast<uint64_t>(file.gcount()) != length)
    {
        outContent.clear();
        return false;
    }

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

        // Normalize header names to lowercase.
        std::string key = toLower(line.substr(0, colon));
        std::string value = line.substr(colon + 1);

        // Remove leading and trailing whitespace from the value.
        size_t start = value.find_first_not_of(" \t");
        if (start == std::string::npos)
            value.clear();
        else
            value.erase(0, start);

        size_t end = value.find_last_not_of(" \t");
        if (end != std::string::npos)
            value.erase(end + 1);

        if (key == "content-length")
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

std::string HTTP::convertPathToURL(const std::string &file_name)
{
    static constexpr char hex[] = "0123456789ABCDEF";

    std::string temp;
    temp.reserve(file_name.size() * 3);

    for (unsigned char c : file_name)
    {
        // RFC 3986 unreserved characters: A-Z a-z 0-9 - . _ ~
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '.' || c == '_' || c == '~')
        {
            temp += static_cast<char>(c);
        }
        else if (c == '/')
        {
            // Preserve path separators.
            temp += '/';
        }
        else
        {
            // Percent-encode this byte, including UTF-8 bytes.
            temp += '%';
            temp += hex[c >> 4];
            temp += hex[c & 0x0F];
        }
    }

    return temp;
}

std::string HTTP::convertURLToPath(const std::string &file_name)
{
    auto hexValue = [](unsigned char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };

    std::string temp;
    temp.reserve(file_name.size());

    for (size_t i = 0; i < file_name.size(); ++i)
    {
        if (file_name[i] == '%')
        {
            // Reject malformed or incomplete percent escapes.
            if (i + 2 >= file_name.size())
                return {};

            int high = hexValue(
                static_cast<unsigned char>(file_name[i + 1]));
            int low = hexValue(
                static_cast<unsigned char>(file_name[i + 2]));

            if (high < 0 || low < 0)
                return {};

            temp += static_cast<char>((high << 4) | low);
            i += 2;
        }
        else
        {
            temp += file_name[i];
        }
    }

    return temp;
}