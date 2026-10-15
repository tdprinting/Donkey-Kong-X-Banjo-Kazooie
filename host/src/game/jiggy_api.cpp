// Banjo-Kazooie content for the DK64 engine.
//
// The player's Banjo-Kazooie ROM is validated with the Jiggy Bridge, stored next to the
// DK64 ROM in the app folder, and its asset bank is exposed to the recompiled game and to
// mods through the jiggy_* imports below (see patches/jiggy.h for the game-side view).

#include "jiggy_host.h"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <optional>
#include <utility>

#include "jiggy/bk_assets.hpp"
#include "jiggy/bk_gfx.hpp"
#include "jiggy/bk_model.hpp"
#include "jiggy/bk_sprite.hpp"
#include "jiggy/rom.hpp"
#include "librecomp/helpers.hpp"
#include "recomp.h"

namespace {
    constexpr const char* stored_banjo_filename = "jiggy_banjo.z64";

    std::mutex banjo_mutex;
    std::filesystem::path stored_banjo_path;
    std::optional<jiggy::Rom> banjo_rom;
    std::optional<jiggy::BkAssetTable> banjo_assets;
    // The last sprite asked for, so drawing a frame at a time doesn't re-inflate it.
    std::optional<std::pair<uint32_t, jiggy::BkSprite>> cached_sprite;

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
        cached_sprite.reset();
        return true;
    }

    // Expects banjo_mutex to be held.
    const jiggy::BkSprite* load_sprite(uint32_t id) {
        if (cached_sprite && cached_sprite->first == id) {
            return &cached_sprite->second;
        }
        if (!banjo_assets) {
            return nullptr;
        }
        std::string error;
        auto data = banjo_assets->load(banjo_rom->data(), id, &error);
        std::optional<jiggy::BkSprite> sprite;
        if (data) {
            sprite = jiggy::BkSprite::parse(*data, &error);
        }
        if (!sprite) {
            fprintf(stderr, "[Jiggy] Banjo-Kazooie asset 0x%X is not a usable sprite: %s\n", id, error.c_str());
            return nullptr;
        }
        cached_sprite.emplace(id, std::move(*sprite));
        return &cached_sprite->second;
    }

    // Frame as RGBA5551 rows, width padded to a multiple of 4 texels so every row is 8-byte
    // aligned for the RDP's texture loads. Expects banjo_mutex to be held.
    struct Rgba16Frame {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<uint8_t> texels;
    };

    std::optional<Rgba16Frame> sprite_frame_rgba16(uint32_t id, uint32_t frame) {
        const jiggy::BkSprite* sprite = load_sprite(id);
        if (!sprite) {
            return std::nullopt;
        }
        std::string error;
        auto image = sprite->decode_frame(frame, &error);
        if (!image) {
            fprintf(stderr, "[Jiggy] Banjo-Kazooie sprite 0x%X frame %u: %s\n", id, frame, error.c_str());
            return std::nullopt;
        }
        Rgba16Frame out;
        out.width = (uint32_t(image->width) + 3) & ~3u;
        out.height = image->height;
        const auto packed = jiggy::rgba8888_to_rgba5551(image->rgba);
        out.texels.assign(size_t(out.width) * out.height * 2, 0);
        for (uint32_t y = 0; y < out.height; y++) {
            std::copy_n(packed.begin() + ptrdiff_t(size_t(y) * image->width * 2), size_t(image->width) * 2,
                        out.texels.begin() + ptrdiff_t(size_t(y) * out.width * 2));
        }
        return out;
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

// s32 jiggy_bk_sprite_frame_count(u32 asset_id): frames in a sprite asset, or -1.
extern "C" void jiggy_bk_sprite_frame_count(uint8_t* rdram, recomp_context* ctx) {
    u32 id = _arg<0, u32>(rdram, ctx);

    std::lock_guard lock{banjo_mutex};
    const jiggy::BkSprite* sprite = load_sprite(id);
    _return<s32>(ctx, sprite ? s32(sprite->frame_count()) : -1);
}

// s32 jiggy_bk_sprite_frame_rgba16(u32 asset_id, u32 frame, void* dst, u32 dst_len):
// decodes one sprite frame to RGBA16 texels (width padded to a multiple of 4). Returns
// (width << 16) | height. With dst NULL or too small nothing is written, so a NULL call asks
// for the size. Returns -1 if the asset isn't a sprite or has no such frame.
extern "C" void jiggy_bk_sprite_frame_rgba16(uint8_t* rdram, recomp_context* ctx) {
    u32 id = _arg<0, u32>(rdram, ctx);
    u32 frame = _arg<1, u32>(rdram, ctx);
    PTR(u8) dst = _arg<2, PTR(u8)>(rdram, ctx);
    u32 dst_len = _arg<3, u32>(rdram, ctx);

    std::lock_guard lock{banjo_mutex};
    auto image = sprite_frame_rgba16(id, frame);
    if (!image || image->width > 0x7FFF || image->height > 0xFFFF) {
        _return<s32>(ctx, -1);
        return;
    }
    if (dst != NULLPTR && image->texels.size() <= dst_len) {
        // RDRAM is stored with 32-bit words byteswapped, so copy through MEM_B.
        for (size_t i = 0; i < image->texels.size(); i++) {
            MEM_B(i, dst) = static_cast<s8>(image->texels[i]);
        }
    }
    _return<s32>(ctx, s32((image->width << 16) | image->height));
}

// s32 jiggy_bk_model_build(u32 asset_id, void* dst, u32 dst_len): converts a Banjo-Kazooie
// model to a block DK64 can draw (bridge/include/jiggy/bk_gfx.hpp), built to live at dst.
// The display list is at the start of the block. Returns the block's size, writing it only
// when dst is 8-byte aligned and big enough (so a NULL dst asks for the size), or -1 if the
// asset isn't a usable model.
extern "C" void jiggy_bk_model_build(uint8_t* rdram, recomp_context* ctx) {
    u32 id = _arg<0, u32>(rdram, ctx);
    PTR(u8) dst = _arg<1, PTR(u8)>(rdram, ctx);
    u32 dst_len = _arg<2, u32>(rdram, ctx);

    auto data = load_asset(id);
    if (!data) {
        _return<s32>(ctx, -1);
        return;
    }
    std::string error;
    auto model = jiggy::BkModel::parse(*data, &error);
    // The size doesn't depend on the address, so a placeholder one serves size queries.
    const uint32_t base = dst != NULLPTR ? uint32_t(dst) : 0x80000000u;
    std::optional<jiggy::BkGfxBlock> block;
    if (model) {
        block = jiggy::bk_model_to_f3dex2(*model, base, &error);
    }
    if (!block) {
        fprintf(stderr, "[Jiggy] Banjo-Kazooie asset 0x%X is not a usable model: %s\n", id, error.c_str());
        _return<s32>(ctx, -1);
        return;
    }
    if (dst != NULLPTR && block->bytes.size() <= dst_len) {
        // RDRAM is stored with 32-bit words byteswapped, so copy through MEM_B.
        for (size_t i = 0; i < block->bytes.size(); i++) {
            MEM_B(i, dst) = static_cast<s8>(block->bytes[i]);
        }
    }
    _return<s32>(ctx, s32(block->bytes.size()));
}
