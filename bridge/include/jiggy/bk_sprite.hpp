// Banjo-Kazooie sprites (portraits, HUD icons, particles): decoding one frame to RGBA.
//
// Layout (from the CC0 Banjo-Kazooie decompilation, include/structs.h BKSprite /
// BKSpriteFrame / BKSpriteTextureBlock, src/core1/sprite.c sprite_getFramePtr and
// src/core1/framebufferdraw.c framebufferdraw_drawSprite_*):
//
//   sprite + 0x00  s16 frame_count, s16 type (SPRITE_TYPE_* bit), 12 more header bytes
//   sprite + 0x10  s32 offsets[frame_count]; frame i starts at 0x10 + 4 * frame_count + offsets[i]
//   frame  + 0x00  s16 unk0, unk2, w, h, chunk_count, then 10 more header bytes
//   frame  + 0x14  palette formats only: palette (RGBA5551) aligned to 8 bytes, 16 or 256 entries
//                  then chunk_count x { s16 x, y, w, h; texels aligned to 8 bytes }
//
// The game aligns to 8 bytes in RAM, and assets are loaded 8-byte aligned, so alignment
// here is relative to the start of the sprite.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy {

enum class BkSpriteFormat : uint16_t {
    CI4 = 0x001,
    CI8 = 0x004,
    I4 = 0x020,
    I8 = 0x040,
    IA4 = 0x080,
    IA8 = 0x100,
    RGBA16 = 0x400,
    RGBA32 = 0x800,
};

// Name like "CI4", or nullptr for a type the decoder doesn't handle.
const char* bk_sprite_format_name(uint16_t type);

struct BkSpriteImage {
    uint16_t width = 0;
    uint16_t height = 0;
    std::vector<uint8_t> rgba; // width * height * 4 bytes, R G B A, rows top to bottom.
};

class BkSprite {
public:
    static std::optional<BkSprite> parse(std::span<const uint8_t> data, std::string* error);

    size_t frame_count() const { return frame_offsets_.size(); }
    uint16_t type() const { return type_; }

    // Decodes one frame onto a transparent canvas the size the frame header gives.
    std::optional<BkSpriteImage> decode_frame(size_t frame, std::string* error) const;
    // Decodes one chunk of a frame on its own, the size the chunk header gives. Fonts keep a
    // letter per chunk (src/core2/print.c print_getLettersFromFont).
    std::optional<BkSpriteImage> decode_chunk(size_t frame, size_t chunk, std::string* error) const;

private:
    std::optional<BkSpriteImage> decode(size_t frame, const size_t* only_chunk, std::string* error) const;

    std::vector<uint8_t> data_; // A copy, so the sprite outlives the buffer it was parsed from.
    uint16_t type_ = 0;
    std::vector<size_t> frame_offsets_;
};

// A letter of Banjo-Kazooie's bold font (the HUD's numbers and titles): chunk `index` of the
// font's RGBA32 alpha mask, colored by a texture sprite centered on it, as
// print_applyTextureToBoldFontLetter does. The mask keeps intensity in its blue byte and
// coverage in alpha.
std::optional<BkSpriteImage> bk_bold_font_letter(const BkSprite& mask, const BkSprite& texture, size_t index,
                                                 std::string* error);

// Packs RGBA8888 pixels into the N64's RGBA5551, big-endian, alpha set when alpha >= 128.
std::vector<uint8_t> rgba8888_to_rgba5551(std::span<const uint8_t> rgba);

} // namespace jiggy
