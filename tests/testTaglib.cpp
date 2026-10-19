#include <iostream>
#include <taglib/fileref.h>
#include <taglib/tag.h>

int main() {
    TagLib::FileRef file("Uyiril Thodum.flac");

    if (file.isNull()) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    TagLib::Tag *tag = file.tag();

    if (tag) {
        std::cout << "Title:  " << tag->title() << '\n';
        std::cout << "Artist: " << tag->artist() << '\n';
        std::cout << "Album:  " << tag->album() << '\n';
    }

    if (file.audioProperties()) {
        std::cout << "Length: "
                  << file.audioProperties()->length()
                  << " seconds\n";
    }

    return 0;
}