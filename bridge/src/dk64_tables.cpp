#include "jiggy/dk64_tables.hpp"

#include "byte_io.hpp"
#include "jiggy/inflate.hpp"

namespace jiggy {

namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

constexpr uint32_t kSharedBit = 0x80000000;
constexpr uint32_t kOffsetMask = 0x7FFFFFFF;
constexpr uint32_t kMaxFilesPerTable = 0x10000;
// Redirects are one hop in practice; the limit only stops a corrupt table looping forever.
constexpr int kMaxRedirects = 16;

std::string file_label(uint32_t table, uint32_t index) {
    return "DK64 table " + std::to_string(table) + " file " + std::to_string(index);
}

} // namespace

const char* dk64_table_name(uint32_t table) {
    switch (table) {
        case 0: return "midi";
        case 1: return "map_geometry";
        case 2: return "map_walls";
        case 3: return "map_floors";
        case 4: return "prop_geometry";
        case 5: return "actor_geometry";
        case 7: return "textures_uncompressed";
        case 8: return "cutscenes";
        case 9: return "setup";
        case 10: return "scripts";
        case 11: return "animations";
        case 12: return "text";
        case 13: return "anim_code";
        case 14: return "textures_hud";
        case 15: return "paths";
        case 16: return "spawners";
        case 17: return "dktv";
        case 18: return "triggers";
        case 21: return "autowalks";
        case 22: return "critters";
        case 23: return "exits";
        case 24: return "checkpoints";
        case 25: return "textures_geometry";
        case 26: return "uncompressed_sizes";
        default: return "unused";
    }
}

std::optional<Dk64PointerTables> Dk64PointerTables::parse(std::span<const uint8_t> rom, uint32_t base,
                                                          std::string* error) {
    if (!range_ok(rom, base, kTableCount * 4)) {
        set_error(error, "DK64 pointer table header is outside the ROM");
        return std::nullopt;
    }

    Dk64PointerTables tables;
    tables.base_ = base;
    size_t populated = 0;
    for (uint32_t t = 0; t < kTableCount; t++) {
        const uint64_t table_offset = read_be32(rom, base + t * 4);
        const uint64_t table_start = uint64_t(base) + table_offset;
        if (!range_ok(rom, table_start, 4)) {
            continue;
        }
        // The first entry is where the first file begins, which is also where the table ends.
        const uint64_t first = read_be32(rom, table_start);
        if (first <= table_offset || (first - table_offset) % 4 != 0) {
            continue;
        }
        const uint64_t words = (first - table_offset) / 4;
        if (words < 2 || words > kMaxFilesPerTable + 1 || !range_ok(rom, table_start, words * 4)) {
            continue;
        }
        auto& entries = tables.entries_[t];
        entries.reserve(words);
        for (uint64_t i = 0; i < words; i++) {
            entries.push_back(read_be32(rom, table_start + i * 4));
        }
        populated++;
    }

    if (populated == 0) {
        set_error(error, "no valid DK64 pointer tables found");
        return std::nullopt;
    }
    return tables;
}

std::optional<Dk64FileEntry> Dk64PointerTables::resolve(std::span<const uint8_t> rom, uint32_t table, uint32_t index,
                                                        std::string* error) const {
    Dk64FileEntry out;
    out.table = table;
    out.index = index;

    uint32_t current = index;
    for (int hop = 0; hop <= kMaxRedirects; hop++) {
        if (current >= file_count(table)) {
            set_error(error, file_label(table, current) + " is out of range");
            return std::nullopt;
        }
        const auto& entries = entries_[table];
        const uint32_t word = entries[current];
        if (word & kSharedBit) {
            const uint64_t at = uint64_t(base_) + (word & kOffsetMask);
            if (!range_ok(rom, at, 2)) {
                set_error(error, file_label(table, current) + " redirects outside the ROM");
                return std::nullopt;
            }
            current = read_be16(rom, at);
            continue;
        }
        const uint32_t next = entries[current + 1] & kOffsetMask;
        const uint64_t start = uint64_t(base_) + word;
        if (next < word || !range_ok(rom, start, next - word)) {
            set_error(error, file_label(table, current) + " has an invalid offset");
            return std::nullopt;
        }
        out.resolved_index = current;
        out.rom_offset = uint32_t(start);
        out.stored_size = next - word;
        return out;
    }
    set_error(error, file_label(table, index) + " has a redirect loop");
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> Dk64PointerTables::load(std::span<const uint8_t> rom, uint32_t table,
                                                             uint32_t index, std::string* error) const {
    auto entry = resolve(rom, table, index, error);
    if (!entry) {
        return std::nullopt;
    }
    const auto stored = rom.subspan(entry->rom_offset, entry->stored_size);
    if (!is_gzip(stored)) {
        return std::vector<uint8_t>(stored.begin(), stored.end());
    }
    std::string inflate_error;
    auto out = inflate_gzip(stored, &inflate_error);
    if (!out) {
        set_error(error, file_label(table, index) + ": " + inflate_error);
    }
    return out;
}

} // namespace jiggy
