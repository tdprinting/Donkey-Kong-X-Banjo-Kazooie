#include "jiggy/install.hpp"

#include <fstream>
#include <system_error>

#include "jiggy/inflate.hpp"
#include "miniz.h"

namespace fs = std::filesystem;

namespace jiggy::updater {

namespace {

// Distinctive so cleanup never deletes a player's own *.old files.
constexpr const char* kOldSuffix = ".jiggy-old";

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

bool ends_with(const std::string& text, const char* suffix) {
    const std::string_view s{suffix};
    return text.size() >= s.size() && text.compare(text.size() - s.size(), s.size(), s) == 0;
}

uint64_t parse_octal(std::span<const uint8_t> field) {
    uint64_t value = 0;
    for (uint8_t c : field) {
        if (c == 0 || c == ' ') {
            if (value != 0) {
                break;
            }
            continue;
        }
        if (c < '0' || c > '7') {
            break;
        }
        value = value * 8 + (c - '0');
    }
    return value;
}

std::string tar_string(std::span<const uint8_t> field) {
    std::string out;
    for (uint8_t c : field) {
        if (c == 0) {
            break;
        }
        out.push_back(char(c));
    }
    return out;
}

// POSIX ustar, plus GNU long names ('L'), which GNU tar writes for paths over 100 bytes.
std::optional<std::vector<ArchiveFile>> extract_tar(std::span<const uint8_t> tar, std::string* error) {
    std::vector<ArchiveFile> files;
    std::string long_name;
    size_t pos = 0;
    while (pos + 512 <= tar.size()) {
        const auto header = tar.subspan(pos, 512);
        bool all_zero = true;
        for (uint8_t b : header) {
            all_zero &= (b == 0);
        }
        if (all_zero) {
            break;
        }
        const uint64_t size = parse_octal(header.subspan(124, 12));
        const uint8_t type = header[156];
        const size_t data_start = pos + 512;
        if (size > tar.size() - data_start) {
            set_error(error, "tar archive is truncated");
            return std::nullopt;
        }
        const auto data = tar.subspan(data_start, size_t(size));

        std::string name = tar_string(header.subspan(0, 100));
        if (std::string_view(reinterpret_cast<const char*>(header.data() + 257), 5) == "ustar") {
            const std::string prefix = tar_string(header.subspan(345, 155));
            if (!prefix.empty()) {
                name = prefix + "/" + name;
            }
        }
        if (!long_name.empty()) {
            name = long_name;
            long_name.clear();
        }

        if (type == 'L') {
            long_name = tar_string(data);
        } else if (type == '0' || type == 0) {
            while (name.starts_with("./")) {
                name.erase(0, 2);
            }
            ArchiveFile file;
            file.path = name;
            file.data.assign(data.begin(), data.end());
            file.executable = (parse_octal(header.subspan(100, 8)) & 0111) != 0;
            files.push_back(std::move(file));
        }
        // Directories ('5') are created as needed; links and other types are skipped.
        pos = data_start + ((size + 511) / 512) * 512;
    }
    return files;
}

std::optional<std::vector<ArchiveFile>> extract_zip(std::span<const uint8_t> zip, std::string* error) {
    mz_zip_archive archive{};
    if (!mz_zip_reader_init_mem(&archive, zip.data(), zip.size(), 0)) {
        set_error(error, "zip archive is unreadable");
        return std::nullopt;
    }
    std::vector<ArchiveFile> files;
    const mz_uint count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint i = 0; i < count; i++) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, i, &stat)) {
            mz_zip_reader_end(&archive);
            set_error(error, "zip archive has a corrupt entry");
            return std::nullopt;
        }
        if (stat.m_is_directory) {
            continue;
        }
        size_t size = 0;
        void* data = mz_zip_reader_extract_to_heap(&archive, i, &size, 0);
        if (!data) {
            mz_zip_reader_end(&archive);
            set_error(error, std::string("could not extract ") + stat.m_filename);
            return std::nullopt;
        }
        ArchiveFile file;
        file.path = stat.m_filename;
        file.data.assign(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
        // Unix permission bits live in the high half of the external attributes.
        file.executable = ((stat.m_external_attr >> 16) & 0111) != 0;
        mz_free(data);
        files.push_back(std::move(file));
    }
    mz_zip_reader_end(&archive);
    return files;
}

