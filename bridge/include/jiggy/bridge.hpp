// The Jiggy Bridge holds both of the player's ROMs and serves Banjo-Kazooie and
// Donkey Kong 64 files to the game. The DK64 Recompiled host links this library and
// exposes it to the recompiled game code as recomp imports (see bridge.h for the C API).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "jiggy/bk_assets.hpp"
#include "jiggy/dk64_tables.hpp"
#include "jiggy/rom.hpp"

namespace jiggy {

struct BridgeOptions {
    // Refuse ROMs whose SHA-1 isn't the US 1.0 dump the asset offsets were taken from.
    // Only tests turn this off, to run against synthetic images.
    bool require_known_dumps = true;
    uint32_t bk_asset_base = BkAssetTable::kUs10RomOffset;
    uint32_t dk64_table_base = Dk64PointerTables::kUsRomOffset;
};

class Bridge {
public:
    static std::unique_ptr<Bridge> open(Rom banjo, Rom dk64, const BridgeOptions& options, std::string* error);

    const Rom& banjo_rom() const { return banjo_; }
    const Rom& dk64_rom() const { return dk64_; }
    const BkAssetTable& bk_assets() const { return bk_assets_; }
    const Dk64PointerTables& dk64_tables() const { return dk64_tables_; }

    std::optional<std::vector<uint8_t>> load_bk_asset(uint32_t id, std::string* error) const {
        return bk_assets_.load(banjo_.data(), id, error);
    }
    std::optional<uint32_t> bk_asset_size(uint32_t id, std::string* error) const {
        return bk_assets_.uncompressed_size(banjo_.data(), id, error);
    }
    std::optional<std::vector<uint8_t>> load_dk64_file(uint32_t table, uint32_t index, std::string* error) const {
        return dk64_tables_.load(dk64_.data(), table, index, error);
    }

private:
    Bridge(Rom banjo, Rom dk64, BkAssetTable bk_assets, Dk64PointerTables dk64_tables)
        : banjo_(std::move(banjo)), dk64_(std::move(dk64)), bk_assets_(std::move(bk_assets)),
          dk64_tables_(std::move(dk64_tables)) {}

    Rom banjo_;
    Rom dk64_;
    BkAssetTable bk_assets_;
    Dk64PointerTables dk64_tables_;
};

} // namespace jiggy
