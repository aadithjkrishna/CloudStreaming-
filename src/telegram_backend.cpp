#include "telegram_backend.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/wait.h>
#include <thread>

using json = nlohmann::json;

namespace
{
    std::string s_server_url;

    std::string shellQuote(const std::string &value)
    {
        std::string result = "'";
        for (char c : value)
        {
            if (c == '\'')
                result += "'\\''";
            else
                result += c;
        }
        result += "'";
        return result;
    }

    std::string urlEncode(const std::string &value)
    {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (unsigned char c : value)
        {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            {
                escaped << c;
            }
            else
            {
                escaped << '%' << std::setw(2) << std::uppercase << static_cast<int>(c);
            }
        }

        return escaped.str();
    }

    std::string getString(const json &object, const char *key)
    {
        if (!object.is_object())
            return {};

        const auto it = object.find(key);
        if (it == object.end() || !it->is_string())
            return {};

        return it->get<std::string>();
    }

    std::string cleanFileId(const std::string &rawId)
    {
        std::string id = rawId;
        // Strip common prefixes like 'telegram_' or 'tg_'
        if (id.rfind("telegram_", 0) == 0)
        {
            id = id.substr(9);
        }
        else if (id.rfind("tg_", 0) == 0)
        {
            id = id.substr(3);
        }

        // Validate numeric file ID
        if (id.empty())
        {
            throw std::invalid_argument("Telegram file ID cannot be empty");
        }

        for (unsigned char c : id)
        {
            if (!std::isdigit(c))
            {
                throw std::invalid_argument("Invalid Telegram file ID: " + rawId);
            }
        }

        return id;
    }

    std::string sanitizeFilename(const std::string &filename)
    {
        std::string clean;
        for (char c : filename)
        {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|' || c == '\0')
            {
                clean += '_';
            }
            else
            {
                clean += c;
            }
        }
        while (!clean.empty() && (clean.front() == '.' || clean.front() == ' '))
        {
            clean.erase(clean.begin());
        }
        return clean.empty() ? "telegram_audio" : clean;
    }

    std::string detectAudioExtension(const std::filesystem::path &filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
            return ".flac";

        std::array<unsigned char, 16> header{};
        file.read(reinterpret_cast<char *>(header.data()), header.size());
        const std::streamsize readBytes = file.gcount();

        // FLAC: 'fLaC'
        if (readBytes >= 4 &&
            header[0] == 'f' && header[1] == 'L' && header[2] == 'a' && header[3] == 'C')
        {
            return ".flac";
        }

        // MP3 with ID3v2 tag: 'ID3'
        if (readBytes >= 3 &&
            header[0] == 'I' && header[1] == 'D' && header[2] == '3')
        {
            return ".mp3";
        }

        // MP3 raw frame sync: 0xFF 0xFB, 0xFF 0xF3, 0xFF 0xF2
        if (readBytes >= 2 &&
            header[0] == 0xFF && (header[1] & 0xE0) == 0xE0)
        {
            return ".mp3";
        }

        // Ogg / Opus / Vorbis: 'OggS'
        if (readBytes >= 4 &&
            header[0] == 'O' && header[1] == 'g' && header[2] == 'g' && header[3] == 'S')
        {
            return ".opus";
        }

        // M4A / AAC (MP4 container with ftyp)
        if (readBytes >= 8 &&
            header[4] == 'f' && header[5] == 't' && header[6] == 'y' && header[7] == 'p')
        {
            return ".m4a";
        }

        return "";
    }

