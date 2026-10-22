#include "file_service.hpp"

#include "cover_extractor.hpp"
#include "http.hpp"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>

namespace
{
using json = nlohmann::json;

FILEFORMAT findFormat(const std::string& file_name)
{
    const auto dot = file_name.rfind('.');
    if (dot == std::string::npos || dot + 1 >= file_name.size())
        return UNKNOWN;

    std::string extension = file_name.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    if (extension == "mp3")  return MP3;
    if (extension == "flac") return FLAC;
    if (extension == "m4a")  return M4A;
    if (extension == "opus") return OPUS;
    if (extension == "png")  return PNG;
    if (extension == "jpg")  return JPG;
    if (extension == "jpeg") return JPEG;
    if (extension == "webp") return WEBP;
    return UNKNOWN;
}

bool isAudioFormat(FILEFORMAT format)
{
    return format == MP3 || format == FLAC ||
           format == M4A || format == OPUS;
}

std::string pathKey(const std::filesystem::path& path)
{
    return std::filesystem::absolute(path).lexically_normal().string();
}

bool isWithinDirectory(const std::filesystem::path& file_path,
                       const std::filesystem::path& directory_path)
{
    const auto file = std::filesystem::absolute(file_path).lexically_normal();
    const auto directory = std::filesystem::absolute(directory_path).lexically_normal();

    auto file_part = file.begin();
    auto directory_part = directory.begin();

    for (; directory_part != directory.end(); ++directory_part, ++file_part)
    {
        if (file_part == file.end() || *file_part != *directory_part)
            return false;
    }

    return true;
}

void updateNextId(int id)
{
    int current = File::next_id.load();
    while (current < id &&
           !File::next_id.compare_exchange_weak(current, id))
    {
        // compare_exchange_weak updates current after a failed attempt.
    }
}

json serializeFile(const File& file)
{
    return json{
        {"id", file.id},
        {"file_size", file.file_size},
        {"date", file.date},
        {"track", file.track},
        {"duration", file.duration},
        {"bitrate", file.bitrate},
        {"sample_rate", file.sample_rate},
        {"file_name", file.file_name},
        {"file_path", file.file_path},
        {"cover_image_path", file.cover_image_path},
        {"url_path", file.url_path},
        {"title", file.title},
        {"artist", file.artist},
        {"album", file.album},
        {"genre", file.genre}
    };
}

File deserializeFile(const json& value)
{
    if (!value.is_object())
        throw std::runtime_error("database entry is not an object");

    File file;
    file.id = value.at("id").get<int>();
    file.file_size = value.value("file_size", std::size_t{0});
    file.date = value.value("date", 0u);
    file.track = value.value("track", 0u);
    file.duration = value.value("duration", 0);
    file.bitrate = value.value("bitrate", 0);
    file.sample_rate = value.value("sample_rate", 0);
    file.file_name = value.value("file_name", std::string{});
    file.file_path = value.value("file_path", std::string{});
    file.cover_image_path = value.value("cover_image_path", std::string{});
    file.url_path = value.value("url_path", std::string{});
    file.title = value.value("title", std::string{});
    file.artist = value.value("artist", std::string{});
    file.album = value.value("album", std::string{});
    file.genre = value.value("genre", std::string{});

    updateNextId(file.id);
    return file;
}

File extractMetadata(const std::filesystem::path& path)
{
    TagLib::FileRef file(path.c_str());
    const std::size_t file_size = std::filesystem::file_size(path);

    unsigned int date = 0;
    unsigned int track = 0;
    int duration = 0;
    int bitrate = 0;
    int sample_rate = 0;

    const std::string file_name = path.filename().string();
    const std::string file_path = pathKey(path);
    const std::string cover_image_path = extractCoverArtUniversal(
        file_path, "./covers");
    const std::string url_path = HTTP::convertPathToURL(file_name);

    std::string title;
    std::string artist;
    std::string album;
    std::string genre;

    if (!file.isNull())
    {
        if (TagLib::Tag* tag = file.tag())
        {
            title = tag->title().toCString(true);
            artist = tag->artist().toCString(true);
            album = tag->album().toCString(true);
            date = tag->year();
            genre = tag->genre().toCString(true);
            track = tag->track();
        }

        if (TagLib::AudioProperties* properties = file.audioProperties())
        {
            duration = properties->lengthInSeconds();
            bitrate = properties->bitrate();
            sample_rate = properties->sampleRate();
        }
    }

    return File(file_size, date, track, duration, bitrate, sample_rate,
                file_name, file_path, cover_image_path, url_path,
                title, artist, album, genre);
}
} // namespace

File::File(std::size_t file_size,
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
           std::string genre)
    : id(++next_id),
      file_size(file_size),
      date(date),
      track(track),
      duration(duration),
      bitrate(bitrate),
      sample_rate(sample_rate),
      file_name(std::move(file_name)),
      file_path(std::move(file_path)),
      cover_image_path(std::move(cover_image_path)),
      url_path(std::move(url_path)),
      title(std::move(title)),
      artist(std::move(artist)),
      album(std::move(album)),
      genre(std::move(genre))
{
}

