#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int main()
{

    fs::path targetDir = "./public";

    if (!fs::exists(targetDir))
    {
        fs::create_directory(targetDir);
    }

    std::cout << "Contents of " << targetDir << ":\n";
    for (const auto &entry : fs::directory_iterator(targetDir))
    {
        if (entry.is_regular_file())
        {
            std::cout << "[File] " << entry.path().filename() << " (" << entry.file_size() << " bytes)\n";
        }
        else if (entry.is_directory())
        {
            std::cout << "[Dir] " << entry.path().filename() << "\n";
            for (const auto &entry2 : fs::directory_iterator(entry))
            {
                if (entry2.is_regular_file())
                {
                    std::cout << "[File] " << entry2.path() << " (" << entry2.file_size() << " bytes)\n";
                }
                else if (entry2.is_directory())
                {
                    std::cout << "[Dir] " << entry2.path().filename() << "\n";
                }
            }
        }
    }

    return 0;
}