#include "jiggy/bridge.hpp"

#include <algorithm>
#include <cstring>

#include "jiggy/bridge.h"

namespace jiggy {

namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

bool check_rom(const Rom& rom, Game expected, const char* expected_sha1, const BridgeOptions& options,
               std::string* error) {
    const RomInfo& info = rom.info();
    if (info.game != expected) {
        set_error(error, std::string("expected a ") + game_name(expected) + " ROM but got " +
                             (info.game == Game::Unknown ? "an unrecognised ROM" : game_name(info.game)) + " (" +
                             info.internal_name + ")");
        return false;
    }
    if (options.require_known_dumps && !info.supported) {
        set_error(error, std::string(game_name(expected)) + " ROM is not a supported US dump (SHA-1 " + info.sha1 +
                             ", expected " + expected_sha1 + ")");
        return false;
    }
    return true;
}

} // namespace

std::unique_ptr<Bridge> Bridge::open(Rom banjo, Rom dk64, const BridgeOptions& options, std::string* error) {
    if (!check_rom(banjo, Game::BanjoKazooie, kBanjoKazooieUs10Sha1, options, error) ||
        !check_rom(dk64, Game::DonkeyKong64, kDonkeyKong64UsSha1, options, error)) {
        return nullptr;
    }
    auto bk_assets = BkAssetTable::parse(banjo.data(), options.bk_asset_base, error);
    if (!bk_assets) {
        return nullptr;
    }
    auto dk64_tables = Dk64PointerTables::parse(dk64.data(), options.dk64_table_base, error);
    if (!dk64_tables) {
        return nullptr;
    }
    return std::unique_ptr<Bridge>(
        new Bridge(std::move(banjo), std::move(dk64), std::move(*bk_assets), std::move(*dk64_tables)));
}

} // namespace jiggy

struct JiggyBridge {
    std::unique_ptr<jiggy::Bridge> bridge;
};

namespace {

void copy_error(const std::string& message, char* error, size_t error_len) {
    if (error && error_len > 0) {
        const size_t n = std::min(message.size(), error_len - 1);
        std::memcpy(error, message.data(), n);
        error[n] = '\0';
    }
}

JiggyBridge* finish_open(std::optional<jiggy::Rom> banjo, std::optional<jiggy::Rom> dk64, std::string& message,
                         char* error, size_t error_len) {
    if (!banjo || !dk64) {
        copy_error(message, error, error_len);
        return nullptr;
    }
    auto bridge = jiggy::Bridge::open(std::move(*banjo), std::move(*dk64), {}, &message);
    if (!bridge) {
        copy_error(message, error, error_len);
        return nullptr;
    }
    return new JiggyBridge{std::move(bridge)};
}

int64_t copy_out(const std::optional<std::vector<uint8_t>>& data, void* dst, size_t dst_len) {
    if (!data) {
        return -1;
    }
    if (dst && dst_len >= data->size() && !data->empty()) {
        std::memcpy(dst, data->data(), data->size());
    }
    return int64_t(data->size());
}

} // namespace

extern "C" {

JiggyBridge* jiggy_bridge_open(const char* banjo_rom_path, const char* dk64_rom_path, char* error, size_t error_len) {
    std::string message;
    if (!banjo_rom_path || !dk64_rom_path) {
        copy_error("both ROM paths are required", error, error_len);
        return nullptr;
    }
    auto banjo = jiggy::Rom::load_file(banjo_rom_path, &message);
    std::optional<jiggy::Rom> dk64;
    if (banjo) {
        dk64 = jiggy::Rom::load_file(dk64_rom_path, &message);
    }
    return finish_open(std::move(banjo), std::move(dk64), message, error, error_len);
}

JiggyBridge* jiggy_bridge_open_memory(const void* banjo_rom, size_t banjo_len, const void* dk64_rom, size_t dk64_len,
                                      char* error, size_t error_len) {
    std::string message;
    if (!banjo_rom || !dk64_rom) {
        copy_error("both ROM images are required", error, error_len);
        return nullptr;
    }
    const auto* banjo_bytes = static_cast<const uint8_t*>(banjo_rom);
    const auto* dk64_bytes = static_cast<const uint8_t*>(dk64_rom);
    auto banjo = jiggy::Rom::from_bytes({banjo_bytes, banjo_bytes + banjo_len}, &message);
    std::optional<jiggy::Rom> dk64;
    if (banjo) {
        dk64 = jiggy::Rom::from_bytes({dk64_bytes, dk64_bytes + dk64_len}, &message);
    }
    return finish_open(std::move(banjo), std::move(dk64), message, error, error_len);
}

void jiggy_bridge_close(JiggyBridge* bridge) {
    delete bridge;
}

uint32_t jiggy_bk_asset_count(const JiggyBridge* bridge) {
    return bridge ? uint32_t(bridge->bridge->bk_assets().size()) : 0;
}

int64_t jiggy_bk_asset_read(const JiggyBridge* bridge, uint32_t asset_id, void* dst, size_t dst_len) {
    if (!bridge) {
        return -1;
    }
    if (!dst) {
        // Size queries don't need to inflate: compressed assets carry their size in the header.
        auto size = bridge->bridge->bk_asset_size(asset_id, nullptr);
        return size ? int64_t(*size) : -1;
    }
    return copy_out(bridge->bridge->load_bk_asset(asset_id, nullptr), dst, dst_len);
}

uint32_t jiggy_dk64_file_count(const JiggyBridge* bridge, uint32_t table) {
    return bridge ? uint32_t(bridge->bridge->dk64_tables().file_count(table)) : 0;
}

int64_t jiggy_dk64_file_read(const JiggyBridge* bridge, uint32_t table, uint32_t index, void* dst, size_t dst_len) {
    if (!bridge) {
        return -1;
    }
    return copy_out(bridge->bridge->load_dk64_file(table, index, nullptr), dst, dst_len);
}

} // extern "C"