    std::string detectImageExtension(const std::string &data)
    {
        const auto size = data.size();
        if (size >= 3)
        {
            const auto *bytes = reinterpret_cast<const unsigned char *>(data.data());

            // JPEG
            if (bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF)
                return ".jpg";

            // PNG
            if (size >= 8 &&
                bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G')
                return ".png";

            // WebP
            if (size >= 12 &&
                bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
                bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P')
                return ".webp";
        }

        return ".jpg";
    }

    std::string extractFilenameFromHeaders(const std::string &headers)
    {
        const std::string needle = "filename=\"";
        auto pos = headers.find(needle);
        if (pos != std::string::npos)
        {
            pos += needle.size();
            auto endPos = headers.find('"', pos);
            if (endPos != std::string::npos)
            {
                return headers.substr(pos, endPos - pos);
            }
        }

        // Try unquoted filename=
        const std::string needleUnquoted = "filename=";
        pos = headers.find(needleUnquoted);
        if (pos != std::string::npos)
        {
            pos += needleUnquoted.size();
            auto endPos = headers.find_first_of(";\r\n", pos);
            if (endPos != std::string::npos)
            {
                return headers.substr(pos, endPos - pos);
            }
            return headers.substr(pos);
        }

        return {};
    }
}

void TelegramBackend::setServerUrl(const std::string &url)
{
    s_server_url = url;
    while (!s_server_url.empty() && s_server_url.back() == '/')
    {
        s_server_url.pop_back();
    }
}

std::string TelegramBackend::getServerUrl()
{
    if (!s_server_url.empty())
    {
        return s_server_url;
    }

    const char *envUrl = std::getenv("TELEGRAM_SERVER_URL");
    if (envUrl != nullptr && *envUrl != '\0')
    {
        std::string url = envUrl;
        while (!url.empty() && url.back() == '/')
        {
            url.pop_back();
        }
        return url;
    }

    return default_server_url;
}

std::string TelegramBackend::exec(const std::string &cmd)
{
    if (cmd.empty())
        throw std::invalid_argument("Command cannot be empty");

    using PipePtr = std::unique_ptr<FILE, int (*)(FILE *)>;
    PipePtr pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe)
        throw std::runtime_error("popen() failed for command: " + cmd);

    std::string output;
    std::array<char, 8192> buffer{};

    while (true)
    {
        const std::size_t bytesRead =
            fread(buffer.data(), 1, buffer.size(), pipe.get());

        if (bytesRead > 0)
            output.append(buffer.data(), bytesRead);

        if (bytesRead < buffer.size())
        {
            if (feof(pipe.get()))
                break;

            if (ferror(pipe.get()))
            {
                FILE *rawPipe = pipe.release();
                (void)pclose(rawPipe);
                throw std::runtime_error("Failed to read command output");
            }
        }
    }

    FILE *rawPipe = pipe.release();
    const int status = pclose(rawPipe);

    if (status == -1)
        throw std::runtime_error("pclose() failed");

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        const std::string exitStatus =
            WIFEXITED(status)
                ? std::to_string(WEXITSTATUS(status))
                : "terminated by signal";

        throw std::runtime_error("Command failed (" + exitStatus + "): " + cmd);
    }

    return output;
}

bool TelegramBackend::isAvailable()
{
    try
    {
        // Keep curl silent and redirect stderr so polling does not spam the terminal
        const std::string cmd =
            "curl --fail --silent --max-time 1 " +
            shellQuote(getServerUrl() + "/api/status") + " 2>/dev/null";
        const std::string resp = exec(cmd);
        const json j = json::parse(resp);
        return j.value("status", "") == "ok";
    }
    catch (...)
    {
        return false;
    }
}

bool TelegramBackend::isAuthorized()
{
    try
    {
        const std::string cmd =
            "curl --fail --silent --max-time 1 " +
            shellQuote(getServerUrl() + "/api/status") + " 2>/dev/null";
        const std::string resp = exec(cmd);
        const json j = json::parse(resp);
        return j.value("authorized", false);
    }
    catch (...)
    {
        return false;
    }
}

bool TelegramBackend::ensureRunning(const std::string &binaryPath, int timeoutMs)
{
    if (isAvailable())
    {
        return true;
    }

    std::vector<std::string> candidates;
    if (!binaryPath.empty())
    {
        candidates.push_back(binaryPath);
    }

    const char *envPath = std::getenv("TELEGRAM_DAEMON_PATH");
    if (envPath != nullptr && *envPath != '\0')
    {
        candidates.push_back(envPath);
    }

    // Default search paths relative to common run locations
    candidates.push_back("./telegram_audio_downloader");
    candidates.push_back("./bin/telegram_audio_downloader");
    candidates.push_back("../Telegram/build/telegram_audio_downloader");
    candidates.push_back("/home/aadith/Documents/Telegram/build/telegram_audio_downloader");

    std::string foundPath;
    for (const auto &candidate : candidates)
    {
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec))
        {
            foundPath = candidate;
            break;
        }
    }

    if (foundPath.empty())
    {
        std::cerr << "[TelegramBackend] Could not locate telegram_audio_downloader executable.\n";
        return false;
    }

    std::filesystem::path binFs(foundPath);
    binFs = std::filesystem::absolute(binFs).lexically_normal();

    // Determine the Telegram project root (where .env and .tdlib_session reside)
    std::filesystem::path workDir = binFs.parent_path();
    if (workDir.filename() == "build" || workDir.filename() == "bin")
    {
        workDir = workDir.parent_path();
    }

    std::cout << "[TelegramBackend] Telegram server offline on " << getServerUrl()
              << ". Auto-starting background daemon: " << binFs.string()
              << " (working dir: " << workDir.string() << ") ...\n";

    // Launch daemon in background with --server inside its project root directory
    const std::string startCmd =
        "(cd " + shellQuote(workDir.string()) + " && " +
        shellQuote(binFs.string()) + " --server) > /tmp/telegram_audio_downloader.log 2>&1 &";

    std::system(startCmd.c_str());

    // Poll until /api/status responds or timeout is reached (TDLib sync can take 3-5 seconds)
    const int stepMs = 200;
    const int maxAttempts = std::max(1, timeoutMs / stepMs);

    for (int i = 0; i < maxAttempts; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
        if (isAvailable())
        {
            std::cout << "[TelegramBackend] Background daemon online and ready!\n";
            return true;
        }
    }

    std::cerr << "[TelegramBackend] Timed out waiting for Telegram server to become available.\n"
              << "[TelegramBackend] Check /tmp/telegram_audio_downloader.log for details.\n";
    return false;
}

