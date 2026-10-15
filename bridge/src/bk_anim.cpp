#include "jiggy/bk_anim.hpp"

#include "byte_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace jiggy {

namespace {

constexpr size_t kModelHeaderSize = 0x38;
constexpr size_t kSkeletonOffsetField = 0x18;
constexpr size_t kBoneSize = 0x10;
constexpr size_t kChannelCount = 9;
constexpr float kPi = 3.14159265358979323846f;

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

float read_bef32(std::span<const uint8_t> data, size_t offset) {
    const uint32_t bits = read_be32(data, offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

// glspline_catmull_rom_interpolate with four knots: the curve from knots[1] to knots[2].
float catmull_rom(float x, const std::array<float, 4>& k) {
    x = std::clamp(x, 0.0f, 1.0f);
    const float c3 = -0.5f * k[0] + 1.5f * k[1] - 1.5f * k[2] + 0.5f * k[3];
    const float c2 = k[0] - 2.5f * k[1] + 2.0f * k[2] - 0.5f * k[3];
    const float c1 = -0.5f * k[0] + 0.5f * k[2];
    return ((c3 * x + c2) * x + c1) * x + k[1];
}

// The rest value of each channel (D_803709E0).
constexpr std::array<float, kChannelCount> kRestValues = {0, 0, 0, 1, 1, 1, 0, 0, 0};

// animationfilebin_func_8033AC38: a track's value at a frame.
float track_value(const BkAnimationTrack& track, int16_t first_frame, float time) {
    const auto& keys = track.keys;
    const auto value = [](const BkAnimationKey& key) { return float(key.value) / 64.0f; };
    const BkAnimationKey& first = keys.front();
    const BkAnimationKey& last = keys.back();

    if (int(time) < first.frame) {
        std::array<float, 4> k;
        k[0] = k[1] = kRestValues[track.channel];
        k[2] = value(first);
        k[3] = (first.ease_in && keys.size() >= 2) ? value(keys[1]) : k[2];
        const float span = float(first.frame - first_frame);
        return catmull_rom(span > 0 ? (time - first_frame) / span : 1.0f, k);
    }
    if (int(time) >= last.frame) {
        std::array<float, 4> k;
        k[1] = value(last);
        k[0] = (last.ease_out && keys.size() >= 2) ? value(keys[keys.size() - 2]) : k[1];
        k[2] = k[3] = k[1];
        return catmull_rom(time - last.frame, k);
    }

    // The keys either side of time.
    size_t lo = 0;
    size_t hi = keys.size() - 1;
    while (lo + 1 < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (keys[mid].frame <= int(time)) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    const BkAnimationKey& a = keys[lo];
    const BkAnimationKey& b = keys[hi];
    const float t = (time - a.frame) / float(b.frame - a.frame);
    if (!a.ease_out && !b.ease_in) {
        return value(a) + (value(b) - value(a)) * t;
    }
    std::array<float, 4> k;
    k[1] = value(a);
    k[2] = value(b);
    k[0] = (a.ease_out && lo > 0) ? value(keys[lo - 1]) : k[1];
    k[3] = (b.ease_in && hi + 1 < keys.size()) ? value(keys[hi + 1]) : k[2];
    return catmull_rom(t, k);
}

// The rotation mlMtxRotRoll, mlMtxRotYaw then mlMtxRotPitch build from the identity
// (func_80345CD4): pitch is applied to a point first, then yaw, then roll.
BkMatrix rotation_matrix(const std::array<float, 3>& degrees) {
    const float to_radians = kPi / 180.0f;
    const float sx = std::sin(degrees[0] * to_radians), cx = std::cos(degrees[0] * to_radians);
    const float sy = std::sin(degrees[1] * to_radians), cy = std::cos(degrees[1] * to_radians);
    const float sz = std::sin(degrees[2] * to_radians), cz = std::cos(degrees[2] * to_radians);
    BkMatrix pitch = bk_matrix_identity();
    pitch[1] = {0, cx, sx, 0};
    pitch[2] = {0, -sx, cx, 0};
    BkMatrix yaw = bk_matrix_identity();
    yaw[0] = {cy, 0, -sy, 0};
    yaw[2] = {sy, 0, cy, 0};
    BkMatrix roll = bk_matrix_identity();
    roll[0] = {cz, sz, 0, 0};
    roll[1] = {-sz, cz, 0, 0};
    return bk_matrix_multiply(bk_matrix_multiply(pitch, yaw), roll);
}

BkMatrix translation_matrix(float x, float y, float z) {
    BkMatrix m = bk_matrix_identity();
    m[3] = {x, y, z, 1};
    return m;
}

} // namespace

BkMatrix bk_matrix_identity() {
    BkMatrix m{};
    for (size_t i = 0; i < 4; i++) {
        m[i][i] = 1.0f;
    }
    return m;
}

BkMatrix bk_matrix_multiply(const BkMatrix& a, const BkMatrix& b) {
    BkMatrix out{};
    for (size_t row = 0; row < 4; row++) {
        for (size_t col = 0; col < 4; col++) {
            float sum = 0;
            for (size_t k = 0; k < 4; k++) {
                sum += a[row][k] * b[k][col];
            }
            out[row][col] = sum;
        }
    }
    return out;
}

std::optional<BkSkeleton> BkSkeleton::parse(std::span<const uint8_t> model, std::string* error) {
    if (model.size() < kModelHeaderSize) {
        set_error(error, "model is too small");
        return std::nullopt;
    }
    BkSkeleton skeleton;
    const uint32_t at = read_be32(model, kSkeletonOffsetField);
    if (at == 0) {
        return skeleton;
    }
    if (!range_ok(model, at, 8)) {
        set_error(error, "skeleton is outside the model");
        return std::nullopt;
    }
    skeleton.scale = read_bef32(model, at);
    const auto count = int16_t(read_be16(model, at + 4));
    if (count < 0 || !range_ok(model, at + 8, size_t(count) * kBoneSize)) {
        set_error(error, "skeleton runs past the end of the model");
        return std::nullopt;
    }
    for (int i = 0; i < count; i++) {
        const size_t bone_at = at + 8 + size_t(i) * kBoneSize;
        BkBone bone;
        for (size_t k = 0; k < 3; k++) {
            bone.pivot[k] = read_bef32(model, bone_at + k * 4);
        }
        bone.bone_id = read_be16(model, bone_at + 12);
        bone.parent = int16_t(read_be16(model, bone_at + 14));
        // Entries are built in order on top of their parent's matrix.
        if (bone.parent >= i) {
            set_error(error, "skeleton entry comes before its parent");
            return std::nullopt;
        }
        skeleton.bones.push_back(bone);
    }
    return skeleton;
}

std::optional<BkAnimation> BkAnimation::parse(std::span<const uint8_t> data, std::string* error) {
    if (data.size() < 8) {
        set_error(error, "animation is too small");
        return std::nullopt;
    }
    BkAnimation animation;
    animation.first_frame_ = int16_t(read_be16(data, 0));
    animation.last_frame_ = int16_t(read_be16(data, 2));
    const auto count = int16_t(read_be16(data, 4));
    if (count < 0 || animation.last_frame_ < animation.first_frame_) {
        set_error(error, "animation header is not valid");
        return std::nullopt;
    }
    size_t at = 8;
    for (int i = 0; i < count; i++) {
        if (!range_ok(data, at, 4)) {
            set_error(error, "animation runs past the end of the file");
            return std::nullopt;
        }
        BkAnimationTrack track;
        const uint16_t id = read_be16(data, at);
        track.bone_id = id >> 4;
        track.channel = id & 0x0F;
        const auto keys = int16_t(read_be16(data, at + 2));
        at += 4;
        if (keys <= 0 || track.channel >= kChannelCount || !range_ok(data, at, size_t(keys) * 4)) {
            set_error(error, "animation track is not valid");
            return std::nullopt;
        }
        for (int k = 0; k < keys; k++) {
            const uint16_t packed = read_be16(data, at);
            BkAnimationKey key;
            key.ease_in = (packed & 0x8000) != 0;
            key.ease_out = (packed & 0x4000) != 0;
            key.frame = packed & 0x3FFF;
            key.value = int16_t(read_be16(data, at + 2));
            if (!track.keys.empty() && key.frame <= track.keys.back().frame) {
                set_error(error, "animation keys are out of order");
                return std::nullopt;
            }
            track.keys.push_back(key);
            at += 4;
        }
        animation.tracks_.push_back(std::move(track));
    }
    return animation;
}

std::vector<BkBoneTransform> BkAnimation::sample(float progress) const {
    uint16_t max_bone = 0;
    for (const auto& track : tracks_) {
        max_bone = std::max(max_bone, track.bone_id);
    }
    std::vector<BkBoneTransform> pose(tracks_.empty() ? 0 : size_t(max_bone) + 1);
    progress = std::clamp(progress, 0.0f, 1.0f);
    const float time = first_frame_ + progress * float(last_frame_ - first_frame_);
    for (const auto& track : tracks_) {
        const float v = track_value(track, first_frame_, time);
        auto& bone = pose[track.bone_id];
        if (track.channel < 3) {
            bone.rotation[track.channel] = v;
        } else if (track.channel < 6) {
            bone.scale[track.channel - 3] = v;
        } else {
            bone.translation[track.channel - 6] = v;
        }
    }
    return pose;
}

std::vector<BkMatrix> bk_skeleton_matrices(const BkSkeleton& skeleton, const std::vector<BkBoneTransform>& pose) {
    std::vector<BkMatrix> out;
    out.reserve(skeleton.bones.size());
    const BkBoneTransform rest;
    for (const auto& bone : skeleton.bones) {
        const BkBoneTransform& t = bone.bone_id < pose.size() ? pose[bone.bone_id] : rest;
        const auto& p = bone.pivot;
        // animMtxList_setBoned: about the pivot, scale then rotate, then move by the animated
        // translation, then the parent's matrix.
        BkMatrix m = translation_matrix(-p[0], -p[1], -p[2]);
        BkMatrix scale = bk_matrix_identity();
        scale[0][0] = t.scale[0];
        scale[1][1] = t.scale[1];
        scale[2][2] = t.scale[2];
        m = bk_matrix_multiply(m, scale);
        m = bk_matrix_multiply(m, rotation_matrix(t.rotation));
        m = bk_matrix_multiply(m, translation_matrix(p[0] + skeleton.scale * t.translation[0],
                                                     p[1] + skeleton.scale * t.translation[1],
                                                     p[2] + skeleton.scale * t.translation[2]));
        if (bone.parent >= 0) {
            m = bk_matrix_multiply(m, out[size_t(bone.parent)]);
        }
        out.push_back(m);
    }
    return out;
}

} // namespace jiggy
