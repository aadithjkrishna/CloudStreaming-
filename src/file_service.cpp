#include "file_service.hpp"

// HELPER FUNCTIONS

FILEFORMAT findFormat(const std::string& file_name)
{
    std::string format_str;
    size_t index = file_name.rfind('.');

    if (index != std::string::npos)
        format_str = file_name.substr(index + 1);

    std::transform(
        format_str.begin(),
        format_str.end(),
        format_str.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    if (format_str == "mp3")  return MP3;
    if (format_str == "flac") return FLAC;
    if (format_str == "m4a")  return M4A;
    if (format_str == "opus") return OPUS;
    if (format_str == "png")  return PNG;
    if (format_str == "jpg")  return JPG;
    if (format_str == "jpeg") return JPEG;
    if (format_str == "webp") return WEBP;

    return UNKNOWN;
}

File extractMetadata(const std::filesystem::path& path)
{
    TagLib::FileRef file(path.c_str());
    const bool isFile = !file.isNull();

    const size_t file_size = std::filesystem::file_size(path);

    unsigned int date = 0;
    unsigned int track = 0;
    int duration = 0;
    int bitrate = 0;
    int sample_rate = 0;

    std::string file_name = path.filename().string();
    std::string file_path = path.string();

    std::string cover_image_path = extractCoverArtUniversal(
        file_path,
        "./covers"
    );

    std::string url_path =
        HTTP::convertPathToURL(path.filename().string());

    std::string title;
    std::string artist;
    std::string album;
    std::string genre;

    TagLib::Tag* tag = file.tag();

    if (isFile && tag)
    {
        title = tag->title().toCString(true);
        artist = tag->artist().toCString(true);
        album = tag->album().toCString(true);
        date = tag->year();
        genre = tag->genre().toCString(true);
        track = tag->track();
    }

    TagLib::AudioProperties* properties = file.audioProperties();

    if (isFile && properties)
    {
        duration = properties->lengthInSeconds();
        bitrate = properties->bitrate();
        sample_rate = properties->sampleRate();
    }

    return File(
        file_size,
        date,
        track,
        duration,
        bitrate,
        sample_rate,
        file_name,
        file_path,
        cover_image_path,
        url_path,
        title,
        artist,
        album,
        genre
    );
}

// CLASS FUNCTIONS

void FileService::listFiles()
{
    findFormat("cherathukal.flac");
    for (auto &file : files)
    {
#ifdef DEBUG
        file.printFile();
#endif
    }
}

void FileService::extractMetadataFiles()
{
    const std::filesystem::path music_directory =
        std::filesystem::path(base_path) / "audios";

    for (const auto& entry :
         std::filesystem::directory_iterator(music_directory))
    {
        if (!entry.is_regular_file())
            continue;

        const auto format = findFormat(
            entry.path().filename().string()
        );

        if (format != MP3 &&
            format != FLAC &&
            format != M4A &&
            format != OPUS)
            continue;

        files.emplace_back(extractMetadata(entry.path()));
    }
}