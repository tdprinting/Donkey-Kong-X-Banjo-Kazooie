// Turns a Banjo-Kazooie model into something the DK64 engine can draw: one F3DEX2 display
// list plus the vertices, textures and render modes it needs, in a single block of N64
// memory.
//
// Banjo-Kazooie draws with F3DEX and DK64 with F3DEX2. The two share the RDP commands, so
// those are copied as they are; the RSP commands (vertices, triangles, geometry and other
// modes, textures) are re-encoded, and segment addresses become absolute ones inside the
// block. The model is drawn in its rest pose: bone matrices aren't applied yet, and the
// display list's own matrix pops are dropped so the caller's matrix stays in place.
//
// The block, from base_address:
//   display list   prologue (2-cycle setup, as Banjo-Kazooie's modelRender uses), every
//                  command of the model in file order, epilogue restoring DK64's defaults
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
    size_t display_list_commands = 0;
    size_t dropped_commands = 0;  // Model commands with no F3DEX2 equivalent here (now no-ops).
};

// base_address is where the block will be placed (a KSEG0 address, 8-byte aligned).
std::optional<BkGfxBlock> bk_model_to_f3dex2(const BkModel& model, uint32_t base_address, std::string* error);

} // namespace jiggy
