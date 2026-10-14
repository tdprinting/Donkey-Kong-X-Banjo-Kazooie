// Updater tests: version ordering and parsing of GitHub's "latest release" responses.
#include <cstdlib>
#include <string>

#include <filesystem>
#include <fstream>
#include <iterator>

#include "jiggy/install.hpp"
#include "jiggy/updater.hpp"
#include "miniz.h"
#include "test_framework.hpp"

using jiggy::updater::Platform;
using jiggy::updater::Version;

namespace {

Version v(const char* text) {
    auto parsed = Version::parse(text);
    if (!parsed) {
        std::fprintf(stderr, "  could not parse %s\n", text);
        std::abort();
    }
    return *parsed;
}

// Shaped like https://docs.github.com/en/rest/releases/releases#get-the-latest-release
std::string release_json(const char* tag, bool prerelease = false, bool draft = false) {
    return std::string(R"({
  "tag_name": ")") + tag + R"(",
  "name": "Jiggy Jungle )" + tag + R"(",
  "html_url": "https://github.com/tdprinting/Donkey-Kong-X-Banjo-Kazooie/releases/tag/)" + tag + R"(",
  "body": "Fixes and new things",
  "draft": )" + (draft ? "true" : "false") + R"(,
  "prerelease": )" + (prerelease ? "true" : "false") + R"(,
  "assets": [
    {"name": "JiggyJungle-0.2.0-windows-x64.zip", "size": 1000,
     "browser_download_url": "https://example.invalid/win.zip"},
    {"name": "JiggyJungle-0.2.0-linux-x64.tar.gz", "size": 2000,
     "browser_download_url": "https://example.invalid/linux.tar.gz"},
    {"name": "JiggyJungle-0.2.0-android-arm64.apk", "size": 3000,
     "browser_download_url": "https://example.invalid/app.apk"},
    {"name": "checksums.txt", "size": 10, "browser_download_url": "https://example.invalid/sums"}
  ]
})";
}

} // namespace

TEST(version_parsing) {
    CHECK(v("1.2.3") == (Version{1, 2, 3, ""}));
    CHECK(v("v0.4.0").minor == 4);
    CHECK(v("1.2").patch == 0);
    CHECK(v("1.2.3-beta.1").prerelease == "beta.1");
    CHECK(v("1.2.3+build.5") == v("1.2.3"));
    CHECK(v("2.0.0-rc.1").to_string() == "2.0.0-rc.1");
    CHECK(!Version::parse("").has_value());
    CHECK(!Version::parse("one.two").has_value());
    CHECK(!Version::parse("1").has_value());
    CHECK(!Version::parse("1.2.3.4").has_value());
    CHECK(!Version::parse("1.2.3-").has_value());
    CHECK(!Version::parse("1.-2.3").has_value());
}

TEST(version_ordering) {
    CHECK(v("0.1.0") < v("0.2.0"));
    CHECK(v("0.9.9") < v("1.0.0"));
    CHECK(v("1.0.9") < v("1.0.10"));
    CHECK(v("1.0.0-beta") < v("1.0.0"));
    CHECK(!(v("1.0.0") < v("1.0.0-beta")));
    // Examples from semver.org section 11.
    CHECK(v("1.0.0-alpha") < v("1.0.0-alpha.1"));
    CHECK(v("1.0.0-alpha.1") < v("1.0.0-alpha.beta"));
    CHECK(v("1.0.0-alpha.beta") < v("1.0.0-beta"));
    CHECK(v("1.0.0-beta") < v("1.0.0-beta.2"));
    CHECK(v("1.0.0-beta.2") < v("1.0.0-beta.11"));
    CHECK(v("1.0.0-beta.11") < v("1.0.0-rc.1"));
    CHECK(v("1.0.0-rc.1") < v("1.0.0"));
}

TEST(parse_release_and_pick_assets) {
    std::string error;
    auto release = jiggy::updater::parse_release(release_json("v0.2.0"), &error);
    CHECK(release.has_value());
    if (!release) {
        return;
    }
    CHECK(release->version == v("0.2.0"));
    CHECK(release->assets.size() == 4);
    CHECK(release->notes == "Fixes and new things");

    auto win = jiggy::updater::pick_asset(*release, Platform::WindowsX64);
    auto apk = jiggy::updater::pick_asset(*release, Platform::AndroidArm64);
    CHECK(win && win->download_url == "https://example.invalid/win.zip" && win->size == 1000);
    CHECK(apk && apk->name == "JiggyJungle-0.2.0-android-arm64.apk");
    CHECK(!jiggy::updater::pick_asset(*release, Platform::MacOS).has_value());
    CHECK(!jiggy::updater::pick_asset(*release, Platform::Unknown).has_value());
}

