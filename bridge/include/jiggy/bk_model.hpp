// Banjo-Kazooie models: reading the vertex list, textures and F3DEX display lists into a
// plain triangle mesh, the first step of converting them for the DK64 engine.
//
// Layout (from the CC0 Banjo-Kazooie decompilation, include/core2/model.h BKModelBin /
// BKTextureList / BKGfxList / BKVertexList, src/core2/modelRender.c modelRender_draw and
// src/core2/code_63690.c):
//
//   model + 0x00  u32 magic 0x0000000B
//   model + 0x04  s32 geo_list_offset       (render commands: bones, LODs, which lists to draw)
//   model + 0x08  s16 texture_list_offset, s16 geo_type
//   model + 0x0C  s32 gfx_list_offset, s32 vtx_list_offset
//   model + 0x30  u16 tri_count, u16 vertex_count
//   textures      s32 size, s16 count, u16 pad, count x { s32 offset; s16 type; u8 pad[2];
//                 u8 width, height; u8 pad[6] }, then the texture data. Each texture's data
//                 is its palette (CI4: 32 bytes, CI8: 512 bytes) followed by its texels.
//   gfx list      u32 command count, u32 pad, then 8-byte F3DEX commands
//   vtx list      s16 min[3], max[3], center[3], local_norm, count, global_norm, then Vtx[count]
//
// The game draws with segment 1 set to the vertices and segment 2 to the texture data, so
// the display lists address both by offset. Segment 3 holds render-mode sub-lists and
// segments 11 to 15 hold animated texture frames.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy {

struct BkModelVertex {
    std::array<int16_t, 3> position{};
    std::array<int16_t, 2> st{};    // Texel coordinates in s10.5 fixed point.
    std::array<uint8_t, 4> color{}; // RGBA, or a normal and alpha on lit models.
};

struct BkModelTexture {
    uint32_t offset = 0; // From the start of the texture data.
    uint16_t type = 0;   // BK_TEXTURE_INFO_TYPE_* bit.
    uint8_t width = 0;
    uint8_t height = 0;
};

struct BkModelTriangle {
    std::array<uint16_t, 3> vertices{};          // Indices into BkModel::vertices().
    std::array<std::array<float, 2>, 3> uv{};    // Normalised to the bound texture, 0..1 per repeat.
    int texture = -1;                            // Index into BkModel::textures(), -1 for none or animated.
};

struct BkModelImage {
    uint16_t width = 0;
    uint16_t height = 0;
    std::vector<uint8_t> rgba; // width * height * 4 bytes, rows top to bottom.
};

class BkModel {
public:
    static constexpr uint32_t kMagic = 0x0000000B;

    // Reads the header, vertices, textures and display lists, then walks the display lists to
    // collect triangles.
    static std::optional<BkModel> parse(std::span<const uint8_t> data, std::string* error);

    const std::vector<BkModelVertex>& vertices() const { return vertices_; }
    const std::vector<BkModelTexture>& textures() const { return textures_; }
    const std::vector<BkModelTriangle>& triangles() const { return triangles_; }

    // The triangle count from the header, which counts what the display lists draw.
    uint16_t header_triangle_count() const { return header_tri_count_; }
    uint16_t geo_type() const { return geo_type_; }
    size_t display_list_commands() const { return gfx_count_; }
    // The raw F3DEX commands ({w0, w1}), in file order, and the texture data they address
    // through segment 2.
    const std::vector<std::array<uint32_t, 2>>& commands() const { return commands_; }
    const std::vector<uint8_t>& texture_data() const { return texture_data_; }

    std::optional<BkModelImage> decode_texture(size_t index, std::string* error) const;

private:
    std::vector<uint8_t> texture_data_;
    std::vector<BkModelVertex> vertices_;
    std::vector<BkModelTexture> textures_;
    std::vector<BkModelTriangle> triangles_;
    std::vector<std::array<uint32_t, 2>> commands_;
    uint16_t header_tri_count_ = 0;
    uint16_t geo_type_ = 0;
    size_t gfx_count_ = 0;
};

// Bytes of palette before a texture's texels, and bits per texel, for a BK texture type.
size_t bk_texture_palette_size(uint16_t type);
unsigned bk_texture_bits(uint16_t type);
const char* bk_texture_format_name(uint16_t type);

} // namespace jiggy
