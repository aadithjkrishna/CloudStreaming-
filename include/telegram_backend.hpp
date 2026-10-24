#pragma once

#if __has_include("file_service.hpp")
#include "file_service.hpp"
#else
#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class SongSource {
    Local,
    YouTube
};

class File {
public:
    int id = 0;
    std::size_t file_size = 0;
    unsigned int date = 0;
    unsigned int track = 0;
    int duration = 0;
    int bitrate = 0;
    int sample_rate = 0;
    std::string file_name;
    std::string file_path;
    std::string cover_image_path;
    std::string url_path;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    SongSource source = SongSource::Local;
};

class FileService {
public:
    std::optional<File> addDownloadedFile(
        const std::filesystem::path& downloaded_path,
        SongSource source = SongSource::Local,
        const std::filesystem::path& cover_path = {});
};
#endif

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <array>
#include <cstdio>
#include <memory>
#include <sys/wait.h>

/**
 * @brief Backend for searching, streaming, and downloading music from Telegram.
 * 
 * Communicates with the Telegram Streaming & Download HTTP Server (default: http://127.0.0.1:8080)
 * to provide searching across Telegram chats/channels, streaming audio URLs,
 * and downloading audio files with cover art directly into MusicServer's FileService.
 */
class TelegramBackend
{
public:
    // Default URL of the running Telegram HTTP streaming server
    inline static constexpr char default_server_url[] = "http://127.0.0.1:8080";

    // Set or get the Telegram backend HTTP service URL
    static void setServerUrl(const std::string &url);
    static std::string getServerUrl();

    // Shell command execution helper (mirrors YT_DLP::exec)
    static std::string exec(const std::string &cmd);

    // Health and authorization checks
    static bool isAvailable();
    static bool isAuthorized();

    // Ensures the Telegram background server is running.
    // If not reachable, searches for the executable and auto-spawns it in the background with --server.
    static bool ensureRunning(
        const std::string &binaryPath = "",
        int timeoutMs = 15000);

    // Search Telegram audio messages across connected chats and channels.
    // Returns a JSON array string matching the exact format expected by MusicServer.
    static std::string retrieveSongs(
        const std::string &query,
        int limit = 10);

    // Get the direct audio stream URL for on-the-fly streaming/playback
    static std::string getAudioStreamUrl(
        const std::string &fileId);

    // Download an audio track by Telegram file ID into musicDirectory and index via FileService
    static std::optional<File> downloadAudio(
        const std::string &fileId,
        const std::filesystem::path &musicDirectory,
        FileService &fileService);

    // Download an audio track and thumbnail by Telegram file ID into musicDirectory and coverDirectory
    static std::optional<File> downloadAudio(
        const std::string &fileId,
        const std::string &thumbnailUrl,
        const std::filesystem::path &musicDirectory,
        const std::filesystem::path &coverDirectory,
        FileService &fileService);

    // Download with preferred filename override
    static std::optional<File> downloadAudio(
        const std::string &fileId,
        const std::string &preferredFileName,
        const std::string &thumbnailUrl,
        const std::filesystem::path &musicDirectory,
        const std::filesystem::path &coverDirectory,
        FileService &fileService);
};

// Convenient alias matching MusicServer conventions
using Telegram = TelegramBackend;
