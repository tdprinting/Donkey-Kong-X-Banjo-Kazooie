// Banjo-Kazooie content for the DK64 engine.
//
// The player's Banjo-Kazooie ROM is validated with the Jiggy Bridge, stored next to the
// DK64 ROM in the app folder, and its asset bank is exposed to the recompiled game and to
// mods through the jiggy_* imports below (see patches/jiggy.h for the game-side view).

#include "jiggy_host.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include "jiggy/bk_anim.hpp"
#include "jiggy/bk_assets.hpp"
#include "jiggy/bk_gfx.hpp"
#include "jiggy/bk_model.hpp"
#include "jiggy/bk_pose.hpp"
#include "jiggy/bk_sprite.hpp"
#include "jiggy/save_store.hpp"
#include "jiggy/rom.hpp"
#include "librecomp/helpers.hpp"
#include "recomp.h"

namespace {
    constexpr const char* stored_banjo_filename = "jiggy_banjo.z64";
    constexpr const char* save_filename = "jiggy_save.bin";

    // Jiggy Jungle's additions to the save files (jiggy/save_store.hpp), written through to
    // the app folder on every change.
    std::mutex save_mutex;
    std::filesystem::path save_path;
    jiggy::SaveStore save_store;

    // Expects save_mutex to be held. Writes a temporary file and renames it over the old one,
    // so a crash mid-write can't lose what was saved before.
    void write_save_store() {
        if (save_path.empty()) {
            return;
        }
        const auto bytes = save_store.serialize();
        std::filesystem::path temp = save_path;
        temp += ".tmp";
        {
            std::ofstream out{temp, std::ios::binary | std::ios::trunc};
            out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
            if (!out) {
                fprintf(stderr, "[Jiggy] Couldn't write %s\n", temp.string().c_str());
                return;
            }
        }
        std::error_code ec;
        std::filesystem::rename(temp, save_path, ec);
        if (ec) {
            fprintf(stderr, "[Jiggy] Couldn't replace %s: %s\n", save_path.string().c_str(), ec.message().c_str());
        }
    }

    void load_save_store(const std::filesystem::path& app_folder) {
        std::lock_guard lock{save_mutex};
        save_path = app_folder / save_filename;
        std::ifstream in{save_path, std::ios::binary};
        if (!in) {
            return;
        }
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        std::string error;
        auto store = jiggy::SaveStore::parse(bytes, &error);
        if (!store) {
            // Keep the unreadable file aside rather than writing over it.
            fprintf(stderr, "[Jiggy] Ignoring %s: %s\n", save_path.string().c_str(), error.c_str());
            std::error_code ec;
            std::filesystem::path aside = save_path;
            aside += ".bad";
            std::filesystem::rename(save_path, aside, ec);
            return;
        }
        save_store = std::move(*store);
    }

    std::mutex banjo_mutex;
    std::filesystem::path stored_banjo_path;
    std::optional<jiggy::Rom> banjo_rom;
    std::optional<jiggy::BkAssetTable> banjo_assets;
    std::atomic<uint32_t> banjo_generation{0};
    // The last sprite asked for, so drawing a frame at a time doesn't re-inflate it.
    std::optional<std::pair<uint32_t, jiggy::BkSprite>> cached_sprite;
    // Clears the HUD numbers made from the last ROM (defined with them below).
    void reset_bold_numbers();

    // Models built into N64 memory with jiggy_bk_model_build, by asset id: what posing them
    // needs. The block's bytes are dropped once copied.
    struct BuiltModel {
        uint32_t address = 0;
        jiggy::BkModel model;
        jiggy::BkSkeleton skeleton;
        jiggy::BkGfxBlock block;
    };
    std::mutex model_mutex;
    std::map<uint32_t, BuiltModel> built_models;
    std::map<uint32_t, std::optional<jiggy::BkAnimation>> animations;

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
        banjo_generation++;
        cached_sprite.reset();
        reset_bold_numbers();
        {
            std::lock_guard models_lock{model_mutex};
            built_models.clear();
            animations.clear();
        }
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

