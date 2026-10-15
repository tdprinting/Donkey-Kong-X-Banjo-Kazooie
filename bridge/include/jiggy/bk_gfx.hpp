// Turns a Banjo-Kazooie model into something the DK64 engine can draw: one F3DEX2 display
// list plus the vertices, textures and render modes it needs, in a single block of N64
// memory.
//
// Banjo-Kazooie draws with F3DEX and DK64 with F3DEX2. The two share the RDP commands, so
// those are copied as they are; the RSP commands (vertices, triangles, geometry and other
// modes, textures) are re-encoded, and segment addresses become absolute ones inside the
// block. The model's sub-lists keep their place and their ends, so the geo list can call
// them one at a time with each bone's matrix (see bk_pose.hpp). Their matrix pops become
// no-ops: the pose display list loads the matrix a pop would have restored instead.
//
// The block, from base_address:
//   prologue       sub-list setting up 2-cycle mode, as Banjo-Kazooie's modelRender does
//   epilogue       sub-list restoring DK64's defaults
//   commands       every command of the model in file order, one F3DEX2 command each
//   render modes   the table Banjo-Kazooie puts in segment 3 (renderModesFullDepthOpa)
//   vertices       the model's Vtx array (segment 1)
//   textures       the model's texture data, palettes included (segment 2)
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "jiggy/bk_model.hpp"

namespace jiggy {

struct BkGfxBlock {
    std::vector<uint8_t> bytes;   // Big-endian, as the block should appear in N64 memory.
    uint32_t prologue_offset = 0; // Offsets into bytes.
    uint32_t epilogue_offset = 0;
    uint32_t commands_offset = 0; // Model command i is at commands_offset + i * 8.
    size_t display_list_commands = 0;
    size_t dropped_commands = 0;  // Model commands with no F3DEX2 equivalent here (now no-ops).
};

// base_address is where the block will be placed (a KSEG0 address, 8-byte aligned).
std::optional<BkGfxBlock> bk_model_to_f3dex2(const BkModel& model, uint32_t base_address, std::string* error);

} // namespace jiggy
