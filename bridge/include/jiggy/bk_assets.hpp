// Banjo-Kazooie's asset bank: every model, animation, sprite, level setup and dialogue
// file the game loads through assetcache_get().
//
// Layout (from the CC0 Banjo-Kazooie decompilation, include/assets.h and
// src/core2/code_B3A80.c assetCache_init / assetcache_get):
//
//   base + 0x0   u32 count, u32 unused
//   base + 0x8   count x { u32 offset; s16 compFlag; s16 type; }
//   data_base = base + 8 + count * 8
//   asset i lives at data_base + entry[i].offset, size entry[i + 1].offset - entry[i].offset
//   (the last entry is a terminator, so there are count - 1 real assets)
//   compFlag & 1 means the data is rarezip-compressed.
//
// In the US 1.0 ROM the bank starts at 0x5E90 in both the retail and decompressed images,
// because BanjoRecomp's decompressor copies everything before the code overlays verbatim.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy {

struct BkAssetEntry {
    uint32_t rom_offset = 0;  // Absolute ROM offset of the stored data.
    uint32_t stored_size = 0; // Bytes in ROM (compressed size when compressed).
    bool compressed = false;
    uint16_t flags = 0;       // Raw compFlag field.
    uint16_t type = 0;        // Raw second field; meaning not fully documented yet.
    bool valid = true;        // False when the table points outside the ROM.
};

// Name from the decompilation's asset_e enum (e.g. "MODEL_BANJOKAZOOIE_HIGH_POLY"),
// or nullptr for assets nobody has named yet.
const char* bk_asset_name(uint32_t id);

class BkAssetTable {
public:
    static constexpr uint32_t kUs10RomOffset = 0x5E90;

    static std::optional<BkAssetTable> parse(std::span<const uint8_t> rom, uint32_t base, std::string* error);

    // Number of real assets (the terminator entry is excluded).
    size_t size() const { return entries_.size(); }
    const BkAssetEntry& entry(size_t id) const { return entries_[id]; }

    // Size of the asset once decompressed, read from the rarezip header when compressed.
    std::optional<uint32_t> uncompressed_size(std::span<const uint8_t> rom, size_t id, std::string* error) const;

    // Returns the asset's bytes, decompressed when needed. Empty (stub) assets return an empty vector.
    std::optional<std::vector<uint8_t>> load(std::span<const uint8_t> rom, size_t id, std::string* error) const;

private:
    std::vector<BkAssetEntry> entries_;
};

} // namespace jiggy
