// Banjo-Kazooie content for the DK64 engine.
//
// The player's Banjo-Kazooie ROM is validated with the Jiggy Bridge, stored next to the
// DK64 ROM in the app folder, and its asset bank is exposed to the recompiled game and to
// mods through the jiggy_* imports below (see patches/jiggy.h for the game-side view).

#include "jiggy_host.h"

#include <fstream>
#include <mutex>
#include <optional>

#include "jiggy/bk_assets.hpp"
#include "jiggy/rom.hpp"
#include "librecomp/helpers.hpp"
#include "recomp.h"

namespace {
    constexpr const char* stored_banjo_filename = "jiggy_banjo.z64";

    std::mutex banjo_mutex;
    std::filesystem::path stored_banjo_path;
    std::optional<jiggy::Rom> banjo_rom;
    std::optional<jiggy::BkAssetTable> banjo_assets;

    // Expects banjo_mutex to be held.
    bool adopt_rom(jiggy::Rom rom, std::string& error) {
        if (rom.info().game != jiggy::Game::BanjoKazooie) {
            error = "This ROM is not Banjo-Kazooie.";
            return false;
        }
        if (!rom.info().supported) {
            error = "This ROM is Banjo-Kazooie, but the wrong version.\n"
                    "Jiggy Jungle requires the NTSC-U (US) 1.0 N64 version of the game.";
            return false;
        }
        auto assets = jiggy::BkAssetTable::parse(rom.data(), jiggy::BkAssetTable::kUs10RomOffset, &error);
        if (!assets) {
            return false;
        }
        banjo_rom = std::move(rom);
        banjo_assets = std::move(assets);
        return true;
    }

    std::optional<std::vector<uint8_t>> load_asset(uint32_t id) {
        std::lock_guard lock{banjo_mutex};
        if (!banjo_assets) {
            return std::nullopt;
        }
        std::string error;
        auto data = banjo_assets->load(banjo_rom->data(), id, &error);
        if (!data) {
            fprintf(stderr, "[Jiggy] %s\n", error.c_str());
        }
        return data;
    }
}

void jiggy_host::init(const std::filesystem::path& app_folder) {
    std::lock_guard lock{banjo_mutex};
    stored_banjo_path = app_folder / stored_banjo_filename;

    std::error_code ec;
    if (!std::filesystem::exists(stored_banjo_path, ec)) {
        return;
    }
    std::string error;
    auto rom = jiggy::Rom::load_file(stored_banjo_path.string(), &error);
    if (!rom || !adopt_rom(std::move(*rom), error)) {
        fprintf(stderr, "[Jiggy] Ignoring stored Banjo-Kazooie ROM: %s\n", error.c_str());
        banjo_rom.reset();
        banjo_assets.reset();
    }
}

bool jiggy_host::select_banjo_rom(const std::filesystem::path& path, std::string& error) {
    auto rom = jiggy::Rom::load_file(path.string(), &error);
    if (!rom) {
        error = "Failed to open ROM file.";
        return false;
    }

    std::lock_guard lock{banjo_mutex};
    if (!adopt_rom(std::move(*rom), error)) {
        return false;
    }

    std::ofstream out(stored_banjo_path, std::ios::binary);
    const auto data = banjo_rom->data();
    out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
    if (!out) {
        // Still usable this session; it just won't be remembered.
        fprintf(stderr, "[Jiggy] Could not store the Banjo-Kazooie ROM at %s\n", stored_banjo_path.string().c_str());
    }
    return true;
}

bool jiggy_host::banjo_rom_loaded() {
    std::lock_guard lock{banjo_mutex};
    return banjo_assets.has_value();
}

size_t jiggy_host::banjo_asset_count() {
    std::lock_guard lock{banjo_mutex};
    return banjo_assets ? banjo_assets->size() : 0;
}

// s32 jiggy_bk_available(void)
extern "C" void jiggy_bk_available(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, jiggy_host::banjo_rom_loaded() ? 1 : 0);
}

// s32 jiggy_bk_asset_size(u32 asset_id): decompressed size, or -1.
extern "C" void jiggy_bk_asset_size(uint8_t* rdram, recomp_context* ctx) {
    u32 id = _arg<0, u32>(rdram, ctx);

    std::lock_guard lock{banjo_mutex};
    std::optional<uint32_t> size;
    if (banjo_assets) {
        size = banjo_assets->uncompressed_size(banjo_rom->data(), id, nullptr);
    }
    _return<s32>(ctx, size ? s32(*size) : -1);
}

// s32 jiggy_bk_asset_read(u32 asset_id, void* dst, u32 dst_len): bytes copied, or -1 if the
// asset is missing or doesn't fit.
extern "C" void jiggy_bk_asset_read(uint8_t* rdram, recomp_context* ctx) {
    u32 id = _arg<0, u32>(rdram, ctx);
    PTR(u8) dst = _arg<1, PTR(u8)>(rdram, ctx);
    u32 dst_len = _arg<2, u32>(rdram, ctx);

    auto data = load_asset(id);
    if (!data || data->size() > dst_len) {
        _return<s32>(ctx, -1);
        return;
    }
    // RDRAM is stored with 32-bit words byteswapped, so copy through MEM_B.
    for (size_t i = 0; i < data->size(); i++) {
        MEM_B(i, dst) = static_cast<s8>((*data)[i]);
    }
    _return<s32>(ctx, s32(data->size()));
}
