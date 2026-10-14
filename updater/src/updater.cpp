#include "jiggy/updater.hpp"

#include <charconv>

#include "json/json.hpp"

namespace jiggy::updater {

namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

bool parse_number(std::string_view text, int& out) {
    if (text.empty()) {
        return false;
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && out >= 0;
}

// Compares dot-separated prerelease identifiers per semver: numeric ones numerically,
// numeric below alphanumeric, and a shorter list first when one is a prefix of the other.
int compare_prerelease(std::string_view a, std::string_view b) {
    while (true) {
        const size_t a_dot = a.find('.');
        const size_t b_dot = b.find('.');
        const std::string_view a_part = a.substr(0, a_dot);
        const std::string_view b_part = b.substr(0, b_dot);
        int a_num = 0, b_num = 0;
        const bool a_is_num = parse_number(a_part, a_num);
        const bool b_is_num = parse_number(b_part, b_num);
        int result = 0;
        if (a_is_num && b_is_num) {
            result = (a_num > b_num) - (a_num < b_num);
        } else if (a_is_num != b_is_num) {
            result = a_is_num ? -1 : 1;
        } else {
            result = a_part.compare(b_part);
            result = (result > 0) - (result < 0);
        }
        if (result != 0) {
            return result;
        }
        if (a_dot == std::string_view::npos || b_dot == std::string_view::npos) {
            return (a_dot != std::string_view::npos) - (b_dot != std::string_view::npos);
        }
        a.remove_prefix(a_dot + 1);
        b.remove_prefix(b_dot + 1);
    }
}

} // namespace

std::optional<Version> Version::parse(std::string_view text) {
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) {
        text.remove_prefix(1);
    }
    if (const size_t plus = text.find('+'); plus != std::string_view::npos) {
        text = text.substr(0, plus);
    }
    Version v;
    if (const size_t dash = text.find('-'); dash != std::string_view::npos) {
        v.prerelease = std::string(text.substr(dash + 1));
        text = text.substr(0, dash);
        if (v.prerelease.empty()) {
            return std::nullopt;
        }
    }
    int* parts[] = {&v.major, &v.minor, &v.patch};
    size_t count = 0;
    while (true) {
        const size_t dot = text.find('.');
        if (count == 3 || !parse_number(text.substr(0, dot), *parts[count])) {
            return std::nullopt;
        }
        count++;
        if (dot == std::string_view::npos) {
            break;
        }
        text.remove_prefix(dot + 1);
    }
    if (count < 2) {
        return std::nullopt;
    }
    return v;
}

std::string Version::to_string() const {
    std::string out = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) {
        out += "-" + prerelease;
    }
    return out;
}

int compare(const Version& a, const Version& b) {
    if (a.major != b.major) return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor) return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch) return a.patch < b.patch ? -1 : 1;
    if (a.prerelease.empty() || b.prerelease.empty()) {
        return (b.prerelease.empty() && !a.prerelease.empty()) ? -1 : (a.prerelease.empty() && !b.prerelease.empty());
    }
    return compare_prerelease(a.prerelease, b.prerelease);
}

Platform current_platform() {
#if defined(__ANDROID__)
    return Platform::AndroidArm64;
#elif defined(_WIN32)
    return Platform::WindowsX64;
#elif defined(__APPLE__)
    return Platform::MacOS;
#elif defined(__linux__) && (defined(__aarch64__) || defined(_M_ARM64))
    return Platform::LinuxArm64;
#elif defined(__linux__)
    return Platform::LinuxX64;
#else
    return Platform::Unknown;
#endif
}

const char* platform_suffix(Platform platform) {
    switch (platform) {
        case Platform::WindowsX64: return "-windows-x64.zip";
        case Platform::LinuxX64: return "-linux-x64.tar.gz";
        case Platform::LinuxArm64: return "-linux-arm64.tar.gz";
        case Platform::MacOS: return "-macos.zip";
        case Platform::AndroidArm64: return "-android-arm64.apk";
        case Platform::Unknown: break;
    }
    return nullptr;
}

std::optional<Release> parse_release(std::string_view json_text, std::string* error) {
    const nlohmann::json json = nlohmann::json::parse(json_text, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        set_error(error, "release response is not valid JSON");
        return std::nullopt;
    }
    if (!json.contains("tag_name") || !json["tag_name"].is_string()) {
        // GitHub answers {"message": "Not Found"} when the repo has no releases yet.
        const std::string message = json.value("message", std::string("no tag_name"));
        set_error(error, "no release found (" + message + ")");
        return std::nullopt;
    }

    Release release;
    release.tag = json["tag_name"].get<std::string>();
    auto version = Version::parse(release.tag);
    if (!version) {
        set_error(error, "release tag '" + release.tag + "' is not a version number");
        return std::nullopt;
    }
    release.version = *version;
    auto string_field = [&](const char* key) {
        return json.contains(key) && json[key].is_string() ? json[key].get<std::string>() : std::string();
    };
    release.name = string_field("name");
    release.notes = string_field("body");
    release.page_url = string_field("html_url");
    release.prerelease = json.value("prerelease", false);
    release.draft = json.value("draft", false);

    if (json.contains("assets") && json["assets"].is_array()) {
        for (const auto& item : json["assets"]) {
            if (!item.is_object() || !item.contains("name") || !item.contains("browser_download_url") ||
                !item["name"].is_string() || !item["browser_download_url"].is_string()) {
                continue;
            }
            ReleaseAsset asset;
            asset.name = item["name"].get<std::string>();
            asset.download_url = item["browser_download_url"].get<std::string>();
            if (item.contains("size") && item["size"].is_number_unsigned()) {
                asset.size = item["size"].get<uint64_t>();
            }
            release.assets.push_back(std::move(asset));
        }
    }
    return release;
}

std::optional<ReleaseAsset> pick_asset(const Release& release, Platform platform) {
    const char* suffix = platform_suffix(platform);
    if (!suffix) {
        return std::nullopt;
    }
    const std::string_view wanted{suffix};
    for (const auto& asset : release.assets) {
        const std::string_view name{asset.name};
        if (name.starts_with("JiggyJungle-") && name.ends_with(wanted)) {
            return asset;
        }
    }
    return std::nullopt;
}

std::optional<UpdateInfo> find_update(std::string_view latest_release_json, const Version& current, Platform platform,
                                      std::string* error) {
    auto release = parse_release(latest_release_json, error);
    if (!release) {
        return std::nullopt;
    }
    if (release->draft || release->prerelease || !(current < release->version)) {
        set_error(error, "");
        return std::nullopt;
    }
    auto asset = pick_asset(*release, platform);
    if (!asset) {
        set_error(error, "release " + release->tag + " has no download for this platform");
        return std::nullopt;
    }
    return UpdateInfo{std::move(*release), std::move(*asset)};
}

} // namespace jiggy::updater
