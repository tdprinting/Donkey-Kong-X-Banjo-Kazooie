#include "jiggy/bk_gfx.hpp"

#include <array>

namespace jiggy {

namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

// F3DEX opcodes (Banjo-Kazooie's microcode).
namespace f3dex {
constexpr uint8_t G_VTX = 0x04;
constexpr uint8_t G_DL = 0x06;
constexpr uint8_t G_POPMTX = 0xBD;
constexpr uint8_t G_TRI2 = 0xB1;
constexpr uint8_t G_RDPHALF_2 = 0xB3;
constexpr uint8_t G_RDPHALF_1 = 0xB4;
constexpr uint8_t G_CLEARGEOMETRYMODE = 0xB6;
constexpr uint8_t G_SETGEOMETRYMODE = 0xB7;
constexpr uint8_t G_ENDDL = 0xB8;
constexpr uint8_t G_SETOTHERMODE_L = 0xB9;
constexpr uint8_t G_SETOTHERMODE_H = 0xBA;
constexpr uint8_t G_TEXTURE = 0xBB;
constexpr uint8_t G_TRI1 = 0xBF;
constexpr uint8_t G_NOOP = 0xC0;
} // namespace f3dex

// F3DEX2 opcodes (DK64's).
namespace f3dex2 {
constexpr uint32_t G_NOOP = 0x00;
constexpr uint32_t G_VTX = 0x01;
constexpr uint32_t G_TRI1 = 0x05;
constexpr uint32_t G_TRI2 = 0x06;
constexpr uint32_t G_TEXTURE = 0xD7;
constexpr uint32_t G_GEOMETRYMODE = 0xD9;
constexpr uint32_t G_DL = 0xDE;
constexpr uint32_t G_ENDDL = 0xDF;
constexpr uint32_t G_RDPHALF_1 = 0xE1;
constexpr uint32_t G_SETOTHERMODE_L = 0xE2;
constexpr uint32_t G_SETOTHERMODE_H = 0xE3;
constexpr uint32_t G_RDPHALF_2 = 0xF1;
} // namespace f3dex2

constexpr uint8_t G_RDPPIPESYNC = 0xE7;
constexpr uint8_t G_SETBLENDCOLOR = 0xF9;
constexpr uint8_t G_SETENVCOLOR = 0xFB;
constexpr uint8_t G_SETTIMG = 0xFD;

constexpr uint32_t kVertexSize = 0x10;

// Banjo-Kazooie's renderModesFullDepthOpa (src/core2/modelRender.c): one
// gsDPSetRenderMode + gsSPEndDisplayList per entry. These are the render mode words.
constexpr std::array<uint32_t, 13> kRenderModes = {
    0x0F0A4030, 0x0C192078, 0x0C184270, 0x0C1841F8, 0x0C184270, 0x0C1841F8, 0x0F0A4010,
    0x0C192058, 0x0C184250, 0x0C1841D8, 0x0C184250, 0x0C1841D8, 0x0C1843D8,
};

// Geometry mode bits that moved between F3DEX and F3DEX2. The rest are the same.
uint32_t geometry_mode_to_f3dex2(uint32_t mode) {
    constexpr uint32_t kSmoothF3dex = 0x00000200, kSmoothF3dex2 = 0x00200000;
    constexpr uint32_t kCullFrontF3dex = 0x00001000, kCullFrontF3dex2 = 0x00000200;
    constexpr uint32_t kCullBackF3dex = 0x00002000, kCullBackF3dex2 = 0x00000400;
    uint32_t out = mode & ~(kSmoothF3dex | kCullFrontF3dex | kCullBackF3dex);
    if (mode & kSmoothF3dex) {
        out |= kSmoothF3dex2;
    }
    if (mode & kCullFrontF3dex) {
        out |= kCullFrontF3dex2;
    }
    if (mode & kCullBackF3dex) {
        out |= kCullBackF3dex2;
    }
    return out;
}

// F3DEX packs SETOTHERMODE as (shift, length); F3DEX2 as (32 - shift - length, length - 1).
uint32_t othermode_to_f3dex2(uint32_t opcode, uint32_t w0) {
    const uint32_t shift = (w0 >> 8) & 0xFF;
    const uint32_t length = w0 & 0xFF;
    if (length == 0 || shift + length > 32) {
        return 0;
    }
    return (opcode << 24) | ((32 - shift - length) << 8) | (length - 1);
}

void put32(std::vector<uint8_t>& out, size_t at, uint32_t value) {
    out[at + 0] = uint8_t(value >> 24);
    out[at + 1] = uint8_t(value >> 16);
    out[at + 2] = uint8_t(value >> 8);
    out[at + 3] = uint8_t(value);
}

size_t align8(size_t value) {
    return (value + 7) & ~size_t(7);
}

} // namespace

