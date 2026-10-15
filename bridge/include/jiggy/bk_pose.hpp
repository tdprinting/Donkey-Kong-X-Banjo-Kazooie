// Drawing a Banjo-Kazooie model in a pose: walks the model's geo list the way Banjo-Kazooie's
// modelRender_executeGeoCmds does and writes an F3DEX2 display list that loads each bone's
// matrix and calls the model's sub-lists in a converted block (bk_gfx.hpp).
//
// The matrices are the caller's: one Mtx per skeleton entry, already including where the
// model stands in the world, plus a root matrix for geometry outside any bone. The list
// pushes the root matrix and pops it at the end, so the caller's modelview is untouched.
//
// What depends on the camera in Banjo-Kazooie is fixed here: the nearest level of detail
// is drawn, both halves of a depth sort are drawn in order, and draw-distance and camera
// area checks always pass.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "jiggy/bk_gfx.hpp"
#include "jiggy/bk_model.hpp"

namespace jiggy {

// Banjo-Kazooie's appendage table (modelRender_setAppendageVisibility): picks branches of
// the geo list's selectors. 0 hides a branch, n > 0 draws branch n, -bits draws several.
using BkSelectors = std::array<int32_t, 0x2A>;

struct BkPoseList {
    std::vector<std::array<uint32_t, 2>> commands; // F3DEX2, ends with G_ENDDL.
    size_t sub_lists = 0;                           // Model sub-lists called.
};

// block_address is where `block` is in N64 memory; bone_matrices[i] is the address of
// skeleton entry i's Mtx.
std::optional<BkPoseList> bk_pose_display_list(const BkModel& model, const BkGfxBlock& block, uint32_t block_address,
                                               uint32_t root_matrix, std::span<const uint32_t> bone_matrices,
                                               const BkSelectors& selectors, std::string* error);

// The N64 fixed-point Mtx (guMtxF2L) of a row-vector matrix, as 64 big-endian bytes.
std::array<uint8_t, 64> bk_matrix_to_mtx(const std::array<std::array<float, 4>, 4>& m);

} // namespace jiggy
