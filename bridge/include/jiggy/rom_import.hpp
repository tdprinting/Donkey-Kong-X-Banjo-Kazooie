// Taking in a ROM the player picked: check it's the right game and dump, then keep a
// big-endian copy in the app's own storage. Shared by the desktop launcher and the
// Android app, which can only read the picked file once through a content URI.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "jiggy/rom.hpp"

namespace jiggy {

// File name the stored copy gets: "DK64.z64" (the name librecomp loads) or "jiggy_banjo.z64".
const char* stored_rom_filename(Game game);

struct RomImportResult {
    bool ok = false;
    std::string message;              // Shown to the player either way.
    std::filesystem::path stored_path; // Set when ok.
};

// `require_known_dump` is false only in tests: synthetic ROMs can't match the real SHA-1s.
RomImportResult import_rom(std::vector<uint8_t> bytes, Game expected, const std::filesystem::path& dir,
                           bool require_known_dump = true);

} // namespace jiggy
