// Decompressors for the two compression wrappers Rare used.
//
// Both games store DEFLATE streams; only the framing differs:
//   * Banjo-Kazooie "rarezip": 11 72, a big-endian u32 uncompressed size, then raw DEFLATE.
//   * Donkey Kong 64: a standard gzip member (RFC 1952).
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy {

inline constexpr uint8_t kRarezipMagic0 = 0x11;
inline constexpr uint8_t kRarezipMagic1 = 0x72;
inline constexpr size_t kRarezipHeaderSize = 6;

bool is_rarezip(std::span<const uint8_t> data);
bool is_gzip(std::span<const uint8_t> data);

// Uncompressed size stored in a rarezip header, if the header is present.
std::optional<uint32_t> rarezip_uncompressed_size(std::span<const uint8_t> data);

// Raw DEFLATE (no zlib or gzip framing). Trailing bytes after the final block are ignored.
std::optional<std::vector<uint8_t>> inflate_raw(std::span<const uint8_t> data, std::string* error);

std::optional<std::vector<uint8_t>> inflate_rarezip(std::span<const uint8_t> data, std::string* error);

// Verifies the gzip CRC-32 and size trailer. Padding after the member is ignored.
std::optional<std::vector<uint8_t>> inflate_gzip(std::span<const uint8_t> data, std::string* error);

} // namespace jiggy
