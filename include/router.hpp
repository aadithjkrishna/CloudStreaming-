#pragma once

#include <vector>
#include <functional>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <limits>
#include "http.hpp"

#include <filesystem>

struct StaticFolder
{
    std::string filesystemPath;
    std::string urlPrefix;
};

inline std::optional<std::pair<uint64_t, uint64_t>>
parseRange(const std::string &range, uint64_t fileSize)
{
    if (fileSize == 0 || range.rfind("bytes=", 0) != 0)
        return std::nullopt;

    const std::string value = range.substr(6);
    if (value.find(',') != std::string::npos)
        return std::nullopt;

    const size_t dash = value.find('-');

    if (dash == std::string::npos)
        return std::nullopt;

    const std::string startStr = value.substr(0, dash);
    const std::string endStr = value.substr(dash + 1);

    auto parseNumber = [](const std::string &s,
                          uint64_t &result) -> bool
    {
        if (s.empty())
            return false;

        result = 0;

        for (unsigned char c : s)
        {
            if (c < '0' || c > '9')
                return false;

            const uint64_t digit = c - '0';

            if (result >
                (std::numeric_limits<uint64_t>::max() - digit) / 10)
                return false;

            result = result * 10 + digit;
        }

        return true;
    };

    uint64_t start = 0;
    uint64_t end = 0;

    if (startStr.empty())
    {
        // bytes=-500: last 500 bytes
        uint64_t suffixLength;

        if (!parseNumber(endStr, suffixLength) || suffixLength == 0)
            return std::nullopt;

        start = suffixLength >= fileSize
                    ? 0
                    : fileSize - suffixLength;

        end = fileSize - 1;
    }
    else
    {
        if (!parseNumber(startStr, start) || start >= fileSize)
            return std::nullopt;

        if (endStr.empty())
        {
            // bytes=500: from byte 500 to EOF
            end = fileSize - 1;
        }
        else
        {
            if (!parseNumber(endStr, end) || start > end)
                return std::nullopt;

            if (end >= fileSize)
                end = fileSize - 1;
        }
    }

    return std::make_pair(start, end);
}

using Handler = std::function<HTTP::HTTPResponse(const HTTP::HTTPRequest &)>;
struct Route
{
    HTTPMethod method;
    std::string path;
    Handler handler;
    std::string path_parameter;
    Route(
        HTTPMethod httpMethod,
        const std::string &httpPath,
        Handler httpHandler)
        : method(httpMethod),
          path(httpPath),
          handler(std::move(httpHandler))
    {
    }
    Route(
        HTTPMethod httpMethod,
        const std::string &httpPath,
        Handler httpHandler, const std::string &path_parameter)
        : method(httpMethod),
          path(httpPath),
          handler(std::move(httpHandler)),
          path_parameter(path_parameter)
    {
    }
};

class Router
{
private:
    std::vector<Route> routes;
    std::vector<StaticFolder> staticFolders;

    static HTTP::HTTPResponse defaultHandler(const HTTP::HTTPRequest &)
    {
        return {
            "501 Not Implemented",
            "text/plain",
            "Not Implemented"};
    }

public:
    void get(const std::string &path, Handler handler = defaultHandler)
    {
        routes.push_back({HTTPMethod::GET,
                          path,
                          std::move(handler)});
    }
    void get(const std::string &path, const std::string &filePath)
    {
        routes.push_back({HTTPMethod::GET,
                          path,
                          [filePath](const HTTP::HTTPRequest &)
                          {
                              std::string content;

                              if (!HTTP::readFile(filePath, content))
                              {
                                  return HTTP::HTTPResponse{
                                      "404 Not Found",
                                      "text/plain",
                                      "File Not Found"};
                              }

                              return HTTP::HTTPResponse{
                                  "200 OK",
                                  HTTP::getMimeType(filePath),
                                  content};
                          }});
    }
    void post(const std::string &path, Handler handler = defaultHandler)
    {
        routes.push_back({HTTPMethod::POST, path, std::move(handler)});
    }

