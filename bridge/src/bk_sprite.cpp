#include "jiggy/bk_sprite.hpp"

#include "byte_io.hpp"

#include <algorithm>

namespace jiggy {

namespace {

constexpr size_t kSpriteHeaderSize = 0x10;
constexpr size_t kFrameHeaderSize = 0x14;
constexpr size_t kChunkHeaderSize = 8;

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

size_t align8(size_t offset) {
    return (offset + 7) & ~size_t(7);
}

int16_t read_s16(std::span<const uint8_t> data, size_t offset) {
    return int16_t(read_be16(data, offset));
}

// One texel as RGBA8888.
struct Texel {
    uint8_t r, g, b, a;
};

Texel from_rgba5551(uint16_t c) {
    auto expand5 = [](uint32_t v) { return uint8_t((v << 3) | (v >> 2)); };
    return {expand5((c >> 11) & 0x1F), expand5((c >> 6) & 0x1F), expand5((c >> 1) & 0x1F),
            uint8_t((c & 1) ? 0xFF : 0)};
}

Texel gray(uint8_t intensity, uint8_t alpha) {
    return {intensity, intensity, intensity, alpha};
}

// Bits per texel in a chunk's texel data.
unsigned texel_bits(uint16_t type) {
    switch (BkSpriteFormat(type)) {
        case BkSpriteFormat::CI4:
        case BkSpriteFormat::I4:
        case BkSpriteFormat::IA4:
            return 4;
        case BkSpriteFormat::CI8:
        case BkSpriteFormat::I8:
        case BkSpriteFormat::IA8:
            return 8;
        case BkSpriteFormat::RGBA16:
            return 16;
        case BkSpriteFormat::RGBA32:
            return 32;
    }
    return 0;
}

size_t palette_entries(uint16_t type) {
    switch (BkSpriteFormat(type)) {
        case BkSpriteFormat::CI4:
            return 16;
        case BkSpriteFormat::CI8:
            return 256;
        default:
            return 0;
    }
}

} // namespace

const char* bk_sprite_format_name(uint16_t type) {
    switch (BkSpriteFormat(type)) {
        case BkSpriteFormat::CI4: return "CI4";
        case BkSpriteFormat::CI8: return "CI8";
        case BkSpriteFormat::I4: return "I4";
        case BkSpriteFormat::I8: return "I8";
        case BkSpriteFormat::IA4: return "IA4";
        case BkSpriteFormat::IA8: return "IA8";
        case BkSpriteFormat::RGBA16: return "RGBA16";
        case BkSpriteFormat::RGBA32: return "RGBA32";
    }
    return nullptr;
}

std::optional<BkSprite> BkSprite::parse(std::span<const uint8_t> data, std::string* error) {
    if (data.size() < kSpriteHeaderSize) {
        set_error(error, "sprite is too small for its header");
        return std::nullopt;
    }
    const int16_t frame_count = read_s16(data, 0);
    const uint16_t type = read_be16(data, 2);
    if (frame_count <= 0) {
        set_error(error, "sprite has no frames");
        return std::nullopt;
    }
    if (!bk_sprite_format_name(type)) {
        set_error(error, "sprite has unsupported type " + std::to_string(type));
        return std::nullopt;
    }
    const size_t table_end = kSpriteHeaderSize + 4 * size_t(frame_count);
    if (!range_ok(data, 0, table_end)) {
        set_error(error, "sprite frame table runs past the end of the data");
        return std::nullopt;
    }

    BkSprite sprite;
    sprite.data_.assign(data.begin(), data.end());
    sprite.type_ = type;
    for (int16_t i = 0; i < frame_count; i++) {
        const uint64_t offset = uint64_t(table_end) + read_be32(data, kSpriteHeaderSize + 4 * size_t(i));
        if (!range_ok(data, offset, kFrameHeaderSize)) {
            set_error(error, "sprite frame " + std::to_string(i) + " is outside the data");
            return std::nullopt;
        }
        sprite.frame_offsets_.push_back(size_t(offset));
    }
    return sprite;
}

std::optional<BkSpriteImage> BkSprite::decode_frame(size_t frame, std::string* error) const {
    return decode(frame, nullptr, error);
}

std::optional<BkSpriteImage> BkSprite::decode_chunk(size_t frame, size_t chunk, std::string* error) const {
    return decode(frame, &chunk, error);
}

std::optional<BkSpriteImage> BkSprite::decode(size_t frame, const size_t* only_chunk, std::string* error) const {
    if (frame >= frame_offsets_.size()) {
        set_error(error, "sprite has no frame " + std::to_string(frame));
        return std::nullopt;
    }
    const size_t base = frame_offsets_[frame];
    const int16_t width = read_s16(data_, base + 4);
    const int16_t height = read_s16(data_, base + 6);
    const int16_t chunk_count = read_s16(data_, base + 8);
    if (width <= 0 || height <= 0 || chunk_count < 0) {
        set_error(error, "sprite frame has a bad size");
        return std::nullopt;
    }
    if (only_chunk && *only_chunk >= size_t(chunk_count)) {
        set_error(error, "sprite frame has no chunk " + std::to_string(*only_chunk));
        return std::nullopt;
    }

    BkSpriteImage image;
    if (!only_chunk) {
        image.width = uint16_t(width);
        image.height = uint16_t(height);
        image.rgba.assign(size_t(width) * size_t(height) * 4, 0);
    }

    std::vector<Texel> palette;
    size_t pos = base + kFrameHeaderSize;
    if (const size_t entries = palette_entries(type_)) {
        pos = align8(pos);
        if (!range_ok(data_, pos, entries * 2)) {
            set_error(error, "sprite palette runs past the end of the data");
            return std::nullopt;
        }
        for (size_t i = 0; i < entries; i++) {
            palette.push_back(from_rgba5551(read_be16(data_, pos + 2 * i)));
        }
        pos += entries * 2;
    }

    const unsigned bits = texel_bits(type_);
    for (int16_t c = 0; c < chunk_count; c++) {
        if (!range_ok(data_, pos, kChunkHeaderSize)) {
            set_error(error, "sprite chunk " + std::to_string(c) + " is outside the data");
            return std::nullopt;
        }
        const int16_t cx = read_s16(data_, pos);
        const int16_t cy = read_s16(data_, pos + 2);
        const int16_t cw = read_s16(data_, pos + 4);
        const int16_t ch = read_s16(data_, pos + 6);
        if (cw < 0 || ch < 0) {
            set_error(error, "sprite chunk has a bad size");
            return std::nullopt;
        }
        pos = align8(pos + kChunkHeaderSize);
        // 4-bit rows are whole bytes: the game steps two texels per byte, odd widths included.
        const size_t row_bytes = bits == 4 ? (size_t(cw) + 1) / 2 : size_t(cw) * bits / 8;
        const size_t chunk_bytes = row_bytes * size_t(ch);
        if (!range_ok(data_, pos, chunk_bytes)) {
            set_error(error, "sprite chunk texels run past the end of the data");
            return std::nullopt;
        }
        // On its own, a chunk is its own canvas.
        int origin_x = cx;
        int origin_y = cy;
        if (only_chunk) {
            if (size_t(c) != *only_chunk) {
                pos += chunk_bytes;
                continue;
            }
            origin_x = 0;
            origin_y = 0;
            image.width = uint16_t(cw);
            image.height = uint16_t(ch);
            image.rgba.assign(size_t(cw) * size_t(ch) * 4, 0);
        }
        const int canvas_w = image.width;
        const int canvas_h = image.height;

        for (int iy = 0; iy < ch; iy++) {
            const size_t row = pos + size_t(iy) * row_bytes;
            for (int ix = 0; ix < cw; ix++) {
                Texel t{};
                switch (BkSpriteFormat(type_)) {
                    case BkSpriteFormat::CI4: {
                        const uint8_t b = data_[row + size_t(ix) / 2];
                        t = palette[(ix & 1) ? (b & 0xF) : (b >> 4)];
                        break;
                    }
                    case BkSpriteFormat::CI8:
                        t = palette[data_[row + size_t(ix)]];
                        break;
                    case BkSpriteFormat::I4: {
                        const uint8_t b = data_[row + size_t(ix) / 2];
                        const uint8_t i = uint8_t(((ix & 1) ? (b & 0xF) : (b >> 4)) * 0x11);
                        t = gray(i, i);
                        break;
                    }
                    case BkSpriteFormat::I8: {
                        const uint8_t i = data_[row + size_t(ix)];
                        t = gray(i, i);
                        break;
                    }
                    case BkSpriteFormat::IA4: {
                        const uint8_t b = data_[row + size_t(ix) / 2];
                        const uint8_t n = (ix & 1) ? (b & 0xF) : (b >> 4);
                        const uint8_t i3 = n >> 1;
                        t = gray(uint8_t((i3 << 5) | (i3 << 2) | (i3 >> 1)), (n & 1) ? 0xFF : 0);
                        break;
                    }
                    case BkSpriteFormat::IA8: {
                        const uint8_t b = data_[row + size_t(ix)];
                        t = gray(uint8_t((b >> 4) * 0x11), uint8_t((b & 0xF) * 0x11));
                        break;
                    }
                    case BkSpriteFormat::RGBA16:
                        t = from_rgba5551(read_be16(data_, row + 2 * size_t(ix)));
                        break;
                    case BkSpriteFormat::RGBA32: {
                        const size_t p = row + 4 * size_t(ix);
                        t = {data_[p], data_[p + 1], data_[p + 2], data_[p + 3]};
                        break;
                    }
                }
                const int x = origin_x + ix;
                const int y = origin_y + iy;
                if (x < 0 || y < 0 || x >= canvas_w || y >= canvas_h) {
                    continue;
                }
                uint8_t* out = &image.rgba[(size_t(y) * size_t(canvas_w) + size_t(x)) * 4];
                out[0] = t.r;
                out[1] = t.g;
                out[2] = t.b;
                out[3] = t.a;
            }
        }
        pos += chunk_bytes;
    }
    return image;
}

std::optional<BkSpriteImage> bk_bold_font_letter(const BkSprite& mask, const BkSprite& texture, size_t index,
                                                 std::string* error) {
    if (mask.type() != uint16_t(BkSpriteFormat::RGBA32)) {
        set_error(error, "bold font mask is not RGBA32");
        return std::nullopt;
    }
    auto letter = mask.decode_chunk(0, index, error);
    if (!letter) {
        return std::nullopt;
    }
    auto colors = texture.decode_frame(0, error);
    if (!colors) {
        return std::nullopt;
    }
    // print_setBoldFontTexturePixel: the texture's 5-bit color times intensity / 31 (integer
    // division, as the game does), keeping the mask's alpha.
    const int x_min = (int(colors->width) - int(letter->width)) >> 1;
    const int y_min = (int(colors->height) - int(letter->height)) >> 1;
    for (int y = 0; y < letter->height; y++) {
        for (int x = 0; x < letter->width; x++) {
            uint8_t* px = &letter->rgba[(size_t(y) * letter->width + size_t(x)) * 4];
            const int tx = std::clamp(x + x_min, 0, int(colors->width) - 1);
            const int ty = std::clamp(y + y_min, 0, int(colors->height) - 1);
            const uint8_t* tc = &colors->rgba[(size_t(ty) * colors->width + size_t(tx)) * 4];
            const int k = px[2] / 0x1F;
            px[0] = uint8_t((tc[0] >> 3) * k);
            px[1] = uint8_t((tc[1] >> 3) * k);
            px[2] = uint8_t((tc[2] >> 3) * k);
        }
    }
    return letter;
}

std::vector<uint8_t> rgba8888_to_rgba5551(std::span<const uint8_t> rgba) {
    std::vector<uint8_t> out;
    out.reserve(rgba.size() / 2);
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const uint16_t c = uint16_t(((rgba[i] >> 3) << 11) | ((rgba[i + 1] >> 3) << 6) | ((rgba[i + 2] >> 3) << 1) |
                                    (rgba[i + 3] >= 128 ? 1 : 0));
        out.push_back(uint8_t(c >> 8));
        out.push_back(uint8_t(c & 0xFF));
    }
    return out;
}

} // namespace jiggy
