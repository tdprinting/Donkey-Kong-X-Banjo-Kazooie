// Update checks against this repository's GitHub Releases.
//
// This part is pure logic: it parses the GitHub API's "latest release" JSON, compares
// versions and picks the download for the current platform. Fetching and installing live
// in the host (desktop) and the Android app, which use their own HTTP stacks.
//
// Release assets must follow this naming convention, which .github/workflows/release.yml
// produces:
//   JiggyJungle-<version>-windows-x64.zip
//   JiggyJungle-<version>-linux-x64.tar.gz
//   JiggyJungle-<version>-linux-arm64.tar.gz
//   JiggyJungle-<version>-macos.zip
//   JiggyJungle-<version>-android-arm64.apk
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jiggy::updater {

inline constexpr const char* kLatestReleaseUrl =
    "https://api.github.com/repos/tdprinting/Donkey-Kong-X-Banjo-Kazooie/releases/latest";

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease; // "beta.1" in 1.2.3-beta.1; empty for a full release.

    // Accepts "1.2.3", "v1.2.3", "1.2" and "1.2.3-beta.1". Build metadata (+...) is ignored.
    static std::optional<Version> parse(std::string_view text);
    std::string to_string() const;

    // Semantic-versioning order: 1.2.3-beta < 1.2.3 < 1.2.4.
    friend int compare(const Version& a, const Version& b);
    friend bool operator<(const Version& a, const Version& b) { return compare(a, b) < 0; }
    friend bool operator==(const Version& a, const Version& b) { return compare(a, b) == 0; }
};

enum class Platform {
    WindowsX64,
    LinuxX64,
    LinuxArm64,
    MacOS,
    AndroidArm64,
    Unknown,
};

Platform current_platform();
const char* platform_suffix(Platform platform);

struct ReleaseAsset {
    std::string name;
    std::string download_url;
    uint64_t size = 0;
};

struct Release {
    std::string tag;
    Version version;
    std::string name;
    std::string notes;
    std::string page_url;
    bool prerelease = false;
    bool draft = false;
    std::vector<ReleaseAsset> assets;
};

std::optional<Release> parse_release(std::string_view json, std::string* error);

// The asset built for `platform`, matched by the naming convention above.
std::optional<ReleaseAsset> pick_asset(const Release& release, Platform platform);

struct UpdateInfo {
    Release release;
    ReleaseAsset asset;
};

// Returns an update only when the release is newer than `current`, isn't a draft or
// prerelease, and has a download for `platform`.
std::optional<UpdateInfo> find_update(std::string_view latest_release_json, const Version& current, Platform platform,
                                      std::string* error);

} // namespace jiggy::updater
