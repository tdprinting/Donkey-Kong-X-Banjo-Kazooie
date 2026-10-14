#include "jiggy/rom_import.hpp"

#include <fstream>
#include <system_error>

namespace jiggy {

const char* stored_rom_filename(Game game) {
    switch (game) {
        case Game::BanjoKazooie:
            return "jiggy_banjo.z64";
        case Game::DonkeyKong64:
            return "DK64.z64"; // The name librecomp loads: game id + ".z64".
        case Game::Unknown:
            break;
    }
    return nullptr;
}

RomImportResult import_rom(std::vector<uint8_t> bytes, Game expected, const std::filesystem::path& dir,
                           bool require_known_dump) {
    RomImportResult result;
    const char* filename = stored_rom_filename(expected);
    if (!filename) {
        result.message = "Internal error: no game to import.";
        return result;
    }

    std::string error;
    auto rom = Rom::from_bytes(std::move(bytes), &error);
    if (!rom) {
        result.message = "That file isn't an N64 ROM (" + error + ").";
        return result;
    }
    const RomInfo& info = rom->info();
    if (info.game != expected) {
        result.message = std::string("That ROM isn't ") + game_name(expected) + ". ";
        result.message += info.game == Game::Unknown ? "It isn't either game Jiggy Jungle uses."
                                                     : std::string("It's ") + game_name(info.game) + ".";
        return result;
    }
    if (require_known_dump && !info.supported) {
        result.message = std::string("That's ") + game_name(expected) +
                         ", but not the version Jiggy Jungle needs. " +
                         (expected == Game::BanjoKazooie ? "Use the NTSC-U (US) N64 release, 1.0 or Rev 1."
                                                         : "Use the NTSC-U (US) 1.0 N64 release.");
        return result;
    }

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const auto final_path = dir / filename;
    auto temp_path = final_path;
    temp_path += ".new";
    {
        std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
        const auto data = rom->data();
        out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
        if (!out) {
            std::filesystem::remove(temp_path, ec);
            result.message = "Couldn't save the ROM in the app's storage. Is the device full?";
            return result;
        }
    }
    std::filesystem::rename(temp_path, final_path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        result.message = "Couldn't save the ROM in the app's storage.";
        return result;
    }
    result.ok = true;
    result.stored_path = final_path;
    result.message = std::string(game_name(expected)) + " ROM loaded.";
    return result;
}

} // namespace jiggy