FileService::FileService(std::filesystem::path music_directory,
                         std::filesystem::path database_path,
                         std::chrono::seconds scan_interval)
    : music_directory(std::filesystem::absolute(std::move(music_directory)).lexically_normal()),
      database_path(std::filesystem::absolute(std::move(database_path)).lexically_normal()),
      scan_interval(scan_interval)
{
    if (scan_interval <= std::chrono::seconds::zero())
        throw std::invalid_argument("Scan interval must be positive");
}

FileService::~FileService()
{
    // Destructors must not allow persistence exceptions to escape.
    try
    {
        stop();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Failed to stop FileService cleanly: "
                  << error.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "Failed to stop FileService cleanly\n";
    }
}

void FileService::setOnNewFile(NewFileCallback callback)
{
    std::lock_guard lock(files_mutex);
    if (running)
        throw std::logic_error("Configure callbacks before start()");
    on_new_file = std::move(callback);
}

void FileService::setOnDeletedFile(DeletedFileCallback callback)
{
    std::lock_guard lock(files_mutex);
    if (running)
        throw std::logic_error("Configure callbacks before start()");
    on_deleted_file = std::move(callback);
}

void FileService::start()
{
    {
        std::lock_guard lock(files_mutex);
        if (running)
            return;
        running = true;
        stopping = false;
    }

    try
    {
        std::filesystem::create_directories(music_directory);
        loadDatabase();
        scanOnce();
        worker = std::thread(&FileService::workerLoop, this);
    }
    catch (...)
    {
        {
            std::lock_guard lock(files_mutex);
            running = false;
            stopping = true;
        }
        stop_cv.notify_all();
        throw;
    }
}

void FileService::stop()
{
    {
        std::lock_guard lock(files_mutex);
        if (!running)
            return;
        stopping = true;
    }

    stop_cv.notify_all();
    if (worker.joinable())
        worker.join();

    {
        std::lock_guard lock(files_mutex);
        running = false;
    }

    saveDatabase();
}

void FileService::workerLoop()
{
    std::unique_lock lock(files_mutex);

    while (!stop_cv.wait_for(lock, scan_interval, [this] {
        return stopping;
    }))
    {
        lock.unlock();
        try
        {
            scanOnce();
        }
        catch (const std::exception& error)
        {
            std::cerr << "Music scan failed: " << error.what() << '\n';
        }
        lock.lock();
    }
}

void FileService::loadDatabase()
{
    std::ifstream input(database_path);
    if (!input)
    {
        // A missing database is normal on first startup. If the path exists
        // but cannot be opened, report it rather than silently ignoring it.
        std::error_code ec;
        const bool exists = std::filesystem::exists(database_path, ec);
        if (ec)
            throw std::runtime_error("Cannot inspect music database: " + ec.message());
        if (exists)
            throw std::runtime_error("Cannot open music database: " + database_path.string());
        return;
    }

    json document;
    input >> document;
    if (!document.is_array())
        throw std::runtime_error("Music database must be a JSON array");

    std::vector<File> loaded;
    std::unordered_set<std::string> paths;

    for (const auto& entry : document)
    {
        try
        {
            File file = deserializeFile(entry);
            if (file.id <= 0 || file.file_path.empty())
                continue;

            // This service manages one directory. Ignore stale entries from
            // outside it so the periodic scan does not treat them as library files.
            if (!isWithinDirectory(file.file_path, music_directory))
                continue;

            const std::string key = pathKey(file.file_path);
            if (!paths.insert(key).second)
                continue;

            file.file_path = key;
            loaded.push_back(std::move(file));
        }
        catch (const std::exception& error)
        {
            std::cerr << "Skipping invalid database entry: "
                      << error.what() << '\n';
        }
    }

    std::lock_guard lock(files_mutex);
    files = std::move(loaded);
    indexed_paths = std::move(paths);
}

void FileService::saveDatabase() const
{
    std::lock_guard database_lock(database_mutex);

    std::vector<File> snapshot;
    {
        std::lock_guard lock(files_mutex);
        snapshot = files;
    }

    json document = json::array();
    for (const auto& file : snapshot)
        document.push_back(serializeFile(file));

    if (!database_path.parent_path().empty())
        std::filesystem::create_directories(database_path.parent_path());

    std::filesystem::path temporary_path = database_path;
    temporary_path += ".tmp";

    {
        std::ofstream output(temporary_path, std::ios::out | std::ios::trunc);
        if (!output)
            throw std::runtime_error("Cannot write music database: " + temporary_path.string());

        output << document.dump(2) << '\n';
        output.flush();
        if (!output)
            throw std::runtime_error("Failed while writing music database: " + temporary_path.string());
    }

    // On Linux, rename atomically replaces an existing destination on the same filesystem.
    std::error_code ec;
    std::filesystem::rename(temporary_path, database_path, ec);
    if (ec)
    {
        std::filesystem::remove(temporary_path);
        throw std::runtime_error("Cannot replace music database: " + ec.message());
    }
}