    Rgba16Frame to_rgba16(const jiggy::BkSpriteImage& image) {
        Rgba16Frame out;
        out.width = (uint32_t(image.width) + 3) & ~3u;
        out.height = image.height;
        const auto packed = jiggy::rgba8888_to_rgba5551(image.rgba);
        out.texels.assign(size_t(out.width) * out.height * 2, 0);
        for (uint32_t y = 0; y < out.height; y++) {
            std::copy_n(packed.begin() + ptrdiff_t(size_t(y) * image.width * 2), size_t(image.width) * 2,
                        out.texels.begin() + ptrdiff_t(size_t(y) * out.width * 2));
        }
        return out;
    }

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
        return to_rgba16(*image);
    }

    // Banjo-Kazooie's bold HUD font: the numbers' alpha mask colored by the orange gradient
    // most levels use (src/core2/print.c).
    constexpr uint32_t kBoldFontNumbers = 0x6ED;
    constexpr uint32_t kBoldFontOrangeGradient = 0x6EE;
    constexpr size_t kBoldFontNumberCount = 10;
    std::array<std::optional<Rgba16Frame>, kBoldFontNumberCount> bold_numbers;
    bool bold_numbers_loaded = false;

    void reset_bold_numbers() {
        bold_numbers = {};
        bold_numbers_loaded = false;
    }

    // Expects banjo_mutex to be held.
    const Rgba16Frame* bold_number_rgba16(uint32_t digit) {
        if (!bold_numbers_loaded) {
            bold_numbers_loaded = true;
            const jiggy::BkSprite* mask_sprite = load_sprite(kBoldFontNumbers);
            if (!mask_sprite) {
                return nullptr;
            }
            const jiggy::BkSprite mask = *mask_sprite; // load_sprite replaces its cached sprite.
            const jiggy::BkSprite* texture = load_sprite(kBoldFontOrangeGradient);
            if (!texture) {
                return nullptr;
            }
            for (size_t i = 0; i < kBoldFontNumberCount; i++) {
                std::string error;
                auto letter = jiggy::bk_bold_font_letter(mask, *texture, i, &error);
                if (!letter) {
                    fprintf(stderr, "[Jiggy] Banjo-Kazooie bold font number %zu: %s\n", i, error.c_str());
                    continue;
                }
                bold_numbers[i] = to_rgba16(*letter);
            }
        }
        if (digit >= kBoldFontNumberCount || !bold_numbers[digit]) {
            return nullptr;
        }
        return &*bold_numbers[digit];
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
    load_save_store(app_folder);
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

uint32_t jiggy_host::banjo_rom_generation() {
    return banjo_generation.load();
}

std::optional<std::vector<uint8_t>> jiggy_host::banjo_rom_bytes(uint32_t offset, uint32_t size) {
    std::lock_guard lock{banjo_mutex};
    if (!banjo_rom) {
        return std::nullopt;
    }
    const auto data = banjo_rom->data();
    if (offset > data.size() || size > data.size() - offset) {
        return std::nullopt;
    }
    return std::vector<uint8_t>(data.begin() + offset, data.begin() + offset + size);
}

std::optional<std::vector<uint8_t>> jiggy_host::banjo_asset(uint32_t id) {
    return load_asset(id);
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

// s32 jiggy_bk_bold_number_rgba16(u32 digit, void* dst, u32 dst_len): Banjo-Kazooie's HUD
// number `digit` (0-9) as RGBA16 texels, like jiggy_bk_sprite_frame_rgba16. Returns
// (width << 16) | height, or -1 without a Banjo-Kazooie ROM.
extern "C" void jiggy_bk_bold_number_rgba16(uint8_t* rdram, recomp_context* ctx) {
    u32 digit = _arg<0, u32>(rdram, ctx);
    PTR(u8) dst = _arg<1, PTR(u8)>(rdram, ctx);
    u32 dst_len = _arg<2, u32>(rdram, ctx);

    std::lock_guard lock{banjo_mutex};
    const Rgba16Frame* image = bold_number_rgba16(digit);
    if (!image || image->width > 0x7FFF || image->height > 0xFFFF) {
        _return<s32>(ctx, -1);
        return;
    }
    if (dst != NULLPTR && image->texels.size() <= dst_len) {
        for (size_t i = 0; i < image->texels.size(); i++) {
            MEM_B(i, dst) = static_cast<s8>(image->texels[i]);
        }
    }
    _return<s32>(ctx, s32((image->width << 16) | image->height));
}

// u32 jiggy_save_get(u32 file, u32 key): a value kept with DK64 save file `file`, 0 if unset.
extern "C" void jiggy_save_get(uint8_t* rdram, recomp_context* ctx) {
    u32 file = _arg<0, u32>(rdram, ctx);
    u32 key = _arg<1, u32>(rdram, ctx);

    std::lock_guard lock{save_mutex};
    _return<u32>(ctx, save_store.get(file, key));
}

// void jiggy_save_set(u32 file, u32 key, u32 value): keeps a value with a save file, written
// to disk straight away.
extern "C" void jiggy_save_set(uint8_t* rdram, recomp_context* ctx) {
    u32 file = _arg<0, u32>(rdram, ctx);
    u32 key = _arg<1, u32>(rdram, ctx);
    u32 value = _arg<2, u32>(rdram, ctx);

    std::lock_guard lock{save_mutex};
    if (save_store.set(file, key, value)) {
        write_save_store();
    }
}

// void jiggy_save_clear(u32 file): forgets a save file's values, when the game starts it afresh.
extern "C" void jiggy_save_clear(uint8_t* rdram, recomp_context* ctx) {
    u32 file = _arg<0, u32>(rdram, ctx);

    std::lock_guard lock{save_mutex};
    if (save_store.clear_file(file)) {
        write_save_store();
    }
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
    const size_t size = block->bytes.size();
    if (dst != NULLPTR && size > dst_len) {
        fprintf(stderr, "[Jiggy] Banjo-Kazooie model 0x%X needs %zu bytes, but its block holds %u\n", id, size,
                unsigned(dst_len));
    }
    if (dst != NULLPTR && size <= dst_len && (base & 7) == 0) {
        // RDRAM is stored with 32-bit words byteswapped, so copy through MEM_B.
        for (size_t i = 0; i < size; i++) {
            MEM_B(i, dst) = static_cast<s8>(block->bytes[i]);
        }
        auto skeleton = jiggy::BkSkeleton::parse(*data, &error);
        if (skeleton) {
            block->bytes.clear();
            block->bytes.shrink_to_fit();
            std::lock_guard lock{model_mutex};
            built_models[id] = BuiltModel{base, std::move(*model), std::move(*skeleton), std::move(*block)};
        } else {
            fprintf(stderr, "[Jiggy] Banjo-Kazooie model 0x%X has no usable skeleton: %s\n", id, error.c_str());
        }
    }
    _return<s32>(ctx, s32(size));
}

namespace {
    // JiggyBkPose (patches/jiggy.h), field offsets.
    constexpr uint32_t pose_model = 0x00;
    constexpr uint32_t pose_block = 0x04;
    constexpr uint32_t pose_animation = 0x08;
    constexpr uint32_t pose_progress = 0x0C;
    constexpr uint32_t pose_world = 0x10;
    constexpr uint32_t pose_selectors = 0x50;
    constexpr uint32_t pose_gfx = 0xF8;
    constexpr uint32_t pose_gfx_capacity = 0xFC;
    constexpr uint32_t pose_mtx = 0x100;
    constexpr uint32_t pose_mtx_capacity = 0x104;

    float read_f32(uint8_t* rdram, gpr base, uint32_t offset) {
        const uint32_t bits = uint32_t(MEM_W(offset, base));
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    // Reads and caches an animation, so later frames only look it up. Takes banjo_mutex (to
    // read the asset) and model_mutex, so callers must hold neither.
    void cache_animation(uint32_t id) {
        {
            std::lock_guard lock{model_mutex};
            if (animations.contains(id)) {
                return;
            }
        }
        std::optional<jiggy::BkAnimation> animation;
        std::string error = "not in the ROM";
        auto data = load_asset(id);
        if (data) {
            animation = jiggy::BkAnimation::parse(*data, &error);
        }
        if (!animation) {
            fprintf(stderr, "[Jiggy] Banjo-Kazooie asset 0x%X is not a usable animation: %s\n", id, error.c_str());
        }
        std::lock_guard lock{model_mutex};
        animations.emplace(id, std::move(animation));
    }
}

// s32 jiggy_bk_model_pose(JiggyBkPose* pose): writes a display list drawing a model built
// with jiggy_bk_model_build in an animation's pose (bridge/include/jiggy/bk_pose.hpp), and
// the bone matrices it loads. Returns the display list's command count, or -1 if the model
// wasn't built at that address or the buffers are too small.
extern "C" void jiggy_bk_model_pose(uint8_t* rdram, recomp_context* ctx) {
    const gpr pose = ctx->r4;
    const uint32_t id = uint32_t(MEM_W(pose_model, pose));
    const uint32_t block_address = uint32_t(MEM_W(pose_block, pose));
    const auto animation_id = int32_t(MEM_W(pose_animation, pose));
    const float progress = read_f32(rdram, pose, pose_progress);
    jiggy::BkMatrix world;
    for (uint32_t i = 0; i < 16; i++) {
        world[i / 4][i % 4] = read_f32(rdram, pose, pose_world + i * 4);
    }
    jiggy::BkSelectors selectors;
    for (uint32_t i = 0; i < selectors.size(); i++) {
        selectors[i] = int32_t(MEM_W(pose_selectors + i * 4, pose));
    }
    const gpr gfx = MEM_W(pose_gfx, pose);
    const uint32_t gfx_capacity = uint32_t(MEM_W(pose_gfx_capacity, pose));
    const gpr mtx = MEM_W(pose_mtx, pose);
    const uint32_t mtx_capacity = uint32_t(MEM_W(pose_mtx_capacity, pose));

    if (animation_id >= 0) {
        cache_animation(uint32_t(animation_id));
    }
    std::lock_guard lock{model_mutex};
    auto built = built_models.find(id);
    if (built == built_models.end() || built->second.address != block_address) {
        _return<s32>(ctx, -1);
        return;
    }
    const BuiltModel& m = built->second;
    std::vector<jiggy::BkBoneTransform> transforms;
    if (animation_id >= 0) {
        auto animation = animations.find(uint32_t(animation_id));
        if (animation != animations.end() && animation->second) {
            transforms = animation->second->sample(progress);
        }
    }
    const auto bones = jiggy::bk_skeleton_matrices(m.skeleton, transforms);
    if (bones.size() + 1 > mtx_capacity) {
        _return<s32>(ctx, -1);
        return;
    }

    // The root matrix first, then one per skeleton entry, each placed in the world.
    auto write_mtx = [&](uint32_t index, const jiggy::BkMatrix& matrix) {
        const auto bytes = jiggy::bk_matrix_to_mtx(matrix);
        for (uint32_t i = 0; i < bytes.size(); i++) {
            MEM_B(index * 64 + i, mtx) = static_cast<s8>(bytes[i]);
        }
        return uint32_t(mtx) + index * 64;
    };
    const uint32_t root = write_mtx(0, world);
    std::vector<uint32_t> addresses;
    addresses.reserve(bones.size());
    for (size_t i = 0; i < bones.size(); i++) {
        addresses.push_back(write_mtx(uint32_t(i + 1), jiggy::bk_matrix_multiply(bones[i], world)));
    }

    std::string error;
    auto list = jiggy::bk_pose_display_list(m.model, m.block, m.address, root, addresses, selectors, &error);
    if (!list || list->commands.size() > gfx_capacity) {
        if (!list) {
            fprintf(stderr, "[Jiggy] Banjo-Kazooie model 0x%X can't be posed: %s\n", id, error.c_str());
        }
        _return<s32>(ctx, -1);
        return;
    }
    for (size_t i = 0; i < list->commands.size(); i++) {
        MEM_W(i * 8, gfx) = s32(list->commands[i][0]);
        MEM_W(i * 8 + 4, gfx) = s32(list->commands[i][1]);
    }
    _return<s32>(ctx, s32(list->commands.size()));
}
