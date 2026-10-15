#include "jiggy/bk_pose.hpp"

#include "byte_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace jiggy {

namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

// Geo commands (include/core2/model.h).
enum GeoCommand : uint32_t {
    kGeoBillboard = 0x00,
    kGeoSort = 0x01,
    kGeoBone = 0x02,
    kGeoLoadDl = 0x03,
    kGeoSkinning = 0x05,
    kGeoCall = 0x06,
    kGeoLoadDl2 = 0x07,
    kGeoLod = 0x08,
    kGeoSelector = 0x0C,
    kGeoDrawDistance = 0x0D,
    kGeoCullSphere = 0x0E,
    kGeoCamera = 0x0F,
};

constexpr uint32_t kCameraOutsideBit = 0x01;
constexpr size_t kMaxDepth = 64;
constexpr size_t kMaxCommands = 0x4000;

constexpr uint32_t G_MTX = 0xDA;
constexpr uint32_t G_POPMTX = 0xD8;
constexpr uint32_t G_DL = 0xDE;
constexpr uint32_t G_ENDDL = 0xDF;

float read_bef32(std::span<const uint8_t> data, size_t offset) {
    const uint32_t bits = read_be32(data, offset);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

class Walker {
public:
    Walker(const BkModel& model, const BkGfxBlock& block, uint32_t block_address, uint32_t root,
           std::span<const uint32_t> bones, const BkSelectors& selectors)
        : geo_(model.geo_list()), command_count_(model.commands().size()), block_(block),
          block_address_(block_address), root_(root), bones_(bones), selectors_(selectors) {
        stack_.push_back(root);
        loaded_ = root;
    }

    bool run(BkPoseList& out, std::string* error) {
        out_ = &out;
        emit((G_DL << 24), block_address_ + block_.prologue_offset);
        // gSPMatrix(root, G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW)
        emit((G_MTX << 24) | 0x380000 | 0x02, root_);
        if (!geo_.empty() && !walk(0, 0)) {
            set_error(error, error_);
            return false;
        }
        emit((G_POPMTX << 24) | 0x380002, 0x40); // gSPPopMatrix(G_MTX_MODELVIEW)
        emit((G_DL << 24), block_address_ + block_.epilogue_offset);
        emit(G_ENDDL << 24, 0);
        return true;
    }

private:
    void emit(uint32_t w0, uint32_t w1) { out_->commands.push_back({w0, w1}); }

    // gSPMatrix(m, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW), skipped when it's already
    // the current matrix.
    void load(uint32_t matrix) {
        if (matrix != loaded_) {
            emit((G_MTX << 24) | 0x380000 | 0x03, matrix);
            loaded_ = matrix;
        }
    }

    bool call(int32_t index) {
        if (index < 0 || size_t(index) >= command_count_) {
            error_ = "geo list calls a display list the model doesn't have";
            return false;
        }
        load(stack_.back());
        emit(G_DL << 24, block_address_ + block_.commands_offset + uint32_t(index) * 8);
        out_->sub_lists++;
        return true;
    }

    bool need(size_t at, size_t size) {
        if (!range_ok(geo_, at, size)) {
            error_ = "geo list runs past the end of the model";
            return false;
        }
        return true;
    }

    bool branch(size_t at, int64_t offset, size_t depth) {
        if (offset == 0) {
            return true;
        }
        const int64_t target = int64_t(at) + offset;
        if (target < 0) {
            error_ = "geo list branches outside the model";
            return false;
        }
        return walk(size_t(target), depth + 1);
    }

    bool walk(size_t at, size_t depth) {
        if (depth > kMaxDepth) {
            error_ = "geo list nests too deeply";
            return false;
        }
        while (true) {
            if (++visited_ > kMaxCommands) {
                error_ = "geo list is too long";
                return false;
            }
            if (!need(at, 8) || !command(at, depth)) {
                return false;
            }
            const auto next = int32_t(read_be32(geo_, at + 4));
            if (next == 0) {
                return true;
            }
            if (next < 0) {
                error_ = "geo list goes backwards";
                return false;
            }
            at += size_t(next);
        }
    }

    bool command(size_t at, size_t depth) {
        switch (read_be32(geo_, at)) {
            case kGeoBillboard:
                // Faces the camera in Banjo-Kazooie; drawn as modelled here.
                return need(at, 12) && branch(at, int16_t(read_be16(geo_, at + 8)), depth);
            case kGeoSort: {
                if (!need(at, 40)) {
                    return false;
                }
                return branch(at, int16_t(read_be16(geo_, at + 34)), depth) &&
                       branch(at, int32_t(read_be32(geo_, at + 36)), depth);
            }
            case kGeoBone: {
                if (!need(at, 10)) {
                    return false;
                }
                const uint8_t offset = geo_[at + 8];
                const auto id = int8_t(geo_[at + 9]);
                uint32_t matrix = root_;
                if (id >= 0) {
                    if (size_t(id) >= bones_.size()) {
                        error_ = "geo list names a bone the skeleton doesn't have";
                        return false;
                    }
                    matrix = bones_[size_t(id)];
                }
                stack_.push_back(matrix);
                const bool ok = branch(at, offset, depth);
                stack_.pop_back();
                return ok;
            }
            case kGeoLoadDl:
            case kGeoLoadDl2:
                return need(at, 10) && call(int16_t(read_be16(geo_, at + 8)));
            case kGeoSkinning: {
                // The first list pops to the enclosing bone's matrix and loads its vertices;
                // the rest load the current bone's vertices and join the two.
                const uint32_t outer = stack_.size() >= 2 ? stack_[stack_.size() - 2] : root_;
                for (size_t i = 0;; i++) {
                    if (!need(at + 8 + i * 2, 2)) {
                        return false;
                    }
                    const auto index = int16_t(read_be16(geo_, at + 8 + i * 2));
                    if (i > 0 && index == 0) {
                        return true;
                    }
                    if (i == 0) {
                        const uint32_t inner = stack_.back();
                        stack_.back() = outer;
                        const bool ok = call(index);
                        stack_.back() = inner;
                        if (!ok) {
                            return false;
                        }
                    } else if (!call(index)) {
                        return false;
                    }
                }
            }
            case kGeoCall:
                return need(at, 12) && branch(at, int32_t(read_be32(geo_, at + 8)), depth);
            case kGeoLod: {
                if (!need(at, 32)) {
                    return false;
                }
                // The nearest level of detail: what Banjo-Kazooie draws up close.
                const float max = read_bef32(geo_, at + 8);
                const float min = read_bef32(geo_, at + 12);
                constexpr float kNear = 1.0f;
                if (min < kNear && kNear <= max) {
                    return branch(at, int32_t(read_be32(geo_, at + 28)), depth);
                }
                return true;
            }
            case kGeoSelector: {
                if (!need(at, 12)) {
                    return false;
                }
                const auto count = int16_t(read_be16(geo_, at + 8));
                const auto index = int16_t(read_be16(geo_, at + 10));
                if (count < 0 || !need(at + 12, size_t(count) * 4)) {
                    return false;
                }
                if (index <= 0 || size_t(index) >= selectors_.size()) {
                    return true;
                }
                const int32_t selection = selectors_[size_t(index)];
                auto offset = [&](int i) { return int32_t(read_be32(geo_, at + 12 + size_t(i) * 4)); };
                if (selection > 0) {
                    return selection > count || branch(at, offset(selection - 1), depth);
                }
                uint32_t bits = uint32_t(-int64_t(selection));
                for (int i = 0; i < count && bits; i++, bits >>= 1) {
                    if ((bits & 1) && !branch(at, offset(i), depth)) {
                        return false;
                    }
                }
                return true;
            }
            case kGeoDrawDistance:
                return need(at, 22) && branch(at, int16_t(read_be16(geo_, at + 20)), depth);
            case kGeoCullSphere:
                return need(at, 18) && branch(at, int16_t(read_be16(geo_, at + 16)), depth);
            case kGeoCamera:
                if (!need(at, 12)) {
                    return false;
                }
                // Treated as outside every camera area.
                if (geo_[at + 11] & kCameraOutsideBit) {
                    return branch(at, int16_t(read_be16(geo_, at + 8)), depth);
                }
                return true;
            default:
                // No-ops, reference points (positions fed back to the game) and texture wrap
                // modes, which the converted lists don't need.
                return true;
        }
    }

    std::span<const uint8_t> geo_;
    size_t command_count_;
    const BkGfxBlock& block_;
    uint32_t block_address_;
    uint32_t root_;
    std::span<const uint32_t> bones_;
    const BkSelectors& selectors_;
    std::vector<uint32_t> stack_;
    uint32_t loaded_;
    size_t visited_ = 0;
    BkPoseList* out_ = nullptr;
    std::string error_;
};

} // namespace

std::optional<BkPoseList> bk_pose_display_list(const BkModel& model, const BkGfxBlock& block, uint32_t block_address,
                                               uint32_t root_matrix, std::span<const uint32_t> bone_matrices,
                                               const BkSelectors& selectors, std::string* error) {
    BkPoseList out;
    Walker walker(model, block, block_address, root_matrix, bone_matrices, selectors);
    if (!walker.run(out, error)) {
        return std::nullopt;
    }
    return out;
}

std::array<uint8_t, 64> bk_matrix_to_mtx(const std::array<std::array<float, 4>, 4>& m) {
    std::array<uint8_t, 64> out{};
    for (size_t row = 0; row < 4; row++) {
        for (size_t col = 0; col < 4; col++) {
            const double scaled = std::clamp(double(m[row][col]) * 65536.0, -2147483648.0, 2147483647.0);
            const auto fixed = uint32_t(int32_t(std::lround(scaled)));
            const size_t at = (row * 4 + col) * 2;
            out[at] = uint8_t(fixed >> 24);
            out[at + 1] = uint8_t(fixed >> 16);
            out[32 + at] = uint8_t(fixed >> 8);
            out[32 + at + 1] = uint8_t(fixed);
        }
    }
    return out;
}

} // namespace jiggy
