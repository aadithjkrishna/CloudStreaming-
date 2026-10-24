#include "server.hpp"

#include <csignal>
#include <filesystem>
#include <nlohmann/json.hpp>

int main()
{
    std::string base = "./frontend";
    TelegramBackend::ensureRunning();
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
            song["source"] =
                file.source == SongSource::YouTube ? "youtube" : "local";
            song["downloaded"] = true;
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
        return HTTP::HTTPResponse{"400 Bad Request", "application/json",
                                  R"({"error":"Missing query parameter 'q'"})"};
    }

    const std::string query = qIt->second;
    int limit = 10;
    if (auto lIt = request.query.find("limit"); lIt != request.query.end()) {
        try { limit = std::stoi(lIt->second); } catch (...) {}
    }

    // Check optional source: "telegram", "youtube" (default: "all")
    std::string source = "all";
    if (auto sIt = request.query.find("source"); sIt != request.query.end()) {
        source = sIt->second;
    }

    try
    {
        // 1. Telegram only (Instant response ~200ms)
        if (source == "telegram" || source == "tg" || source == "tl")
        {
            return HTTP::HTTPResponse{
                "200 OK",
                "application/json",
                TelegramBackend::retrieveSongs(query, limit)
            };
        }

        // 2. YouTube only
        if (source == "youtube" || source == "yt")
        {
            return HTTP::HTTPResponse{
                "200 OK",
                "application/json",
                YT_DLP::retrieveSongs(query, limit)
            };
        }

        // 3. Fallback: Both combined (if source=all)
        nlohmann::json combined = nlohmann::json::array();
        
        // Execute Telegram
        try {
            auto tgJson = nlohmann::json::parse(TelegramBackend::retrieveSongs(query, limit));
            for (auto& item : tgJson) combined.push_back(std::move(item));
        } catch (...) {}

        // Execute YouTube
        try {
            auto ytJson = nlohmann::json::parse(YT_DLP::retrieveSongs(query, limit));
            for (auto& item : ytJson) combined.push_back(std::move(item));
        } catch (...) {}

        return HTTP::HTTPResponse{"200 OK", "application/json", combined.dump()};
    }
    catch (const std::exception &e)
    {
        return HTTP::HTTPResponse{"500 Internal Server Error", "application/json",
                                  std::string("{\"error\":\"") + e.what() + "\"}"};
    } });

    server.get("/song/{id}", [&server](const HTTP::HTTPRequest &request)
               {
    const auto idIt = request.params.find("id");
    if (idIt == request.params.end())
    {
        return HTTP::HTTPResponse{
            "400 Bad Request",
            "application/json",
            R"({"error":"Missing song ID"})"
        };
    }

    // Check optional source: "telegram", "youtube"
    std::string source = "youtube";
    if (auto sIt = request.query.find("source"); sIt != request.query.end()) {
        source = sIt->second;
    } else {
        // Fallback: If ID is numeric, it is a Telegram message/file ID
        bool allDigits = !idIt->second.empty() && 
            std::all_of(idIt->second.begin(), idIt->second.end(), ::isdigit);
        if (allDigits) source = "telegram";
    }

    const auto thumbnailIt = request.query.find("img_url");
    const std::string thumbnailUrl =
        thumbnailIt != request.query.end() ? thumbnailIt->second : "";

    try
    {
        std::optional<File> file;

        if (source == "telegram" || source == "tg")
        {
            file = TelegramBackend::downloadAudio(
                idIt->second,
                thumbnailUrl,
                "./audios",
                "./covers",
                server.fs);
        }
        else
        {
            file = YT_DLP::downloadAudio(
                idIt->second,
                thumbnailUrl,
                "./audios",
                "./covers",
                server.fs);
        }

        if (!file)
        {
            return HTTP::HTTPResponse{
                "500 Internal Server Error",
                "application/json",
                R"({"error":"Audio was downloaded but could not be indexed"})"
            };
        }

        nlohmann::json response;
        response["id"] = file->id;
        response["title"] = file->title;
        response["artist"] = file->artist;
        response["source"] = (source == "telegram" || source == "tg") ? "telegram" : "youtube";
        response["downloaded"] = true;
        response["audio_path"] = "/audios/" + file->url_path;
        response["audio_url"] = "/audios/" + file->url_path;

        if (!file->cover_image_path.empty())
        {
            response["cover_path"] =
                "/covers/" +
                std::filesystem::path(file->cover_image_path).filename().string();
        }
        else
        {
            response["cover_path"] = nullptr;
        }

        return HTTP::HTTPResponse{
            "200 OK",
            "application/json",
            response.dump()
        };
    }
    catch (const std::exception& error)
    {
        nlohmann::json response;
        response["error"] = error.what();

        return HTTP::HTTPResponse{
            "500 Internal Server Error",
            "application/json",
            response.dump()
        };
    } });

    if (server.start() != 0)
        return 1;

    server.run();
    return 0;
}