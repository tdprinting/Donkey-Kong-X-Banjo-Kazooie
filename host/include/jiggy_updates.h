#ifndef __JIGGY_UPDATES_H__
#define __JIGGY_UPDATES_H__

// Self-updater: checks this repository's GitHub Releases on launch and, on Windows and
// Linux, downloads and installs a newer build over the current one. Elsewhere (Flatpak,
// macOS) it opens the release page instead. All work runs on a background thread; the
// launcher polls state() each frame.

#include <string>

namespace jiggy_host::updates {
    enum class State {
        Checking,
        UpToDate,
        CheckFailed,
        Available,
        Installing,
        Installed,
        InstallFailed,
    };

    // Deletes files a previous update replaced, then starts the release check.
    void start();
    // Downloads and installs the available update, or opens the release page where the
    // game can't update itself.
    void install_or_open();

    State state();
    std::string available_version();
    std::string last_error();
    // True when the game can install updates itself here.
    bool can_self_install();
}

#endif