TEST(find_update_decisions) {
    using jiggy::updater::find_update;
    std::string error;
    auto update = find_update(release_json("v0.2.0"), v("0.1.0"), Platform::LinuxX64, &error);
    CHECK(update && update->asset.download_url == "https://example.invalid/linux.tar.gz");

    CHECK(!find_update(release_json("v0.2.0"), v("0.2.0"), Platform::LinuxX64, &error)); // up to date
    CHECK(!find_update(release_json("v0.2.0"), v("0.3.0"), Platform::LinuxX64, &error)); // newer locally
    CHECK(!find_update(release_json("v0.2.0", true), v("0.1.0"), Platform::LinuxX64, &error));
    CHECK(!find_update(release_json("v0.2.0", false, true), v("0.1.0"), Platform::LinuxX64, &error));

    CHECK(!find_update(release_json("v0.2.0"), v("0.1.0"), Platform::MacOS, &error));
    CHECK(error.find("no download") != std::string::npos);
}

TEST(bad_responses) {
    std::string error;
    CHECK(!jiggy::updater::parse_release("not json", &error));
    CHECK(error.find("JSON") != std::string::npos);
    CHECK(!jiggy::updater::parse_release(R"({"message": "Not Found"})", &error));
    CHECK(error.find("Not Found") != std::string::npos);
    CHECK(!jiggy::updater::parse_release(R"({"tag_name": "latest"})", &error));
    CHECK(!jiggy::updater::parse_release("[]", &error));
    // Odd asset entries are skipped rather than failing the whole release.
    auto release = jiggy::updater::parse_release(R"({"tag_name": "1.0.0", "assets": [42, {"name": 1}]})", &error);
    CHECK(release && release->assets.empty());
}