    HTTP::HTTPResponse handleFile(
        const std::string &relativePath,
        const std::string &folderPath)
    {
        namespace fs = std::filesystem;

        fs::path root = fs::weakly_canonical(fs::path(folderPath));
        fs::path file = fs::weakly_canonical(root / relativePath);

        auto rootIt = root.begin();
        auto fileIt = file.begin();

        for (; rootIt != root.end(); ++rootIt, ++fileIt)
        {
            if (fileIt == file.end() || *rootIt != *fileIt)
            {
                return {"404 Not Found", "text/plain", "Not Found"};
            }
        }

        std::string content;

        if (!fs::is_regular_file(file) ||
            !HTTP::readFile(file.string(), content))
        {
            return {"404 Not Found", "text/plain", "Not Found"};
        }

        return {
            "200 OK",
            HTTP::getMimeType(file.string()),
            content};
    }

    HTTP::HTTPResponse handleFile(
        const std::string &relativePath,
        const std::string &folderPath,
        const HTTP::HTTPRequest &req)
    {
        namespace fs = std::filesystem;

        fs::path root = fs::weakly_canonical(folderPath);
        fs::path file = fs::weakly_canonical(root / relativePath);

        // Ensure the requested file stays inside the served folder.
        auto rootIt = root.begin();
        auto fileIt = file.begin();

        for (; rootIt != root.end(); ++rootIt, ++fileIt)
        {
            if (fileIt == file.end() || *rootIt != *fileIt)
                return {"404 Not Found", "text/plain", "Not Found"};
        }

        if (!fs::is_regular_file(file))
            return {"404 Not Found", "text/plain", "Not Found"};

        const uint64_t fileSize = fs::file_size(file);
        const std::string mimeType = HTTP::getMimeType(file.string());

        auto rangeIt = req.headers.find("range");

        // No Range header: send the entire file.
        if (rangeIt == req.headers.end())
        {
            std::string content;

            if (!HTTP::readFile(file.string(), content))
                return {"500 Internal Server Error", "text/plain",
                        "Failed to read file"};

            return {"200 OK", mimeType, std::move(content)};
        }

        auto range = parseRange(rangeIt->second, fileSize);

        // Invalid or unsatisfiable range.
        if (!range)
        {
            HTTP::HTTPResponse response{
                "416 Range Not Satisfiable",
                "text/plain",
                "Range Not Satisfiable"};

            response.totalSize = fileSize;
            return response;
        }

        const uint64_t start = range->first;
        const uint64_t end = range->second;

        std::string content;

        if (!HTTP::readFile(file.string(), content, start, end))
        {
            return {"500 Internal Server Error", "text/plain",
                    "Failed to read file range"};
        }

        HTTP::HTTPResponse response{
            "206 Partial Content",
            mimeType,
            std::move(content)};

        response.partialContent = true;
        response.rangeStart = start;
        response.rangeEnd = end;
        response.totalSize = fileSize;

        return response;
    }

