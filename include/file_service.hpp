#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

enum FILEFORMAT
{
    UNKNOWN,
    MP3,
    FLAC,
    M4A,
    OPUS,
    PNG,
    WEBP,
    JPEG = 7,
    JPG = 7
};

enum class SongSource {
    Local,
    YouTube
};

class File
{
public:
    inline static std::atomic<int> next_id{0};

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

    File() = default;

    File(std::size_t file_size,
         unsigned int date,
         unsigned int track,
         int duration,
         int bitrate,
         int sample_rate,
         std::string file_name,
         std::string file_path,
         std::string cover_image_path,
         std::string url_path,
         std::string title,
         std::string artist,
         std::string album,
         std::string genre);
};

class FileService
{
public:
    using NewFileCallback = std::function<void(const File&)>;
    using DeletedFileCallback = std::function<void(const File&)>;

    explicit FileService(
        std::filesystem::path music_directory,
        std::filesystem::path database_path,
        std::chrono::seconds scan_interval = std::chrono::minutes(2));

    ~FileService();

    FileService(const FileService&) = delete;
    FileService& operator=(const FileService&) = delete;

    void start();
    void stop();

    void setOnNewFile(NewFileCallback callback);
    void setOnDeletedFile(DeletedFileCallback callback);

    std::optional<File> addDownloadedFile(
        const std::filesystem::path& downloaded_path,
        SongSource source = SongSource::Local,
        const std::filesystem::path& cover_path = {});

    void scanOnce();

    std::vector<File> getFilesSnapshot() const;
    std::optional<File> findById(int id) const;
    std::optional<File> findByUrl(const std::string& url_path) const;

    std::vector<File> searchLocal(const std::string& query) const;

private:
    std::filesystem::path music_directory;
    std::filesystem::path database_path;
    std::chrono::seconds scan_interval;

    mutable std::mutex files_mutex;
    std::mutex scan_mutex;
    mutable std::mutex database_mutex;
    std::condition_variable stop_cv;

    std::vector<File> files;
    std::unordered_set<std::string> indexed_paths;

    NewFileCallback on_new_file;
    DeletedFileCallback on_deleted_file;

    std::thread worker;
    bool running = false;
    bool stopping = false;

    void workerLoop();
    void loadDatabase();
    void saveDatabase() const;
    std::optional<File> indexFile(const std::filesystem::path& path);
};