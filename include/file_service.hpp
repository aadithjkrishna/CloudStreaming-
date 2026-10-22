#pragma once

// FileService reads files in the given directory
// Indexes them and saves them with relevant info
// Handles Queries

// #define DEBUG

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <utility>
#include <vector>

#include "http.hpp"
#include "cover_extractor.hpp"

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

class File
{
public:
    inline static int next_id = 0;
    int id = 0;
    size_t file_size = 0;
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

public:
    File(size_t file_size,
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
         std::string genre) : id(++next_id),
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

#ifdef DEBUG
    void printFile()
    {
        std::cout << "ID: " << id << "\nFile size: " << (float)file_size / (1024 * 1024) << "MB" << "\nDate: " << date << "\nTrack: " << track << "\nDuration: " << duration << "\nBitrate: " << bitrate << "\nSample Rate: " << sample_rate << "\nFile Name: " << file_name << "\nFile path: " << file_path << "\nCover path: " << cover_image_path << "\nURL Path: " << url_path << "\nTitle: " << title << "\nArtist: " << artist << "\nAlbum: " << album << "\nGenre: " << genre << "\n\n";
    }
#endif
};

class FileService
{
    const std::string base_path = "/home/aadith/Documents/CPP/MusicServer";
    const std::string path = "/audios";

public:
    std::vector<File> files;
    void listFiles();
    void extractMetadataFiles();
};