    HTTP::HTTPResponse route(const HTTP::HTTPRequest &request)
    {
        HTTP::HTTPRequest matchedRequest = request;
        auto splitPath = [](const std::string &path)
        {
            std::vector<std::string> segments;
            std::stringstream ss(path);
            std::string segment;
            while (std::getline(ss, segment, '/'))
            {
                if (!segment.empty())
                    segments.push_back(segment);
            }
            return segments;
        };
        auto urlDecode = [](const std::string &value)
        {
            std::string result;
            result.reserve(value.size());
            for (size_t i = 0; i < value.size(); ++i)
            {
                if (value[i] == '%' && i + 2 < value.size())
                {
                    auto hexValue = [](char c) -> int
                    {
                        if (c >= '0' && c <= '9')
                            return c - '0';
                        if (c >= 'a' && c <= 'f')
                            return c - 'a' + 10;
                        if (c >= 'A' && c <= 'F')
                            return c - 'A' + 10;
                        return -1;
                    };
                    int high = hexValue(value[i + 1]);
                    int low = hexValue(value[i + 2]);
                    if (high >= 0 && low >= 0)
                    {
                        result += static_cast<char>((high << 4) | low);
                        i += 2;
                        continue;
                    }
                }
                else if (value[i] == '+')
                {
                    result += ' ';
                    continue;
                }
                result += value[i];
            }
            return result;
        };
        const size_t queryPos = request.path.find('?');
        const std::string pathOnly =
            request.path.substr(0, queryPos);
        const std::string queryString =
            queryPos != std::string::npos
                ? request.path.substr(queryPos + 1)
                : "";
        matchedRequest.query.clear();
        std::stringstream queryStream(queryString);
        std::string pair;
        while (std::getline(queryStream, pair, '&'))
        {
            if (pair.empty())
                continue;
            const size_t equalPos = pair.find('=');
            const std::string key =
                urlDecode(pair.substr(0, equalPos));
            const std::string value =
                equalPos != std::string::npos
                    ? urlDecode(pair.substr(equalPos + 1))
                    : "";
            if (!key.empty())
                matchedRequest.query[key] = value;
        }
        const auto requestSegments = splitPath(pathOnly);
        for (const auto &route : routes)
        {
            if (route.method != request.method)
                continue;
            const auto routeSegments = splitPath(route.path);
            if (routeSegments.size() != requestSegments.size())
                continue;
            bool matches = true;
            matchedRequest.params = request.params;
            for (size_t i = 0; i < routeSegments.size(); ++i)
            {
                const auto &pattern = routeSegments[i];
                const auto &value = requestSegments[i];

                if (pattern.size() >= 3 &&
                    pattern.front() == '{' &&
                    pattern.back() == '}')
                {
                    const std::string name =
                        pattern.substr(1, pattern.size() - 2);

                    matchedRequest.params[name] = urlDecode(value);
                }
                else if (pattern != value)
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return route.handler(matchedRequest);
        }
        if (request.method == HTTPMethod::GET)
        {
            for (const auto &folder : staticFolders)
            {
                const std::string &prefix = folder.urlPrefix;
                bool matches =
                    prefix == "/"
                        ? (!pathOnly.empty() && pathOnly.front() == '/')
                        : (pathOnly == prefix ||
                           pathOnly.rfind(prefix + "/", 0) == 0);
                if (!matches)
                    continue;
                std::string relativePath;
                if (prefix == "/")
                {
                    relativePath = pathOnly.substr(1);
                }
                else
                {
                    relativePath = pathOnly.substr(prefix.size());

                    if (!relativePath.empty() &&
                        relativePath.front() == '/')
                    {
                        relativePath.erase(0, 1);
                    }
                }
                relativePath = HTTP::convertURLToPath(relativePath);
                if (relativePath.empty())
                    continue;
                HTTP::HTTPResponse response =
                    handleFile(relativePath, folder.filesystemPath, request);
                if (response.statusCode != "404 Not Found")
                    return response;
            }
        }

        return {
            "404 Not Found",
            "text/plain",
            "Not Found"};
    }

    void serveFolder(const std::string &folder)
    {
        const std::string prefix =
            "/" + std::filesystem::path(folder).filename().string();
        staticFolders.push_back({folder, prefix});
    }

    void serveFolder(
        const std::string &folder,
        const std::string &urlPrefix)
    {
        std::string prefix = urlPrefix;

        if (prefix.empty() || prefix.front() != '/')
            prefix.insert(prefix.begin(), '/');

        while (prefix.size() > 1 && prefix.back() == '/')
            prefix.pop_back();

        staticFolders.push_back({folder, prefix});
    }
};