std::string TelegramBackend::retrieveSongs(const std::string &query, int limit)
{
    if (query.empty())
        throw std::invalid_argument("Search query cannot be empty");

    if (limit < 1 || limit > 100)
        throw std::invalid_argument("Search limit must be between 1 and 100");

    if (!isAvailable())
    {
        ensureRunning();
        if (!isAvailable())
        {
            return json::array().dump();
        }
    }

    const std::string baseUrl = getServerUrl();
    const std::string searchUrl =
        baseUrl + "/api/search?q=" + urlEncode(query) +
        "&type=audio&limit=" + std::to_string(limit);

    const std::string cmd =
        "curl --fail --silent --show-error --max-time 30 " +
        shellQuote(searchUrl);

    std::string output;
    try
    {
        output = exec(cmd);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Telegram search failed: " << e.what() << '\n';
        return json::array().dump();
    }

    if (output.empty())
        return json::array().dump();

    json document;
    try
    {
        document = json::parse(output);
    }
    catch (...)
    {
        return json::array().dump();
    }

    if (!document.is_array())
        return json::array().dump();

    json results = json::array();

    for (const auto &entry : document)
    {
        if (!entry.is_object())
            continue;

        int64_t fileId = 0;
        if (entry.contains("file_id") && entry["file_id"].is_number())
        {
            fileId = entry["file_id"].get<int64_t>();
        }

        if (fileId <= 0)
            continue;

        std::string title = getString(entry, "title");
        std::string performer = getString(entry, "performer");
        std::string fileName = getString(entry, "file_name");
        std::string chatTitle = getString(entry, "chat_title");
        std::string quality = getString(entry, "quality");
        std::string caption = getString(entry, "caption");
        int durationSec = entry.value("duration_sec", 0);
        int64_t sizeBytes = entry.value("size_bytes", int64_t{0});

        if (title.empty())
        {
            if (!fileName.empty())
            {
                auto dot = fileName.rfind('.');
                title = (dot != std::string::npos) ? fileName.substr(0, dot) : fileName;
            }
            else
            {
                title = "Telegram Track " + std::to_string(fileId);
            }
        }

        std::string streamUrl = baseUrl + "/api/stream?id=" + std::to_string(fileId);

        std::string thumbnailUrl;
        if (entry.contains("thumbnail_url") && entry["thumbnail_url"].is_string())
        {
            std::string tUrl = entry["thumbnail_url"].get<std::string>();
            if (!tUrl.empty())
            {
                thumbnailUrl = baseUrl + tUrl;
            }
        }

        if (thumbnailUrl.empty() && entry.contains("minithumbnail") && entry["minithumbnail"].is_string())
        {
            thumbnailUrl = entry["minithumbnail"].get<std::string>();
        }

        json song;
        song["id"] = std::to_string(fileId);
        song["title"] = title;
        song["artist"] = performer.empty() ? chatTitle : performer;
        song["album"] = chatTitle;
        song["url"] = streamUrl;
        song["thumbnail"] = thumbnailUrl;
        song["img_url"] = thumbnailUrl;
        song["source"] = "telegram";
        song["downloaded"] = false;
        song["file_name"] = fileName;
        song["duration"] = durationSec;
        song["size"] = sizeBytes;
        song["quality"] = quality;
        if (!caption.empty())
        {
            song["caption"] = caption;
        }

        results.push_back(std::move(song));
    }

    return results.dump();
}

std::string TelegramBackend::getAudioStreamUrl(const std::string &fileId)
{
    const std::string cleanId = cleanFileId(fileId);
    return getServerUrl() + "/api/stream?id=" + cleanId;
}

std::optional<File> TelegramBackend::downloadAudio(
    const std::string &fileId,
    const std::filesystem::path &musicDirectory,
    FileService &fileService)
{
    return downloadAudio(fileId, "", "", musicDirectory, "./covers", fileService);
}

