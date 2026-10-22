
#pragma once

#include <taglib/fileref.h>
#include <taglib/tvariant.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

// Convert a string to lowercase safely.
inline std::string lowerCase(std::string text)
{
    std::transform(
        text.begin(), text.end(), text.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

    return text;
}

// Convert a TagLib string to UTF-8.
inline std::string tagStringToStd(const TagLib::String& value)
{
    const char* text = value.toCString(true);
    return text ? std::string(text) : std::string{};
}

// Determine the extension from MIME metadata.
inline std::string imageExtension(const std::string& mime)
{
    std::string type = lowerCase(mime);

    // Ignore optional MIME parameters.
    const auto semicolon = type.find(';');
    if (semicolon != std::string::npos)
        type.resize(semicolon);

    if (type == "image/jpeg" ||
        type == "image/jpg" ||
        type == "image/pjpeg")
        return ".jpg";

    if (type == "image/png" || type == "image/x-png")
        return ".png";

    if (type == "image/webp")
        return ".webp";

    if (type == "image/gif")
        return ".gif";

    if (type == "image/bmp" || type == "image/x-ms-bmp")
        return ".bmp";

    if (type == "image/avif")
        return ".avif";

    if (type == "image/heic")
        return ".heic";

    if (type == "image/heif")
        return ".heif";

    if (type == "image/tiff" || type == "image/x-tiff")
        return ".tiff";

    return "";
}

// Detect common image types using the binary signature.
// This is useful when metadata reports an unknown or incorrect MIME type.
inline std::string detectImageExtension(
    const TagLib::ByteVector& data,
    const std::string& mime)
{
    const auto size = data.size();

    if (size > 0)
    {
        const auto* bytes =
            reinterpret_cast<const unsigned char*>(data.data());

        auto matches = [&](std::size_t offset, const char* text)
        {
            const std::size_t length = std::strlen(text);

            return offset + length <= size &&
                   std::memcmp(bytes + offset, text, length) == 0;
        };

        // JPEG
        if (size >= 3 &&
            bytes[0] == 0xFF &&
            bytes[1] == 0xD8 &&
            bytes[2] == 0xFF)
            return ".jpg";

        // PNG
        if (size >= 8 &&
            bytes[0] == 0x89 &&
            matches(1, "PNG\r\n\x1A\n"))
            return ".png";

        // GIF
        if (matches(0, "GIF87a") || matches(0, "GIF89a"))
            return ".gif";

        // BMP
        if (matches(0, "BM"))
            return ".bmp";

        // WebP
        if (size >= 12 &&
            matches(0, "RIFF") &&
            matches(8, "WEBP"))
            return ".webp";

        // AVIF
        if (size >= 12 &&
            matches(4, "ftyp") &&
            (matches(8, "avif") || matches(8, "avis")))
            return ".avif";

        // HEIC / HEIF family
        if (size >= 12 && matches(4, "ftyp"))
        {
            if (matches(8, "heic") ||
                matches(8, "heix") ||
                matches(8, "hevc") ||
                matches(8, "hevx"))
                return ".heic";

            if (matches(8, "mif1") || matches(8, "msf1"))
                return ".heif";
        }

        // TIFF
        if (size >= 4 &&
            ((bytes[0] == 'I' && bytes[1] == 'I' &&
              bytes[2] == 42 && bytes[3] == 0) ||
             (bytes[0] == 'M' && bytes[1] == 'M' &&
              bytes[2] == 0 && bytes[3] == 42)))
            return ".tiff";
    }

    // Use the declared MIME type if the signature is unrecognized.
    return imageExtension(mime);
}

inline bool saveImage(
    const TagLib::ByteVector& data,
    const fs::path& outputPath)
{
    if (data.size() == 0)
    {
        std::cerr << "Cannot save empty artwork: "
                  << outputPath.string() << '\n';
        return false;
    }

    std::ofstream out(outputPath, std::ios::binary);

    if (!out)
    {
        std::cerr << "Cannot open output file: "
                  << outputPath.string() << '\n';
        return false;
    }

    out.write(
        data.data(),
        static_cast<std::streamsize>(data.size()));

    if (!out)
    {
        std::cerr << "Failed writing image: "
                  << outputPath.string() << '\n';
        return false;
    }

    return true;
}

// Extract an embedded picture through TagLib's common PICTURE interface.
inline std::string extractCoverArtUniversal(
    const std::string& filePath,
    const std::string& outputDirectory)
{
    TagLib::FileRef file(filePath.c_str(), false);

    if (file.isNull() ||
        file.file() == nullptr ||
        !file.file()->isValid())
    {
        std::cerr << "TagLib could not read the audio file: "
                  << filePath << '\n';
        return "";
    }

    const auto pictures =
        file.complexProperties(TagLib::String("PICTURE"));

    if (pictures.isEmpty())
    {
        std::cerr << "No embedded PICTURE metadata found: "
                  << filePath << '\n';
        return "";
    }

    TagLib::ByteVector fallbackImage;
    std::string fallbackExtension;

    TagLib::ByteVector selectedImage;
    std::string selectedExtension;

    for (const auto& picture : pictures)
    {
        // The common PICTURE interface returns the image data,
        // MIME type, and possibly the picture type.
        const TagLib::ByteVector data =
            picture.value(TagLib::String("data")).toByteVector();

        if (data.size() == 0)
            continue;

        const std::string mime = tagStringToStd(
            picture.value(TagLib::String("mimeType")).toString());

        const std::string extension =
            detectImageExtension(data, mime);

        // Skip images whose format cannot be identified.
        if (extension.empty())
            continue;

        const std::string type = lowerCase(
            tagStringToStd(
                picture.value(
                    TagLib::String("pictureType")).toString()));

        // Remember the first usable image as a fallback.
        if (fallbackImage.size() == 0)
        {
            fallbackImage = data;
            fallbackExtension = extension;
        }

        // Prefer the actual front cover, but don't require it.
        if (type == "front cover")
        {
            selectedImage = data;
            selectedExtension = extension;
            break;
        }
    }

    // If no image was explicitly marked as the front cover,
    // use the first supported image found.
    if (selectedImage.size() == 0)
    {
        selectedImage = fallbackImage;
        selectedExtension = fallbackExtension;
    }

    if (selectedImage.size() == 0)
    {
        std::cerr << "No usable embedded image found: "
                  << filePath << '\n';
        return "";
    }

    std::error_code ec;

    const fs::path directory =
        outputDirectory.empty()
            ? fs::path(".")
            : fs::path(outputDirectory);

    fs::create_directories(directory, ec);

    if (ec)
    {
        std::cerr << "Failed to create artwork directory '"
                  << directory.string() << "': "
                  << ec.message() << '\n';
        return "";
    }

    const fs::path audioFile(filePath);

    const fs::path outputPath =
        directory /
        (audioFile.filename().string() + selectedExtension);

    if (!saveImage(selectedImage, outputPath))
        return "";

    return outputPath.string();
}
