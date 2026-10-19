#include "server.hpp"

#include <csignal>

int main()
{
    std::string base = "./frontend";
    Server server;
    std::signal(SIGINT, Server::handleSignal);

    server.serveFolder(base + "/public");
    server.get("/", base + "/pages/index.html");
    server.get("/users", [](const HTTP::HTTPRequest &req)
               { return HTTP::HTTPResponse{"200 OK", "application/json", "[\"Aadith\",\"Athul\",\"Rahul\", \"Clara\"]"}; });
    server.post("/login", [](const HTTP::HTTPRequest &req)
    {
        std::string responsePayload = "{\"status\" : \"success\",\"message\" : \"User added\", \"user\": " + req.body + "}";
        std::cout << "POST body: " << req.body << '\n';\
        return HTTP::HTTPResponse{"201 Created", "application/json", responsePayload}; });
    if (server.start() != 0)
        return 1;
    server.run();
    return 0;
}