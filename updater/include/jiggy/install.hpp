// Installing a downloaded desktop release over the running copy.
//
// Release archives (.zip on Windows and macOS, .tar.gz on Linux) hold the files that sit
// next to the executable, with no top-level folder. Installing renames each file it
// replaces to "<name>.jiggy-old" first, which works even for the running executable on
// Windows, then moves the new file into place. The next launch deletes the leftovers.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy::updater {

struct ArchiveFile {
    std::string path; // Relative, '/'-separated.
    std::vector<uint8_t> data;
    bool executable = false;
};

// Rejects absolute paths and ".." components so an archive can't write outside the install.
bool is_safe_relative_path(const std::string& path);

std::optional<std::vector<ArchiveFile>> extract_archive(std::span<const uint8_t> archive, const std::string& file_name,
                                                        std::string* error);

// Writes `files` under `install_dir`. On failure, files already replaced are restored.
bool install_files(const std::vector<ArchiveFile>& files, const std::filesystem::path& install_dir,
                   std::string* error);

// Deletes the "*.jiggy-old" files a previous install left behind. Safe to call on every launch.
void remove_old_files(const std::filesystem::path& install_dir);

} // namespace jiggy::updater