std::optional<File> FileService::indexFile(const std::filesystem::path& path)
{
    const std::string key = pathKey(path);
    {
        std::lock_guard lock(files_mutex);
        if (indexed_paths.count(key) != 0)
            return std::nullopt;
    }

    File file = extractMetadata(path);
    file.file_path = key;

    NewFileCallback callback;
    {
        std::lock_guard lock(files_mutex);
        if (!indexed_paths.insert(key).second)
            return std::nullopt;
        files.push_back(file);
        callback = on_new_file;
    }

    // Persist the index before notifying other components.
    saveDatabase();

    if (callback)
    {
        try
        {
            callback(file);
        }
        catch (const std::exception& error)
        {
            std::cerr << "New-file callback failed: " << error.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "New-file callback failed\n";
        }
    }

    return file;
}

std::optional<File> FileService::addDownloadedFile(
    const std::filesystem::path& downloaded_path)
{
    try
    {
        if (!std::filesystem::is_regular_file(downloaded_path))
            return std::nullopt;

        if (!isWithinDirectory(downloaded_path, music_directory))
        {
            std::cerr << "Downloaded file must be inside the managed music directory: "
                      << downloaded_path << '\n';
            return std::nullopt;
        }

        if (!isAudioFormat(findFormat(downloaded_path.filename().string())))
            return std::nullopt;

        return indexFile(downloaded_path);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Could not index downloaded file: "
                  << error.what() << '\n';
        return std::nullopt;
    }
}

void FileService::scanOnce()
{
    std::lock_guard scan_lock(scan_mutex);

    std::error_code ec;
    if (!std::filesystem::is_directory(music_directory, ec))
    {
        if (ec)
            throw std::runtime_error("Music directory error: " + ec.message());
        throw std::runtime_error("Music directory does not exist: " + music_directory.string());
    }

    std::unordered_set<std::string> disk_paths;

    for (std::filesystem::directory_iterator it(music_directory, ec), end;
         !ec && it != end;
         it.increment(ec))
    {
        try
        {
            if (!it->is_regular_file())
                continue;

            const auto path = it->path();
            if (!isAudioFormat(findFormat(path.filename().string())))
                continue;

            const std::string key = pathKey(path);
            disk_paths.insert(key);

            try
            {
                indexFile(path);
            }
            catch (const std::exception& error)
            {
                std::cerr << "Could not index " << path << ": "
                          << error.what() << '\n';
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << "Could not inspect directory entry: "
                      << error.what() << '\n';
        }
    }

    if (ec)
    {
        // Do not remove records after an incomplete directory scan.
        throw std::runtime_error("Incomplete music scan: " + ec.message());
    }

    std::vector<File> deleted;
    DeletedFileCallback callback;
    {
        std::lock_guard lock(files_mutex);
        for (auto it = files.begin(); it != files.end();)
        {
            if (disk_paths.count(pathKey(it->file_path)) == 0)
            {
                indexed_paths.erase(pathKey(it->file_path));
                deleted.push_back(*it);
                it = files.erase(it);
            }
            else
            {
                ++it;
            }
        }
        callback = on_deleted_file;
    }

    if (!deleted.empty())
    {
        saveDatabase();
        if (callback)
        {
            for (const auto& file : deleted)
            {
                try
                {
                    callback(file);
                }
                catch (const std::exception& error)
                {
                    std::cerr << "Deleted-file callback failed: "
                              << error.what() << '\n';
                }
                catch (...)
                {
                    std::cerr << "Deleted-file callback failed\n";
                }
            }
        }
    }
}

std::vector<File> FileService::getFilesSnapshot() const
{
    std::lock_guard lock(files_mutex);
    return files;
}

std::optional<File> FileService::findById(int id) const
{
    std::lock_guard lock(files_mutex);
    for (const auto& file : files)
    {
        if (file.id == id)
            return file;
    }
    return std::nullopt;
}

std::optional<File> FileService::findByUrl(const std::string& url_path) const
{
    std::lock_guard lock(files_mutex);
    for (const auto& file : files)
    {
        if (file.url_path == url_path)
            return file;
    }
    return std::nullopt;
}

std::vector<File> FileService::searchLocal(const std::string& query) const
{
    const auto lowercase = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
        return value;
    };

    const std::string needle = lowercase(query);
    if (needle.empty())
        return {};

    std::vector<File> result;
    std::lock_guard lock(files_mutex);

    for (const auto& file : files)
    {
        const std::string text = lowercase(
            file.title + " " + file.artist + " " +
            file.album + " " + file.file_name);

        if (text.find(needle) != std::string::npos)
            result.push_back(file);
    }

    return result;
}
