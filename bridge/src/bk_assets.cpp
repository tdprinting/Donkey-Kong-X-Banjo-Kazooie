#include "jiggy/bk_assets.hpp"

#include <algorithm>
#include <iterator>

#include "byte_io.hpp"
#include "jiggy/inflate.hpp"

namespace jiggy {

namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

// The real table has a few thousand entries; this only rejects garbage.
constexpr uint32_t kMaxEntries = 0x10000;

struct AssetName {
    uint32_t id;
    const char* name;
};

constexpr AssetName kAssetNames[] = {
#include "bk_asset_names.inc"
};

} // namespace

const char* bk_asset_name(uint32_t id) {
    const auto it = std::lower_bound(std::begin(kAssetNames), std::end(kAssetNames), id,
                                     [](const AssetName& entry, uint32_t value) { return entry.id < value; });
    return (it != std::end(kAssetNames) && it->id == id) ? it->name : nullptr;
}

std::optional<BkAssetTable> BkAssetTable::parse(std::span<const uint8_t> rom, uint32_t base, std::string* error) {
    if (!range_ok(rom, base, 8)) {
        set_error(error, "Banjo-Kazooie asset table header is outside the ROM");
        return std::nullopt;
    }
    const uint32_t count = read_be32(rom, base);
    if (count < 1 || count > kMaxEntries) {
        set_error(error, "Banjo-Kazooie asset table has an implausible entry count");
        return std::nullopt;
    }
    const uint64_t table_start = uint64_t(base) + 8;
    const uint64_t data_base = table_start + uint64_t(count) * 8;
    if (!range_ok(rom, table_start, uint64_t(count) * 8)) {
        set_error(error, "Banjo-Kazooie asset table runs past the end of the ROM");
        return std::nullopt;
    }

    BkAssetTable table;
    table.entries_.reserve(count - 1);
    for (uint32_t i = 0; i + 1 < count; i++) {
        const uint64_t at = table_start + uint64_t(i) * 8;
        const uint32_t offset = read_be32(rom, at);
        const uint32_t next = read_be32(rom, at + 8);
        BkAssetEntry entry;
        // Keep going past a bad entry so one odd slot doesn't hide the rest of the bank.
        entry.valid = next >= offset && range_ok(rom, data_base + offset, next - offset);
        if (entry.valid) {
            entry.rom_offset = uint32_t(data_base + offset);
            entry.stored_size = next - offset;
        }
        entry.flags = read_be16(rom, at + 4);
        entry.type = read_be16(rom, at + 6);
        entry.compressed = (entry.flags & 1) != 0;
        table.entries_.push_back(entry);
    }
    return table;
}

std::optional<uint32_t> BkAssetTable::uncompressed_size(std::span<const uint8_t> rom, size_t id,
                                                        std::string* error) const {
    if (id >= entries_.size()) {
        set_error(error, "Banjo-Kazooie asset id out of range");
        return std::nullopt;
    }
    const BkAssetEntry& e = entries_[id];
    if (!e.valid) {
        set_error(error, "Banjo-Kazooie asset " + std::to_string(id) + " has an invalid offset");
        return std::nullopt;
    }
    if (!e.compressed) {
        return e.stored_size;
    }
    auto size = rarezip_uncompressed_size(rom.subspan(e.rom_offset, e.stored_size));
    if (!size) {
        set_error(error, "Banjo-Kazooie asset " + std::to_string(id) + " is flagged compressed but has no rarezip header");
    }
    return size;
}

std::optional<std::vector<uint8_t>> BkAssetTable::load(std::span<const uint8_t> rom, size_t id,
                                                       std::string* error) const {
    if (id >= entries_.size()) {
        set_error(error, "Banjo-Kazooie asset id out of range");
        return std::nullopt;
    }
    const BkAssetEntry& e = entries_[id];
    if (!e.valid) {
        set_error(error, "Banjo-Kazooie asset " + std::to_string(id) + " has an invalid offset");
        return std::nullopt;
    }
    const auto stored = rom.subspan(e.rom_offset, e.stored_size);
    if (!e.compressed) {
        return std::vector<uint8_t>(stored.begin(), stored.end());
    }
    std::string inflate_error;
    auto out = inflate_rarezip(stored, &inflate_error);
    if (!out) {
        set_error(error, "Banjo-Kazooie asset " + std::to_string(id) + ": " + inflate_error);
    }
    return out;
}

} // namespace jiggy
