#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <map>

void printFilesRecursively(const std::filesystem::path &path, int count = 0)
{
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
        return;

    for (const auto &entry : std::filesystem::directory_iterator(path))
    {
        for (int i = 0; i < count; i++)
            std::cout << "\t";

        std::cout << entry.path().filename().string() << '\n';

        if (entry.is_directory())
        {
            printFilesRecursively(entry.path(), count + 1);
        }
    }
}

int main()
{
    std::filesystem::path curr_path = std::filesystem::current_path();
    std::filesystem::path path_pub = curr_path / "frontend" / "public" / "js" / "api.js";

    std::cout << "Full Path: " << path_pub << '\n';
    std::cout << "Filename:  " << path_pub.filename() << '\n';

    std::ifstream file(path_pub);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open file at " << path_pub << '\n';
        return 1;
    }

    std::string line;
    std::map<std::string, int> map;
while (std::getline(file, line))
{
    std::string word;

    for (char c : line)
    {
        if (c != ' ')
        {
            word += c;
        }
        else if (!word.empty())
        {
            map[word]++;
            word.clear();
        }
    }

    if (!word.empty())
    {
        map[word]++;
    }

    std::cout << line << '\n';
}


        for (auto &[key, value] : map)
        {
            std::cout << "Key: " << key << " Value: " << value << '\n';
        }

        file.close();
        return 0;
    }
