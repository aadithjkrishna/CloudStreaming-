#include "server.hpp"

#include <csignal>
#include <filesystem>
#include <nlohmann/json.hpp>

int main()
{
    std::string base = "./frontend";
    Server server;

    std::signal(SIGINT, Server::handleSignal);

    server.serveFolder("./frontend/public", "/");
    server.serveFolder("./audios", "/audios");
    server.serveFolder("./covers", "/covers");

    server.get("/", base + "/pages/index.html");

    server.get("/users", [](const HTTP::HTTPRequest &req)
               { return HTTP::HTTPResponse{
                     "200 OK",
                     "application/json",
                     R"(["Aadith","Athul","Rahul","Clara"])"}; });

    server.post("/login", [](const HTTP::HTTPRequest &req)
                {
        std::string responsePayload =
            "{\"status\":\"success\",\"message\":\"User added\",\"user\":" +
            req.body + "}";

        std::cout << "POST body: " << req.body << '\n';

        return HTTP::HTTPResponse{
            "201 Created",
            "application/json",
            responsePayload
        }; });

    server.get("/songs", [&](const HTTP::HTTPRequest &req)
               {
        nlohmann::json response;
        response["songs"] = nlohmann::json::array();

        for (const auto& file : server.fs.files)
        {
            nlohmann::json song;

            song["title"] = file.title;
            song["artist"] = file.artist;
            song["album"] = file.album;
            song["audio_path"] = "/audios/" + file.url_path;

            if (!file.cover_image_path.empty())
            {
                song["cover_path"] =
                    "/covers/" +
                    std::filesystem::path(file.cover_image_path)
                        .filename().string();
            }
            else
            {
                song["cover_path"] = nullptr;
            }

            response["songs"].push_back(song);
        }

        return HTTP::HTTPResponse{
            "200 OK",
            "application/json",
            response.dump()
        }; });

    if (server.start() != 0)
        return 1;

    server.run();
    return 0;
}