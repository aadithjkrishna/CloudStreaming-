#pragma once

#include <vector>
#include <functional>
#include "http.hpp"

using Handler = std::function<HTTP::HTTPResponse(const HTTP::HTTPRequest &)>;
struct Route
{
    HTTPMethod method;
    std::string path;
    Handler handler;
    Route(
        HTTPMethod httpMethod,
        const std::string &httpPath,
        Handler httpHandler)
        : method(httpMethod),
          path(httpPath),
          handler(std::move(httpHandler))
    {
    }
};

class Router
{
private:
    std::vector<Route> routes;
    std::vector<std::string> staticFolders;

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
        routes.push_back({HTTPMethod::GET, path, std::move(handler)});
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

    void serveFolder(const std::string &folder)
    {
        staticFolders.push_back(folder);
    }

    HTTP::HTTPResponse handleFile(
        const std::string &path,
        const std::string &folderPath)
    {
        std::string filePath = folderPath + path;
        std::string content;

        if (HTTP::readFile(filePath, content))
        {
            return {
                "200 OK",
                HTTP::getMimeType(path),
                content};
        }

        return {
            "404 Not Found",
            "text/plain",
            "Not Found"};
    }

    HTTP::HTTPResponse route(const HTTP::HTTPRequest &request)
    {
        for (const auto &route : routes)
        {
            if (route.method == request.method &&
                route.path == request.path)
            {
                return route.handler(request);
            }
        }
        if (request.method == HTTPMethod::GET)
        {
            for (const auto &folder : staticFolders)
            {
                HTTP::HTTPResponse response =
                    handleFile(request.path, folder);

                if (response.statusCode != "404 Not Found")
                    return response;
            }
        }
        return {
            "404 Not Found",
            "text/plain",
            "Not Found"};
    }
};