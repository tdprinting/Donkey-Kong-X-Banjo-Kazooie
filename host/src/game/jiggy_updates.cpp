#include "jiggy_updates.h"

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>

#include <curl/curl.h>

#include "jiggy/install.hpp"
#include "jiggy/updater.hpp"
#include "jiggy_version.h"

#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

namespace updater = jiggy::updater;

namespace {
    std::atomic<jiggy_host::updates::State> current_state{jiggy_host::updates::State::Checking};
    std::mutex info_mutex;
    std::optional<updater::UpdateInfo> pending_update;
    std::string error_message;

    void set_error(std::string message) {
        std::lock_guard lock{info_mutex};
        error_message = std::move(message);
    }

    size_t append_bytes(void* contents, size_t size, size_t nmemb, void* userdata) {
        auto* out = static_cast<std::vector<uint8_t>*>(userdata);
        const auto* bytes = static_cast<const uint8_t*>(contents);
        out->insert(out->end(), bytes, bytes + size * nmemb);
        return size * nmemb;
    }

#ifndef JIGGY_UPDATE_TOKEN
#define JIGGY_UPDATE_TOKEN ""
#endif

    // The token for the private repository: JIGGY_GITHUB_TOKEN from the environment when set
    // (handy for testing), otherwise the one release.yml built in.
    std::string update_token() {
        const char* env = getenv("JIGGY_GITHUB_TOKEN");
        return env != nullptr && env[0] != '\0' ? std::string(env) : std::string(JIGGY_UPDATE_TOKEN);
    }

    // Plain blocking GET. GitHub's API requires a User-Agent, and release downloads
    // redirect to a CDN, so redirects are followed. curl only sends the Authorization
    // header to the host it was set for, so the token never reaches the CDN.
    std::optional<std::vector<uint8_t>> http_get(const std::string& url, bool api, std::string& error) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            error = "could not start a network request";
            return std::nullopt;
        }
        std::vector<uint8_t> body;
        curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, api ? "Accept: application/vnd.github+json"
                                                 : "Accept: application/octet-stream");
        const std::string authorization = updater::authorization_header(update_token());
        if (!authorization.empty()) {
            headers = curl_slist_append(headers, authorization.c_str());
        }
        const std::string user_agent = std::string("JiggyJungle/") + JIGGY_VERSION;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        // Big enough for a release download on a slow connection, small enough not to hang forever.
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, api ? 20L : 900L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_bytes);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

        const CURLcode result = curl_easy_perform(curl);
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (result != CURLE_OK) {
            error = curl_easy_strerror(result);
            return std::nullopt;
        }
        // 404 from the releases API just means nothing is published yet; let the parser say so.
        if (status != 200 && !(api && status == 404)) {
            error = "server answered HTTP " + std::to_string(status);
            return std::nullopt;
        }
        return body;
    }

    std::filesystem::path install_dir() {
        char* base = SDL_GetBasePath();
        if (!base) {
            return {};
        }
        std::filesystem::path dir{reinterpret_cast<const char8_t*>(base)};
        SDL_free(base);
        return dir;
    }

    void check_worker() {
        using jiggy_host::updates::State;
        std::string error;
        auto body = http_get(updater::kLatestReleaseUrl, true, error);
        if (!body) {
            set_error("Couldn't check for updates: " + error);
            current_state = State::CheckFailed;
            return;
        }
        const auto current = updater::Version::parse(JIGGY_VERSION);
        const std::string_view json{reinterpret_cast<const char*>(body->data()), body->size()};
        auto update = updater::find_update(json, *current, updater::current_platform(), &error);
        if (!update) {
            // An empty error means "already up to date"; anything else is worth logging.
            if (!error.empty()) {
                fprintf(stderr, "[Jiggy] Update check: %s\n", error.c_str());
            }
            current_state = State::UpToDate;
            return;
        }
        {
            std::lock_guard lock{info_mutex};
            pending_update = std::move(update);
        }
        current_state = State::Available;
    }

    void install_worker(updater::UpdateInfo update) {
        using jiggy_host::updates::State;
        std::string error;
        auto archive = http_get(update.asset.fetch_url(), false, error);
        if (!archive) {
            set_error("Couldn't download the update: " + error);
            current_state = State::InstallFailed;
            return;
        }
        if (update.asset.size != 0 && archive->size() != update.asset.size) {
            set_error("The update download was incomplete. Please try again.");
            current_state = State::InstallFailed;
            return;
        }
        auto files = updater::extract_archive(*archive, update.asset.name, &error);
        if (!files) {
            set_error("Couldn't unpack the update: " + error);
            current_state = State::InstallFailed;
            return;
        }
        if (!updater::install_files(*files, install_dir(), &error)) {
            set_error("Couldn't install the update: " + error + "\nYour current version was left as it was.");
            current_state = State::InstallFailed;
            return;
        }
        current_state = State::Installed;
    }
}

bool jiggy_host::updates::can_self_install() {
#if defined(RECOMP_FLATPAK) || defined(__APPLE__) || defined(__ANDROID__)
    // Flatpak installs are read-only, macOS app bundles are signed as a whole, and the
    // Android app installs APKs through the system installer.
    return false;
#else
    return !install_dir().empty();
#endif
}

void jiggy_host::updates::start() {
    if (can_self_install()) {
        updater::remove_old_files(install_dir());
    }
    current_state = State::Checking;
    // Reference-counted, so this is fine alongside the mod store's own curl use.
    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::thread(check_worker).detach();
}

void jiggy_host::updates::install_or_open() {
    if (current_state != State::Available && current_state != State::InstallFailed) {
        return;
    }
    std::optional<updater::UpdateInfo> update;
    {
        std::lock_guard lock{info_mutex};
        update = pending_update;
    }
    if (!update) {
        return;
    }
    if (!can_self_install()) {
        SDL_OpenURL(update->release.page_url.c_str());
        return;
    }
    current_state = State::Installing;
    std::thread(install_worker, std::move(*update)).detach();
}

jiggy_host::updates::State jiggy_host::updates::state() {
    return current_state;
}

std::string jiggy_host::updates::available_version() {
    std::lock_guard lock{info_mutex};
    return pending_update ? pending_update->release.version.to_string() : std::string();
}

std::string jiggy_host::updates::last_error() {
    std::lock_guard lock{info_mutex};
    return error_message;
}
