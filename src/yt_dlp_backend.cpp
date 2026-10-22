#include "yt_dlp_backend.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
  // Safely quote a string as one POSIX shell argument.
  std::string shellQuote(const std::string &value)
  {
    std::string result = "'";

    for (char c : value)
    {
      if (c == '\'')
        result += "'\\''";
      else
        result += c;
    }

    result += "'";
    return result;
  }

  std::string getString(
      const json &object,
      const char *key)
  {
    if (!object.is_object())
      return {};

    const auto it = object.find(key);

    if (it == object.end() || !it->is_string())
      return {};

    return it->get<std::string>();
  }

  std::string getThumbnail(const json &entry)
  {
    if (entry.contains("thumbnails") &&
        entry["thumbnails"].is_array())
    {
      const auto &thumbnails = entry["thumbnails"];

      // Prefer the last available thumbnail URL.
      for (auto it = thumbnails.rbegin();
           it != thumbnails.rend();
           ++it)
      {
        const std::string url = getString(*it, "url");

        if (!url.empty())
          return url;
      }
    }

    return getString(entry, "thumbnail");
  }

  // Returns the source page URL used by yt-dlp to obtain audio.
  // This is not a direct audio-stream URL.
  std::string getAudioUrl(const json &entry)
  {
    std::string url = getString(entry, "webpage_url");
    if (!url.empty())
      return url;
    const std::string id = getString(entry, "id");
    if (!id.empty())
      return "https://www.youtube.com/watch?v=" + id;
    url = getString(entry, "url");
    if (url.rfind("https://", 0) == 0 ||
        url.rfind("http://", 0) == 0)
    {
      return url;
    }
    return {};
  }
}

std::string YT_DLP::exec(const std::string &cmd)
{
  if (cmd.empty())
    throw std::invalid_argument("Command cannot be empty");
  using PipePtr = std::unique_ptr<FILE, int (*)(FILE *)>;
  PipePtr pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe)
    throw std::runtime_error("popen() failed");
  std::string output;
  std::array<char, 8192> buffer{};
  while (true)
  {
    const std::size_t bytesRead =
        fread(buffer.data(), 1, buffer.size(), pipe.get());
    if (bytesRead > 0)
      output.append(buffer.data(), bytesRead);
    if (bytesRead < buffer.size())
    {
      if (feof(pipe.get()))
        break;
      if (ferror(pipe.get()))
      {
        FILE *rawPipe = pipe.release();
        (void)pclose(rawPipe);

        throw std::runtime_error(
            "Failed to read command output");
      }
    }
  }
  FILE *rawPipe = pipe.release();
  const int status = pclose(rawPipe);
  if (status == -1)
    throw std::runtime_error("pclose() failed");
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
  {
    const std::string exitStatus =
        WIFEXITED(status)
            ? std::to_string(WEXITSTATUS(status))
            : "terminated by signal";
    throw std::runtime_error(
        "yt-dlp command failed: " + exitStatus);
  }

  return output;
}

std::string YT_DLP::retrieveSongs(
    const std::string &query,
    int limit)
{
  if (query.empty())
    throw std::invalid_argument(
        "Search query cannot be empty");
  if (limit < 1 || limit > 50)
    throw std::invalid_argument(
        "Search limit must be between 1 and 50");
  const std::string searchTerm =
      "ytsearch" + std::to_string(limit) + ":" + query;
  const std::string command =
      "yt-dlp"
      " --no-warnings"
      " --ignore-errors"
      " --skip-download"
      " --flat-playlist"
      " --dump-single-json " +
      shellQuote(searchTerm);
  const std::string output = exec(command);
  if (output.empty())
    return json::array().dump();
  const json document = json::parse(output);
  json results = json::array();
  if (!document.contains("entries") ||
      !document["entries"].is_array())
  {
    return results.dump();
  }

  for (const auto &entry : document["entries"])
  {
    if (!entry.is_object())
      continue;
    const std::string title = getString(entry, "title");
    const std::string id = getString(entry, "id");
    const std::string url = getAudioUrl(entry);
    const std::string thumbnail = getThumbnail(entry);
    if (title.empty() || url.empty())
      continue;
    json song;
    song["title"] = title;
    song["url"] = url;
    song["thumbnail"] = thumbnail;
    if (!id.empty())
      song["id"] = id;

    results.push_back(std::move(song));
  }

  return results.dump();
}

std::string YT_DLP::getAudioStreamUrl(
    const std::string &videoId)
{
  if (videoId.size() != 11)
    throw std::invalid_argument("Invalid YouTube video ID");
  for (unsigned char c : videoId)
  {
    if (!std::isalnum(c) && c != '_' && c != '-')
      throw std::invalid_argument("Invalid YouTube video ID");
  }
  const std::string videoUrl =
      "https://www.youtube.com/watch?v=" + videoId;
  const std::string command =
      "yt-dlp"
      " --no-warnings"
      " --no-playlist"
      " --format bestaudio"
      " --get-url " +
      // " --cookies-from-browser firefox " +
      std::string(cookie_path) +
      " " +
      shellQuote(videoUrl);
  std::string output = exec(command);
  while (!output.empty() &&
         (output.back() == '\n' || output.back() == '\r'))
  {
    output.pop_back();
  }
  if (output.empty())
    throw std::runtime_error(
        "yt-dlp returned an empty audio stream URL");
  if (output.find('\n') != std::string::npos ||
      output.find('\r') != std::string::npos)
  {
    throw std::runtime_error(
        "yt-dlp returned multiple audio stream URLs");
  }
  if (output.rfind("https://", 0) != 0 &&
      output.rfind("http://", 0) != 0)
  {
    throw std::runtime_error(
        "yt-dlp returned an invalid audio stream URL");
  }
  return output;
}

std::optional<File> YT_DLP::downloadAudio(
    const std::string &videoId,
    const std::filesystem::path &musicDirectory,
    FileService &fileService)
{
  if (videoId.size() != 11)
    throw std::invalid_argument("Invalid YouTube video ID");
  for (unsigned char c : videoId)
  {
    if (!std::isalnum(c) && c != '_' && c != '-')
      throw std::invalid_argument("Invalid YouTube video ID");
  }
  std::filesystem::create_directories(musicDirectory);
  const auto outputPath = musicDirectory / (videoId + ".opus");
  const std::string videoUrl =
      "https://www.youtube.com/watch?v=" + videoId;
  const std::string outputTemplate =
      (musicDirectory / "%(title)s.%(ext)s").string();
  const std::string command =
      "yt-dlp"
      " --no-warnings"
      " --no-playlist"
      " --no-overwrites"
      " --format bestaudio"
      " --extract-audio"
      " --audio-format opus"
      " --audio-quality 0"
      " --embed-metadata" +
      std::string(cookie_path) +
      " " +
      // " --cookies-from-browser firefox "
      " --output " +
      shellQuote(outputTemplate) +
      " " + shellQuote(videoUrl);
  exec(command);
  if (!std::filesystem::is_regular_file(outputPath) ||
      std::filesystem::file_size(outputPath) == 0)
  {
    throw std::runtime_error(
        "Audio download completed without a valid output file");
  }
  return fileService.addDownloadedFile(outputPath);
}