std::optional<BkGfxBlock> bk_model_to_f3dex2(const BkModel& model, uint32_t base_address, std::string* error) {
    if (base_address & 7) {
        set_error(error, "base address must be 8-byte aligned");
        return std::nullopt;
    }

    std::vector<std::array<uint32_t, 2>> dl;
    auto emit = [&dl](uint32_t w0, uint32_t w1) { dl.push_back({w0, w1}); };

    // Prologue: Banjo-Kazooie's setup2CycleWhiteEnvDL (its models are built for 2-cycle mode
    // with a pass-through first cycle).
    emit(uint32_t(G_RDPPIPESYNC) << 24, 0);
    emit(0xE3000800, 0x00800000); // gsDPPipelineMode(G_PM_1PRIMITIVE)
    emit(0xE3000A01, 0x00100000); // gsDPSetCycleType(G_CYC_2CYCLE)
    emit(uint32_t(G_SETENVCOLOR) << 24, 0xFFFFFFFF);
    emit(uint32_t(G_SETBLENDCOLOR) << 24, 0x00000080);
    emit(f3dex2::G_ENDDL << 24, 0);

    // Epilogue: back to DK64's usual 1-cycle state.
    const size_t epilogue_at = dl.size();
    emit(uint32_t(G_RDPPIPESYNC) << 24, 0);
    emit(0xE3000A01, 0x00000000); // gsDPSetCycleType(G_CYC_1CYCLE)
    emit(0xE3000800, 0x00000000); // gsDPPipelineMode(G_PM_NPRIMITIVE)
    emit(0xE3001001, 0x00000000); // gsDPSetTextureLUT(G_TT_NONE)
    emit(f3dex2::G_TEXTURE << 24, 0); // gsSPTexture(0, 0, 0, 0, G_OFF)
    emit(uint32_t(G_RDPPIPESYNC) << 24, 0);
    emit(f3dex2::G_ENDDL << 24, 0);

    const size_t commands_at = dl.size();
    const size_t dl_bytes = (commands_at + model.commands().size()) * 8;
    const size_t render_modes_at = dl_bytes;
    const size_t vertices_at = render_modes_at + kRenderModes.size() * 16;
    const size_t textures_at = align8(vertices_at + model.vertices().size() * kVertexSize);
    const size_t total = align8(textures_at + model.texture_data().size());
    if (total > 0x00800000) {
        set_error(error, "model is too large");
        return std::nullopt;
    }

    size_t dropped = 0;
    // Segment addresses become absolute ones in the block. Animated texture segments (11 to
    // 15) point at the texture data as a stand-in until texture animation is supported.
    auto address = [&](uint32_t segmented, bool& ok) -> uint32_t {
        const uint32_t segment = (segmented >> 24) & 0x0F;
        const uint32_t offset = segmented & 0x00FFFFFF;
        ok = true;
        switch (segment) {
            case 1:
                return base_address + uint32_t(vertices_at) + offset;
            case 2:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
                return base_address + uint32_t(textures_at) + offset;
            case 3:
                return base_address + uint32_t(render_modes_at) + offset;
            default:
                ok = false;
                return 0;
        }
    };

    for (const auto& command : model.commands()) {
        const uint32_t w0 = command[0];
        const uint32_t w1 = command[1];
        const uint8_t opcode = uint8_t(w0 >> 24);
        bool ok = true;
        switch (opcode) {
            case f3dex::G_VTX: {
                const uint32_t first = ((w0 >> 16) & 0xFF) / 2;
                const uint32_t count = (w0 >> 10) & 0x3F;
                const uint32_t where = address(w1, ok);
                if (!ok || count == 0 || first + count > 32) {
                    break;
                }
                emit((f3dex2::G_VTX << 24) | (count << 12) | ((first + count) * 2), where);
                continue;
            }
            case f3dex::G_DL: {
                const uint32_t where = address(w1, ok);
                if (!ok) {
                    break;
                }
                emit((f3dex2::G_DL << 24) | (w0 & 0x00FF0000), where);
                continue;
            }
            case f3dex::G_TRI1:
                emit((f3dex2::G_TRI1 << 24) | (w1 & 0x00FFFFFF), 0);
                continue;
            case f3dex::G_TRI2:
                emit((f3dex2::G_TRI2 << 24) | (w0 & 0x00FFFFFF), w1 & 0x00FFFFFF);
                continue;
            case f3dex::G_TEXTURE: {
                const uint32_t level = (w0 >> 11) & 7;
                const uint32_t tile = (w0 >> 8) & 7;
                const uint32_t on = (w0 & 0xFF) ? 1 : 0;
                emit((f3dex2::G_TEXTURE << 24) | (level << 11) | (tile << 8) | (on << 1), w1);
                continue;
            }
            case f3dex::G_SETGEOMETRYMODE:
                emit((f3dex2::G_GEOMETRYMODE << 24) | 0x00FFFFFF, geometry_mode_to_f3dex2(w1));
                continue;
            case f3dex::G_CLEARGEOMETRYMODE:
                emit((f3dex2::G_GEOMETRYMODE << 24) | (~geometry_mode_to_f3dex2(w1) & 0x00FFFFFF), 0);
                continue;
            case f3dex::G_SETOTHERMODE_L:
            case f3dex::G_SETOTHERMODE_H: {
                const uint32_t out = othermode_to_f3dex2(opcode == f3dex::G_SETOTHERMODE_L ? f3dex2::G_SETOTHERMODE_L
                                                                                            : f3dex2::G_SETOTHERMODE_H,
                                                         w0);
                if (out == 0) {
                    break;
                }
                emit(out, w1);
                continue;
            }
            case f3dex::G_RDPHALF_1:
                emit((f3dex2::G_RDPHALF_1 << 24) | (w0 & 0x00FFFFFF), w1);
                continue;
            case f3dex::G_RDPHALF_2:
                emit((f3dex2::G_RDPHALF_2 << 24) | (w0 & 0x00FFFFFF), w1);
                continue;
            case f3dex::G_ENDDL:
                emit(f3dex2::G_ENDDL << 24, 0);
                continue;
            case f3dex::G_NOOP:
            case f3dex::G_POPMTX:
                // Skinning pops back to the enclosing bone's matrix before loading that bone's
                // vertices; the pose display list loads that matrix before calling the list.
                emit(f3dex2::G_NOOP << 24, 0);
                continue;
            default:
                if (opcode >= 0xC8) {
                    // RDP commands are the same in both microcodes.
                    if (opcode == G_SETTIMG) {
                        const uint32_t where = address(w1, ok);
                        if (!ok) {
                            break;
                        }
                        emit(w0, where);
                        continue;
                    }
                    emit(w0, w1);
                    continue;
                }
                // CULLDL, MOVEWORD and the like.
                break;
        }
        dropped++;
        emit(f3dex2::G_NOOP << 24, 0);
    }

    BkGfxBlock block;
    block.bytes.assign(total, 0);
    for (size_t i = 0; i < dl.size(); i++) {
        put32(block.bytes, i * 8, dl[i][0]);
        put32(block.bytes, i * 8 + 4, dl[i][1]);
    }
    for (size_t i = 0; i < kRenderModes.size(); i++) {
        put32(block.bytes, render_modes_at + i * 16, 0xE200001C); // gsDPSetRenderMode
        put32(block.bytes, render_modes_at + i * 16 + 4, kRenderModes[i]);
        put32(block.bytes, render_modes_at + i * 16 + 8, f3dex2::G_ENDDL << 24);
    }
    for (size_t i = 0; i < model.vertices().size(); i++) {
        const auto& v = model.vertices()[i];
        const size_t at = vertices_at + i * kVertexSize;
        for (size_t k = 0; k < 3; k++) {
            block.bytes[at + k * 2] = uint8_t(uint16_t(v.position[k]) >> 8);
            block.bytes[at + k * 2 + 1] = uint8_t(v.position[k]);
        }
        for (size_t k = 0; k < 2; k++) {
            block.bytes[at + 8 + k * 2] = uint8_t(uint16_t(v.st[k]) >> 8);
            block.bytes[at + 8 + k * 2 + 1] = uint8_t(v.st[k]);
        }
        for (size_t k = 0; k < 4; k++) {
            block.bytes[at + 12 + k] = v.color[k];
        }
    }
    std::copy(model.texture_data().begin(), model.texture_data().end(), block.bytes.begin() + std::ptrdiff_t(textures_at));

    block.prologue_offset = 0;
    block.epilogue_offset = uint32_t(epilogue_at * 8);
    block.commands_offset = uint32_t(commands_at * 8);
    block.display_list_commands = dl.size();
    block.dropped_commands = dropped;
    return block;
}

} // namespace jiggy
