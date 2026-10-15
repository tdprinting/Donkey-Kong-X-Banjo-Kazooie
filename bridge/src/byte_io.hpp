// Bounds-unchecked integer reads. Callers check ranges first.
#pragma once

#include <cstdint>
#include <span>

namespace jiggy {

inline uint32_t read_be32(std::span<const uint8_t> data, size_t offset) {
    return (uint32_t(data[offset]) << 24) | (uint32_t(data[offset + 1]) << 16) |
           (uint32_t(data[offset + 2]) << 8) | uint32_t(data[offset + 3]);
}

inline uint16_t read_be16(std::span<const uint8_t> data, size_t offset) {
    return uint16_t((data[offset] << 8) | data[offset + 1]);
}

inline uint32_t read_le32(std::span<const uint8_t> data, size_t offset) {
    return uint32_t(data[offset]) | (uint32_t(data[offset + 1]) << 8) |
           (uint32_t(data[offset + 2]) << 16) | (uint32_t(data[offset + 3]) << 24);
}

inline bool range_ok(std::span<const uint8_t> data, uint64_t offset, uint64_t size) {
    return offset <= data.size() && size <= data.size() - offset;
}

} // namespace jiggy