namespace {

using Bytes = std::vector<uint8_t>;
namespace fs = std::filesystem;

Bytes bytes_of(const std::string& text) {
    return Bytes(text.begin(), text.end());
}

Bytes make_zip(const std::vector<std::pair<std::string, std::string>>& entries) {
    mz_zip_archive zip{};
    mz_zip_writer_init_heap(&zip, 0, 0);
    for (const auto& [name, content] : entries) {
        mz_zip_writer_add_mem(&zip, name.c_str(), content.data(), content.size(), MZ_DEFAULT_COMPRESSION);
    }
    void* data = nullptr;
    size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&zip, &data, &size);
    Bytes out(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
    mz_free(data); // finalize_heap_archive hands ownership of the buffer to us
    mz_zip_writer_end(&zip);
    return out;
}

void put_octal(Bytes& header, size_t at, size_t width, uint64_t value) {
    std::string text(width - 1, '0');
    for (size_t i = width - 1; i-- > 0 && value;) {
        text[i] = char('0' + (value & 7));
        value >>= 3;
    }
    std::copy(text.begin(), text.end(), header.begin() + long(at));
}

struct TarEntry {
    std::string name;
    std::string content;
    uint32_t mode = 0644;
    char type = '0';
};

Bytes make_tar_gz(const std::vector<TarEntry>& entries) {
    Bytes tar;
    for (const auto& e : entries) {
        Bytes header(512, 0);
        std::copy(e.name.begin(), e.name.end(), header.begin());
        put_octal(header, 100, 8, e.mode);
        put_octal(header, 124, 12, e.content.size());
        header[156] = uint8_t(e.type);
        std::copy_n("ustar", 5, header.begin() + 257);
        tar.insert(tar.end(), header.begin(), header.end());
        tar.insert(tar.end(), e.content.begin(), e.content.end());
        tar.resize((tar.size() + 511) / 512 * 512, 0);
    }
    tar.resize(tar.size() + 1024, 0);

    Bytes gz = {0x1F, 0x8B, 0x08, 0, 0, 0, 0, 0, 0, 3};
    size_t len = 0;
    void* body = tdefl_compress_mem_to_heap(tar.data(), tar.size(), &len, TDEFL_DEFAULT_MAX_PROBES);
    gz.insert(gz.end(), static_cast<uint8_t*>(body), static_cast<uint8_t*>(body) + len);
    mz_free(body);
    const uint32_t crc = uint32_t(mz_crc32(MZ_CRC32_INIT, tar.data(), tar.size()));
    for (uint32_t v : {crc, uint32_t(tar.size())}) {
        for (int i = 0; i < 4; i++) {
            gz.push_back(uint8_t(v >> (8 * i)));
        }
    }
    return gz;
}

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

fs::path fresh_dir(const char* name) {
    const fs::path dir = fs::temp_directory_path() / ("jiggy_updater_test_" + std::string(name));
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

} // namespace

TEST(safe_paths) {
    using jiggy::updater::is_safe_relative_path;
    CHECK(is_safe_relative_path("DK64Recompiled"));
    CHECK(is_safe_relative_path("assets/fonts/a.ttf"));
    CHECK(is_safe_relative_path("a..b/c"));
    CHECK(!is_safe_relative_path(""));
    CHECK(!is_safe_relative_path("/etc/passwd"));
    CHECK(!is_safe_relative_path("../evil"));
    CHECK(!is_safe_relative_path("assets/../../evil"));
    CHECK(!is_safe_relative_path("assets\\..\\evil"));
    CHECK(!is_safe_relative_path("C:/Windows/evil"));
}

TEST(extract_zip_release) {
    const Bytes zip = make_zip({{"JiggyJungle.exe", "new exe"}, {"assets/theme.rcss", "body {}"}});
    std::string error;
    auto files = jiggy::updater::extract_archive(zip, "JiggyJungle-0.2.0-windows-x64.zip", &error);
    CHECK(files && files->size() == 2);
    if (files && files->size() == 2) {
        CHECK((*files)[0].path == "JiggyJungle.exe" && (*files)[0].data == bytes_of("new exe"));
        CHECK((*files)[1].path == "assets/theme.rcss");
    }
    const Bytes evil = make_zip({{"../outside.txt", "x"}});
    CHECK(!jiggy::updater::extract_archive(evil, "evil.zip", &error));
    CHECK(error.find("unsafe") != std::string::npos);
}

TEST(extract_tar_gz_release) {
    const Bytes tgz = make_tar_gz({
        {"./DK64Recompiled", "elf", 0755, '0'},
        {"assets/", "", 0755, '5'},
        {"assets/recomp.rcss", "css", 0644, '0'},
    });
    std::string error;
    auto files = jiggy::updater::extract_archive(tgz, "JiggyJungle-0.2.0-linux-x64.tar.gz", &error);
    CHECK(files && files->size() == 2);
    if (files && files->size() == 2) {
        CHECK((*files)[0].path == "DK64Recompiled" && (*files)[0].executable);
        CHECK((*files)[1].path == "assets/recomp.rcss" && !(*files)[1].executable);
        CHECK((*files)[1].data == bytes_of("css"));
    }
    CHECK(!jiggy::updater::extract_archive(tgz, "release.rar", &error));
    Bytes truncated(tgz.begin(), tgz.begin() + 20);
    CHECK(!jiggy::updater::extract_archive(truncated, "x.tar.gz", &error));
}

TEST(install_replaces_and_cleans_up) {
    const fs::path dir = fresh_dir("install");
    write_text(dir / "game", "old game");
    write_text(dir / "notes.old", "player's own file");

    std::vector<jiggy::updater::ArchiveFile> files = {
        {"game", bytes_of("new game"), true},
        {"assets/new.txt", bytes_of("added"), false},
    };
    std::string error;
    CHECK(jiggy::updater::install_files(files, dir, &error));
    CHECK(read_text(dir / "game") == "new game");
    CHECK(read_text(dir / "assets" / "new.txt") == "added");
    CHECK(read_text(dir / "game.jiggy-old") == "old game");
#ifndef _WIN32
    CHECK((fs::status(dir / "game").permissions() & fs::perms::owner_exec) != fs::perms::none);
#endif

    jiggy::updater::remove_old_files(dir);
    CHECK(!fs::exists(dir / "game.jiggy-old"));
    CHECK(fs::exists(dir / "notes.old"));
    fs::remove_all(dir);
}

TEST(install_rolls_back_on_failure) {
    const fs::path dir = fresh_dir("rollback");
    write_text(dir / "game", "old game");
    // A directory where the second file should go makes its rename fail.
    fs::create_directories(dir / "blocked" / "inner");
    write_text(dir / "blocked" / "inner" / "keep", "x");

    std::vector<jiggy::updater::ArchiveFile> files = {
        {"game", bytes_of("new game"), false},
        {"fresh.txt", bytes_of("fresh"), false},
        {"blocked", bytes_of("file over a directory"), false},
    };
    std::string error;
    CHECK(!jiggy::updater::install_files(files, dir, &error));
    CHECK(read_text(dir / "game") == "old game");
    CHECK(!fs::exists(dir / "game.jiggy-old"));
    CHECK(!fs::exists(dir / "fresh.txt"));
    CHECK(fs::exists(dir / "blocked" / "inner" / "keep"));
    fs::remove_all(dir);
}

JIGGY_TEST_MAIN