fs::path old_path_for(const fs::path& target) {
    fs::path old = target;
    old += kOldSuffix;
    return old;
}

} // namespace

bool is_safe_relative_path(const std::string& path) {
    if (path.empty() || path.front() == '/' || path.front() == '\\' || path.find(':') != std::string::npos) {
        return false;
    }
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find_first_of("/\\", start);
        if (end == std::string::npos) {
            end = path.size();
        }
        const std::string_view part(path.data() + start, end - start);
        if (part == "..") {
            return false;
        }
        start = end + 1;
    }
    return true;
}

std::optional<std::vector<ArchiveFile>> extract_archive(std::span<const uint8_t> archive, const std::string& file_name,
                                                        std::string* error) {
    std::optional<std::vector<ArchiveFile>> files;
    if (ends_with(file_name, ".zip")) {
        files = extract_zip(archive, error);
    } else if (ends_with(file_name, ".tar.gz") || ends_with(file_name, ".tgz")) {
        std::string inflate_error;
        auto tar = inflate_gzip(archive, &inflate_error);
        if (!tar) {
            set_error(error, "could not decompress " + file_name + ": " + inflate_error);
            return std::nullopt;
        }
        files = extract_tar(*tar, error);
    } else {
        set_error(error, "don't know how to unpack " + file_name);
        return std::nullopt;
    }
    if (!files) {
        return std::nullopt;
    }
    if (files->empty()) {
        set_error(error, file_name + " is empty");
        return std::nullopt;
    }
    for (const auto& file : *files) {
        if (!is_safe_relative_path(file.path)) {
            set_error(error, file_name + " contains an unsafe path: " + file.path);
            return std::nullopt;
        }
    }
    return files;
}

bool install_files(const std::vector<ArchiveFile>& files, const fs::path& install_dir, std::string* error) {
    std::error_code ec;
    std::vector<fs::path> replaced; // targets whose original now lives at target.old
    std::vector<fs::path> created;  // targets that didn't exist before

    auto roll_back = [&]() {
        std::error_code ignored;
        for (const auto& target : created) {
            fs::remove(target, ignored);
        }
        for (const auto& target : replaced) {
            fs::remove(target, ignored);
            fs::rename(old_path_for(target), target, ignored);
        }
    };

    for (const auto& file : files) {
        const fs::path target = install_dir / fs::path(file.path).make_preferred();
        if (fs::is_directory(target, ec)) {
            roll_back();
            set_error(error, "a folder is in the way of " + target.string());
            return false;
        }
        fs::path staged = target;
        staged += ".new";

        fs::create_directories(target.parent_path(), ec);
        {
            std::ofstream out(staged, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(file.data.data()), std::streamsize(file.data.size()));
            if (!out) {
                fs::remove(staged, ec);
                roll_back();
                set_error(error, "could not write " + staged.string());
                return false;
            }
        }
        if (file.executable) {
            fs::permissions(staged,
                            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                            fs::perm_options::add, ec);
        }

        const bool existed = fs::exists(target, ec);
        if (existed) {
            fs::remove(old_path_for(target), ec);
            fs::rename(target, old_path_for(target), ec);
            if (ec) {
                fs::remove(staged, ec);
                roll_back();
                set_error(error, "could not move aside " + target.string());
                return false;
            }
            replaced.push_back(target);
        }
        fs::rename(staged, target, ec);
        if (ec) {
            fs::remove(staged, ec);
            roll_back();
            set_error(error, "could not install " + target.string());
            return false;
        }
        if (!existed) {
            created.push_back(target);
        }
    }
    return true;
}

void remove_old_files(const fs::path& install_dir) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(install_dir, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == kOldSuffix) {
            std::error_code ignored;
            fs::remove(it->path(), ignored);
        }
    }
}

} // namespace jiggy::updater
