#include "jiggy/bk_model.hpp"

#include "byte_io.hpp"

#include <algorithm>

namespace jiggy {

namespace {

constexpr size_t kHeaderSize = 0x38;
constexpr size_t kTextureInfoSize = 0x10;
constexpr size_t kVertexListHeaderSize = 0x18;
constexpr size_t kVertexSize = 0x10;
constexpr size_t kVertexCacheSize = 32; // F3DEX's vertex buffer.

// F3DEX opcodes (the game is built with F3DEX_GBI).
constexpr uint8_t G_VTX = 0x04;
constexpr uint8_t G_TRI2 = 0xB1;
constexpr uint8_t G_TEXTURE = 0xBB;
constexpr uint8_t G_TRI1 = 0xBF;
constexpr uint8_t G_SETTIMG = 0xFD;

constexpr uint8_t kVertexSegment = 0x01;
constexpr uint8_t kTextureSegment = 0x02;

constexpr uint16_t kCi4 = 0x01;
constexpr uint16_t kCi8 = 0x02;
constexpr uint16_t kRgba16 = 0x04;
constexpr uint16_t kRgba32 = 0x08;

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

std::array<uint8_t, 4> from_rgba5551(uint16_t c) {
    auto expand5 = [](uint32_t v) { return uint8_t((v << 3) | (v >> 2)); };
    return {expand5((c >> 11) & 0x1F), expand5((c >> 6) & 0x1F), expand5((c >> 1) & 0x1F),
            uint8_t((c & 1) ? 0xFF : 0)};
}

} // namespace

size_t bk_texture_palette_size(uint16_t type) {
    if (type & kCi4) return 32;
    if (type & kCi8) return 512;
    return 0;
}

unsigned bk_texture_bits(uint16_t type) {
    if (type & kCi4) return 4;
    if (type & kCi8) return 8;
    if (type & kRgba16) return 16;
    if (type & kRgba32) return 32;
    return 0;
}

const char* bk_texture_format_name(uint16_t type) {
    if (type & kCi4) return "CI4";
    if (type & kCi8) return "CI8";
    if (type & kRgba16) return "RGBA16";
    if (type & kRgba32) return "RGBA32";
    return nullptr;
}

std::optional<BkModel> BkModel::parse(std::span<const uint8_t> data, std::string* error) {
    if (data.size() < kHeaderSize || read_be32(data, 0) != kMagic) {
        set_error(error, "not a Banjo-Kazooie model (bad magic)");
        return std::nullopt;
    }
    BkModel model;
    const uint32_t texture_list = read_be16(data, 0x08);
    model.geo_type_ = read_be16(data, 0x0A);
    const uint32_t gfx_list = read_be32(data, 0x0C);
    const uint32_t vtx_list = read_be32(data, 0x10);
    model.header_tri_count_ = read_be16(data, 0x30);
    const uint32_t geo_list = read_be32(data, 0x04);
    if (geo_list != 0 && geo_list < data.size()) {
        model.geo_list_.assign(data.begin() + std::ptrdiff_t(geo_list), data.end());
    }

    if (vtx_list == 0 || !range_ok(data, vtx_list, kVertexListHeaderSize)) {
        set_error(error, "model has no vertex list");
        return std::nullopt;
    }
    const auto vertex_count = uint16_t(read_be16(data, vtx_list + 0x14));
    const size_t vertices_at = vtx_list + kVertexListHeaderSize;
    if (!range_ok(data, vertices_at, size_t(vertex_count) * kVertexSize)) {
        set_error(error, "vertex list runs past the end of the model");
        return std::nullopt;
    }
    model.vertices_.resize(vertex_count);
    for (size_t i = 0; i < vertex_count; i++) {
        const size_t at = vertices_at + i * kVertexSize;
        auto& v = model.vertices_[i];
        for (size_t k = 0; k < 3; k++) {
            v.position[k] = int16_t(read_be16(data, at + k * 2));
        }
        v.st = {int16_t(read_be16(data, at + 8)), int16_t(read_be16(data, at + 10))};
        v.color = {data[at + 12], data[at + 13], data[at + 14], data[at + 15]};
    }

    if (texture_list != 0) {
        if (!range_ok(data, texture_list, 8)) {
            set_error(error, "texture list is outside the model");
            return std::nullopt;
        }
        const uint32_t data_size = read_be32(data, texture_list);
        const auto count = uint16_t(read_be16(data, texture_list + 4));
        const size_t infos_at = texture_list + 8;
        const size_t data_at = infos_at + size_t(count) * kTextureInfoSize;
        if (!range_ok(data, infos_at, size_t(count) * kTextureInfoSize)) {
            set_error(error, "texture list runs past the end of the model");
            return std::nullopt;
        }
        for (size_t i = 0; i < count; i++) {
            const size_t at = infos_at + i * kTextureInfoSize;
            model.textures_.push_back({read_be32(data, at), read_be16(data, at + 4), data[at + 8], data[at + 9]});
        }
        // The size field covers the texture data; clamp to what the file holds.
        if (data_at <= data.size()) {
            const size_t n = std::min<size_t>(data_size, data.size() - data_at);
            model.texture_data_.assign(data.begin() + std::ptrdiff_t(data_at), data.begin() + std::ptrdiff_t(data_at + n));
        }
    }

    if (gfx_list == 0 || !range_ok(data, gfx_list, 8)) {
        set_error(error, "model has no display list");
        return std::nullopt;
    }
    model.gfx_count_ = read_be32(data, gfx_list);
    const size_t commands_at = gfx_list + 8;
    if (!range_ok(data, commands_at, model.gfx_count_ * 8)) {
        set_error(error, "display list runs past the end of the model");
        return std::nullopt;
    }

    // Walk every command in order. The geo list picks which of these sub-lists to draw (LODs,
    // bones), but every sub-list lives in this one array, so a linear pass sees all geometry.
    std::array<int, kVertexCacheSize> cache;
    cache.fill(-1);
    int texture = -1;
    float scale_s = 1.0f;
    float scale_t = 1.0f;

    auto add_triangle = [&](uint32_t packed) {
        std::array<int, 3> slots = {int((packed >> 16) & 0xFF) / 2, int((packed >> 8) & 0xFF) / 2, int(packed & 0xFF) / 2};
        BkModelTriangle tri;
        tri.texture = texture;
        for (size_t k = 0; k < 3; k++) {
            if (slots[k] >= int(kVertexCacheSize) || cache[size_t(slots[k])] < 0) {
                return;
            }
            const auto index = uint16_t(cache[size_t(slots[k])]);
            tri.vertices[k] = index;
            const auto& v = model.vertices_[index];
            float w = 32.0f;
            float h = 32.0f;
            if (texture >= 0) {
                w = std::max<float>(1.0f, model.textures_[size_t(texture)].width);
                h = std::max<float>(1.0f, model.textures_[size_t(texture)].height);
            }
            tri.uv[k] = {v.st[0] / 32.0f * scale_s / w, v.st[1] / 32.0f * scale_t / h};
        }
        model.triangles_.push_back(tri);
    };

    for (size_t i = 0; i < model.gfx_count_; i++) {
        const size_t at = commands_at + i * 8;
        const uint32_t w0 = read_be32(data, at);
        const uint32_t w1 = read_be32(data, at + 4);
        model.commands_.push_back({w0, w1});
        switch (uint8_t(w0 >> 24)) {
            case G_VTX: {
                if ((w1 >> 24) != kVertexSegment) {
                    break;
                }
                const size_t first_slot = ((w0 >> 16) & 0xFF) / 2;
                const size_t count = ((w0 & 0xFFFF) + 1) / kVertexSize;
                const size_t first_vertex = (w1 & 0xFFFFFF) / kVertexSize;
                for (size_t k = 0; k < count && first_slot + k < kVertexCacheSize; k++) {
                    const size_t vertex = first_vertex + k;
                    cache[first_slot + k] = vertex < model.vertices_.size() ? int(vertex) : -1;
                }
                break;
            }
            case G_TRI1:
                add_triangle(w1);
                break;
            case G_TRI2:
                add_triangle(w0 & 0xFFFFFF);
                add_triangle(w1);
                break;
            case G_TEXTURE: {
                const auto s = (w1 >> 16) & 0xFFFF;
                const auto t = w1 & 0xFFFF;
                scale_s = s == 0xFFFF ? 1.0f : float(s) / 65536.0f;
                scale_t = t == 0xFFFF ? 1.0f : float(t) / 65536.0f;
                break;
            }
            case G_SETTIMG: {
                const uint8_t segment = uint8_t(w1 >> 24);
                if (segment != kTextureSegment) {
                    // Animated textures (segments 11 to 15) and anything else we can't resolve.
                    if (segment >= 11 && segment <= 15) {
                        texture = -1;
                    }
                    break;
                }
                const uint32_t offset = w1 & 0xFFFFFF;
                for (size_t t = 0; t < model.textures_.size(); t++) {
                    const auto& info = model.textures_[t];
                    // Palette loads point at the texture's start; texel loads point past the palette.
                    if (offset == info.offset + bk_texture_palette_size(info.type)) {
                        texture = int(t);
                        break;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    return model;
}

std::optional<BkModelImage> BkModel::decode_texture(size_t index, std::string* error) const {
    if (index >= textures_.size()) {
        set_error(error, "texture index out of range");
        return std::nullopt;
    }
    const auto& info = textures_[index];
    const unsigned bits = bk_texture_bits(info.type);
    if (bits == 0) {
        set_error(error, "unknown texture type " + std::to_string(info.type));
        return std::nullopt;
    }
    const size_t palette = bk_texture_palette_size(info.type);
    const size_t pixels = size_t(info.width) * info.height;
    const std::span<const uint8_t> bytes(texture_data_);
    if (!range_ok(bytes, info.offset, palette + pixels * bits / 8)) {
        set_error(error, "texture data runs past the end of the model");
        return std::nullopt;
    }
    BkModelImage image;
    image.width = info.width;
    image.height = info.height;
    image.rgba.resize(pixels * 4);
    const size_t texels = info.offset + palette;
    for (size_t p = 0; p < pixels; p++) {
        std::array<uint8_t, 4> c{};
        switch (bits) {
            case 4: {
                const uint8_t byte = bytes[texels + p / 2];
                const uint8_t i = (p & 1) ? (byte & 0xF) : (byte >> 4);
                c = from_rgba5551(read_be16(bytes, info.offset + i * 2));
                break;
            }
            case 8:
                c = from_rgba5551(read_be16(bytes, info.offset + bytes[texels + p] * 2));
                break;
            case 16:
                c = from_rgba5551(read_be16(bytes, texels + p * 2));
                break;
            case 32:
                c = {bytes[texels + p * 4], bytes[texels + p * 4 + 1], bytes[texels + p * 4 + 2], bytes[texels + p * 4 + 3]};
                break;
        }
        std::copy(c.begin(), c.end(), image.rgba.begin() + std::ptrdiff_t(p * 4));
    }
    return image;
}

} // namespace jiggy
