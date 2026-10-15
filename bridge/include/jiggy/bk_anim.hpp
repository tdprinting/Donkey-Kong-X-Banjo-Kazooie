// Banjo-Kazooie skeletons and skeletal animations: the bone list a model carries and the
// animation assets that move it, evaluated into one matrix per bone.
//
// Layout and maths follow the CC0 Banjo-Kazooie decompilation:
//
//   skeleton (model + 0x18, BKAnimationList in include/core2/model.h)
//     f32 scale, s16 count, u16 pad, then count x { f32 pivot[3]; s16 bone_id; s16 parent }
//     Entry i's matrix is built on its parent entry's (src/core2/code_630D0.c
//     animMtxList_setBoned).
//   animation (BKAnimationFileBin, include/core2/animationfile.h)
//     s16 first_frame, s16 last_frame, s16 track_count, u16 pad, then per track:
//     u16 bone_id:12 channel:4, s16 key_count, key_count x { u16 ease_in:1 ease_out:1
//     frame:14; s16 value (1/64ths) }. Channels 0-2 are rotation in degrees (pitch, yaw,
//     roll), 3-5 scale, 6-8 translation. Keys are interpolated linearly or with
//     Catmull-Rom splines (src/core2/animationfile.c).
//
// Matrices are row-vector 4x4s, like the N64's MtxF: a point p becomes p * M, and the
// translation is in row 3.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy {

using BkMatrix = std::array<std::array<float, 4>, 4>;

BkMatrix bk_matrix_identity();
// The transform that applies a, then b.
BkMatrix bk_matrix_multiply(const BkMatrix& a, const BkMatrix& b);

struct BkBone {
    std::array<float, 3> pivot{};
    uint16_t bone_id = 0; // Which animation track drives this entry.
    int16_t parent = -1;  // Entry index, -1 for none.
};

struct BkSkeleton {
    float scale = 1.0f; // Multiplies animated translations.
    std::vector<BkBone> bones;

    // Reads the skeleton of a model file. A model without one gives an empty skeleton.
    static std::optional<BkSkeleton> parse(std::span<const uint8_t> model, std::string* error);
};

struct BkAnimationKey {
    uint16_t frame = 0;
    bool ease_in = false;  // Spline from the previous key into this one.
    bool ease_out = false; // Spline from this key into the next one.
    int16_t value = 0;     // In 1/64ths.
};

struct BkAnimationTrack {
    uint16_t bone_id = 0;
    uint8_t channel = 0;
    std::vector<BkAnimationKey> keys;
};

// One bone's pose: what an animation sets for it.
struct BkBoneTransform {
    std::array<float, 3> rotation{};             // Degrees: pitch, yaw, roll.
    std::array<float, 3> scale{1.0f, 1.0f, 1.0f};
    std::array<float, 3> translation{};
};

class BkAnimation {
public:
    static std::optional<BkAnimation> parse(std::span<const uint8_t> data, std::string* error);

    int16_t first_frame() const { return first_frame_; }
    int16_t last_frame() const { return last_frame_; }
    const std::vector<BkAnimationTrack>& tracks() const { return tracks_; }

    // The pose at progress 0..1 through the animation, indexed by bone id. Bones the
    // animation doesn't move keep the rest transform.
    std::vector<BkBoneTransform> sample(float progress) const;

private:
    int16_t first_frame_ = 0;
    int16_t last_frame_ = 0;
    std::vector<BkAnimationTrack> tracks_;
};

// Each skeleton entry's model-space matrix for a pose (indexed by bone id, as
// BkAnimation::sample returns; missing bones are at rest). An empty pose gives the rest pose.
std::vector<BkMatrix> bk_skeleton_matrices(const BkSkeleton& skeleton, const std::vector<BkBoneTransform>& pose);

} // namespace jiggy
