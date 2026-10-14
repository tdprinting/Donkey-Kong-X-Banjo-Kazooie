#ifndef __JIGGY_HOST_H__
#define __JIGGY_HOST_H__

// Host side of the Jiggy Bridge: keeps the player's Banjo-Kazooie ROM loaded next to the
// DK64 ROM the runtime already manages, and serves its assets to the recompiled game.

#include <cstddef>
#include <filesystem>
#include <string>

namespace jiggy_host {
    // Loads the Banjo-Kazooie ROM stored in the app folder, if the player has picked one.
    void init(const std::filesystem::path& app_folder);

    // Validates a ROM the player picked and stores a big-endian copy in the app folder.
    // On failure, `error` holds a message suitable for showing to the player.
    bool select_banjo_rom(const std::filesystem::path& path, std::string& error);

    bool banjo_rom_loaded();
    size_t banjo_asset_count();
}

#endif
