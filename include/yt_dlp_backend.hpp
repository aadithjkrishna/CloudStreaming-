#include "file_service.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <cctype>
#include <stdexcept>
#include <array>
#include <cstdio>
#include <memory>
#include <sys/wait.h>

class YT_DLP
{
public:
    inline static constexpr char cookie_path[] = "/home/aadith/Documents/CPP/MusicServer/cookies_youtube_aadith.txt";
    static std::string exec(const std::string &cmd);
    static std::string retrieveSongs(
        const std::string &query,
        int limit = 10);
    static std::string getAudioStreamUrl(
        const std::string &videoId);
    static std::optional<File> downloadAudio(
        const std::string &videoId,
        const std::filesystem::path &musicDirectory,
        FileService &fileService);
};