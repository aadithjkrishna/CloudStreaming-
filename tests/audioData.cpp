#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

// TagLib core headers
#include <taglib/fileref.h>
#include <taglib/tag.h>

// Format specific headers
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/opusfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/xiphcomment.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4item.h>

// 1. MP3 Cover Extraction (Fixed: Restored the missing function)
bool extractMp3Cover(const std::string& mp3Path, const std::string& outputPath) {
    TagLib::MPEG::File file(mp3Path.c_str());
    if (!file.isValid() || !file.ID3v2Tag()) return false;

    TagLib::ID3v2::Tag* tag = file.ID3v2Tag();
    TagLib::ID3v2::FrameList frames = tag->frameList("APIC");
    if (frames.isEmpty()) return false;

    for (auto* frame : frames) {
        auto* picFrame = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frame);
        if (picFrame && picFrame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
            TagLib::ByteVector imageData = picFrame->picture();
            
            std::ofstream outFile(outputPath, std::ios::binary);
            outFile.write(imageData.data(), imageData.size());
            
            std::cout << "Extracted MP3 cover (" << picFrame->mimeType().toCString(true) 
                      << ") to " << outputPath << std::endl;
            return true;
        }
    }
    return false;
}

// 2. MP4 Cover Extraction
bool extractMp4Cover(const std::string& mp4Path, const std::string& outputPath) {
    TagLib::MP4::File file(mp4Path.c_str());
    if (!file.isValid() || !file.tag()) return false;

    TagLib::MP4::Tag* tag = file.tag();

    if (tag->contains("covr")) {
        // FIX: Added 'const' (or use const auto& itemsMap = tag->itemMap();)
        const TagLib::MP4::ItemMap& itemsMap = tag->itemMap();
        
        // Grab the 'covr' item block and convert it to a CoverArtList
        TagLib::MP4::CoverArtList covers = itemsMap["covr"].toCoverArtList();
        
        if (!covers.isEmpty()) {
            TagLib::MP4::CoverArt cover = covers.front();
            TagLib::ByteVector imageData = cover.data();

            std::ofstream outFile(outputPath, std::ios::binary);
            outFile.write(imageData.data(), imageData.size());
            
            std::string formatStr = (cover.format() == TagLib::MP4::CoverArt::PNG) ? "image/png" : "image/jpeg";
            std::cout << "Extracted MP4 cover (" << formatStr << ") to " << outputPath << std::endl;
            return true;
        }
    }
    return false;
}


// Helper: Universal parser for XiphComment containers (Opus / Ogg)
bool extractXiphCover(TagLib::Ogg::XiphComment* comment, const std::string& outputPath) {
    if (!comment) return false;

    const TagLib::List<TagLib::FLAC::Picture*>& picList = comment->pictureList();
    if (picList.isEmpty()) return false;

    for (auto* pic : picList) {
        if (pic->type() == TagLib::FLAC::Picture::FrontCover) {
            TagLib::ByteVector imageData = pic->data();

            std::ofstream outFile(outputPath, std::ios::binary);
            outFile.write(imageData.data(), imageData.size());

            std::cout << "Extracted cover (" << pic->mimeType().toCString(true) 
                      << ") to " << outputPath << std::endl;
            return true;
        }
    }
    return false;
}

// 3. OPUS Wrapper
bool extractOpusCover(const std::string& opusPath, const std::string& outputPath) {
    TagLib::Ogg::Opus::File file(opusPath.c_str());
    if (!file.isValid()) return false;
    return extractXiphCover(file.tag(), outputPath);
}

// 4. OGG Wrapper
bool extractOggCover(const std::string& oggPath, const std::string& outputPath) {
    TagLib::Ogg::Vorbis::File file(oggPath.c_str());
    if (!file.isValid()) return false;
    return extractXiphCover(file.tag(), outputPath);
}

// 5. FLAC Cover Extraction
bool extractFlacCover(const std::string& flacPath, const std::string& outputPath) {
    TagLib::FLAC::File file(flacPath.c_str());
    if (!file.isValid()) return false;

    const TagLib::List<TagLib::FLAC::Picture*>& picList = file.pictureList();
    if (picList.isEmpty()) return false;

    for (auto* pic : picList) {
        if (pic->type() == TagLib::FLAC::Picture::FrontCover) {
            TagLib::ByteVector imageData = pic->data();
            
            std::ofstream outFile(outputPath, std::ios::binary);
            outFile.write(imageData.data(), imageData.size());
            
            std::cout << "Extracted FLAC cover (" << pic->mimeType().toCString(true) 
                      << ") to " << outputPath << std::endl;
            return true;
        }
    }
    return false;
}

// Universal Router for Cover Extraction
void extractCoverArtUniversal(const std::string& filePath, const std::string& outputPath) {
    std::string ext = filePath.substr(filePath.find_last_of(".") + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    bool success = false;
    if (ext == "mp3") {
        success = extractMp3Cover(filePath, outputPath);
    } else if (ext == "flac") {
        success = extractFlacCover(filePath, outputPath);
    } else if (ext == "opus") {
        success = extractOpusCover(filePath, outputPath);
    } else if (ext == "ogg" || ext == "oga") {
        success = extractOggCover(filePath, outputPath);
    } else if (ext == "m4a" || ext == "mp4") {
        success = extractMp4Cover(filePath, outputPath);
    } else {
        std::cerr << "Unsupported format extension: " << ext << std::endl;
        return;
    }

    if (!success) {
        std::cout << "No embedded front cover found or file format issue." << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <path_to_audio_file>" << std::endl;
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::cout << "\n--- Cover Art Extraction ---" << std::endl;
    extractCoverArtUniversal(inputFile, "cover_output.png");
    
    return 0;
}