std::optional<File> TelegramBackend::downloadAudio(
    const std::string &fileId,
    const std::string &thumbnailUrl,
    const std::filesystem::path &musicDirectory,
    const std::filesystem::path &coverDirectory,
    FileService &fileService)
{
    return downloadAudio(fileId, "", thumbnailUrl, musicDirectory, coverDirectory, fileService);
}

std::optional<File> TelegramBackend::downloadAudio(
    const std::string &fileId,
    const std::string &preferredFileName,
    const std::string &thumbnailUrl,
    const std::filesystem::path &musicDirectory,
    const std::filesystem::path &coverDirectory,
    FileService &fileService)
{
    const std::string cleanId = cleanFileId(fileId);
    const std::string baseUrl = getServerUrl();

    if (!isAvailable())
    {
        ensureRunning();
        if (!isAvailable())
        {
            throw std::runtime_error("Telegram server is not running on " + baseUrl);
        }
    }

    std::filesystem::create_directories(musicDirectory);
    std::filesystem::create_directories(coverDirectory);

    std::string downloadUrl = baseUrl + "/api/download?id=" + cleanId;
    std::string targetFilename;

    if (!preferredFileName.empty())
    {
        targetFilename = sanitizeFilename(preferredFileName);
    }
    else
    {
        // Query headers to discover original filename from Content-Disposition
        try
        {
            const std::string headCmd =
                "curl --silent --head --location --max-time 10 " +
                shellQuote(downloadUrl);
            const std::string headers = exec(headCmd);
            const std::string headerName = extractFilenameFromHeaders(headers);
            if (!headerName.empty())
            {
                targetFilename = sanitizeFilename(headerName);
            }
        }
        catch (...)
        {
            // Fallback if HEAD request fails
        }
    }

    if (targetFilename.empty())
    {
        targetFilename = "telegram_" + cleanId + ".flac";
    }

    std::filesystem::path outputPath = musicDirectory / targetFilename;

    // Download audio file with curl
    const std::string downloadCmd =
        "curl --fail --location --silent --show-error --max-time 600 -o " +
        shellQuote(outputPath.string()) + " " +
        shellQuote(downloadUrl);

    exec(downloadCmd);

    if (!std::filesystem::is_regular_file(outputPath) ||
        std::filesystem::file_size(outputPath) == 0)
    {
        throw std::runtime_error(
            "Telegram audio download completed without a valid output file: " + outputPath.string());
    }

    // Inspect magic bytes and verify audio extension compatibility for FileService
    const std::string realExt = detectAudioExtension(outputPath);
    const std::string curExt = outputPath.extension().string();

    if (!realExt.empty() && curExt != realExt)
    {
        std::filesystem::path correctedPath = outputPath;
        correctedPath.replace_extension(realExt);
        std::error_code ec;
        std::filesystem::rename(outputPath, correctedPath, ec);
        if (!ec)
        {
            outputPath = correctedPath;
        }
    }

    // Download cover image if available
    std::filesystem::path coverPath;
    if (!thumbnailUrl.empty())
    {
        std::string fullThumbUrl = thumbnailUrl;
        if (fullThumbUrl.rfind("/", 0) == 0)
        {
            fullThumbUrl = baseUrl + fullThumbUrl;
        }

        // Only download if http(s) URL (not inline data URI)
        if (fullThumbUrl.rfind("http://", 0) == 0 || fullThumbUrl.rfind("https://", 0) == 0)
        {
            try
            {
                const std::string coverCmd =
                    "curl --fail --location --silent --show-error"
                    " --max-time 20 --max-filesize 10485760 " +
                    shellQuote(fullThumbUrl);

                const std::string imageData = exec(coverCmd);

                if (!imageData.empty())
                {
                    std::string imgExt = detectImageExtension(imageData);
                    coverPath = coverDirectory / (outputPath.stem().string() + imgExt);

                    std::ofstream out(coverPath, std::ios::binary | std::ios::trunc);
                    if (out)
                    {
                        out.write(imageData.data(), static_cast<std::streamsize>(imageData.size()));
                        if (!out)
                        {
                            out.close();
                            std::filesystem::remove(coverPath);
                            coverPath.clear();
                        }
                    }
                    else
                    {
                        coverPath.clear();
                    }
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "Cover download failed: " << e.what() << '\n';
                coverPath.clear();
            }
        }
    }

    // Index downloaded file with FileService (Tagged as Local source)
    return fileService.addDownloadedFile(
        outputPath,
        SongSource::Local,
        coverPath);
}
