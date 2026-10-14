// Donkey Kong 64's pointer tables: the 32 file banks (music, map geometry, actor models,
// textures, text, setups...) the game loads through getPointerTableFile().
//
// Layout (from the CC0 DK64 decompilation, src/global_asm/code_6AF80.c
// func_global_asm_8066ADA0 / 8066B5F4 / 8066B4D4; `romAssetBin` in symbol_addrs.us.txt):
//
//   base + 0x0   32 x u32: offset of each table, relative to base
//   table t      u32 entries, relative to base. The first entry also marks where the table
//                ends, so the table holds (entry[0] - table_offset) / 4 words, the last of
//                which is a terminator. File i spans entry[i] .. entry[i + 1] (low 31 bits).
//   If bit 31 of an entry is set, the file is shared: the first u16 at that offset is the
//   index of another file in the same table to use instead.
//   Files in compressed tables are standard gzip members.
//
// In the US ROM `base` is 0x101C50. DK64 Recompiled runs from the retail image unchanged,
// so the same offset applies to the ROM the player supplies.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy {

// Names from the decompilation's pointertable_e enum.
enum class Dk64Table : uint32_t {
    Midi = 0,
    MapGeometry = 1,
    MapWalls = 2,
    MapFloors = 3,
    PropGeometry = 4,
    ActorGeometry = 5,
    TexturesUncompressed = 7,
    Cutscenes = 8,
    Setup = 9,
    Scripts = 10,
    Animations = 11,
    Text = 12,
    AnimCode = 13,
    TexturesHud = 14,
    Paths = 15,
    Spawners = 16,
    Dktv = 17,
    Triggers = 18,
    Autowalks = 21,
    Critters = 22,
    Exits = 23,
    Checkpoints = 24,
    TexturesGeometry = 25,
    UncompressedSizes = 26,
};

const char* dk64_table_name(uint32_t table);

struct Dk64FileEntry {
    uint32_t table = 0;
    uint32_t index = 0;          // Index that was asked for.
    uint32_t resolved_index = 0; // Index the data actually came from after following redirects.
    uint32_t rom_offset = 0;     // Absolute ROM offset.
    uint32_t stored_size = 0;
};

class Dk64PointerTables {
public:
    static constexpr uint32_t kUsRomOffset = 0x101C50;
    static constexpr size_t kTableCount = 32;

    static std::optional<Dk64PointerTables> parse(std::span<const uint8_t> rom, uint32_t base, std::string* error);

    size_t file_count(uint32_t table) const {
        return table < kTableCount && !entries_[table].empty() ? entries_[table].size() - 1 : 0;
    }

    // Follows shared-file redirects and returns where the file's bytes live.
    std::optional<Dk64FileEntry> resolve(std::span<const uint8_t> rom, uint32_t table, uint32_t index,
                                         std::string* error) const;

    // Returns the file's bytes, inflating gzip members. Empty files return an empty vector.
    std::optional<std::vector<uint8_t>> load(std::span<const uint8_t> rom, uint32_t table, uint32_t index,
                                             std::string* error) const;

private:
    uint32_t base_ = 0;
    // Raw entry words for each table, terminator included.
    std::array<std::vector<uint32_t>, kTableCount> entries_{};
};

} // namespace jiggy
