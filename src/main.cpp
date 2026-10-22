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
    server.get("/songs", [&server](const HTTP::HTTPRequest &req)
               {
    nlohmann::json response;
    response["songs"] = nlohmann::json::array();

    const auto files = server.fs.getFilesSnapshot();

    for (const auto& file : files)
    {
        nlohmann::json song;

        song["id"] = file.id;
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

        response["songs"].push_back(std::move(song));
    }
    return HTTP::HTTPResponse{
        "200 OK",
        "application/json",
        response.dump()
    }; });
    server.get("/search", [](const HTTP::HTTPRequest &request)
               {
    const auto qIt = request.query.find("q");
    if (qIt == request.query.end() || qIt->second.empty())
    {
        return HTTP::HTTPResponse{
            "400 Bad Request",
            "application/json",
            R"({"error":"Missing required query parameter: q"})"
        };
    }
    const std::string query = qIt->second;
    const auto limitIt = request.query.find("limit");
    const std::string limit =
        limitIt != request.query.end() ? limitIt->second : "10";
    std::cout << "Search query: " << query << '\n';
    std::cout << "Limit: " << limit << '\n';
    const std::string result = YT_DLP::retrieveSongs(query);
    return HTTP::HTTPResponse{
        "200 OK",
        "application/json",
        result
    }; });

    server.get("/song/{id}", [&server](const HTTP::HTTPRequest &request)
               {
        // std::cout << "ID: " << request.params.at("id") << "\n";
        std::string id = request.params.at("id");
        std::string audio_url = YT_DLP::getAudioStreamUrl(id);
        server.download(id);
        return HTTP::HTTPResponse{"200 OK", "application/json", "{\"audio_url\":\"" + audio_url + "\"}"}; });
    if (server.start() != 0)
        return 1;

    server.run();
    return 0;
}