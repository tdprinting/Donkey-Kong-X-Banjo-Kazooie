// ROM loading and identification for the two games Jiggy Jungle is built from.
//
// The player supplies their own dumps of Banjo-Kazooie and Donkey Kong 64. Nothing
// here embeds game data: we only know the SHA-1 of the exact US 1.0 dumps the two
// recomp projects target, plus the header fields needed to tell the games apart.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy {

enum class Game {
    Unknown,
    BanjoKazooie,
    DonkeyKong64,
};

// N64 dumps circulate in three byte orders. Everything in the bridge works on the
// big-endian (.z64) order the console itself used.
enum class ByteOrder {
    Unknown,
    BigEndian,    // .z64: 80 37 12 40
    ByteSwapped,  // .v64: 37 80 40 12
    LittleEndian, // .n64: 40 12 37 80
};

struct RomInfo {
    Game game = Game::Unknown;
    ByteOrder original_order = ByteOrder::Unknown;
    std::string internal_name; // Header bytes 0x20..0x33, trailing spaces trimmed.
    std::string game_code;     // Header bytes 0x3B..0x3E, e.g. "NBKE".
    uint8_t revision = 0;      // Header byte 0x3F.
    std::string sha1;          // Lowercase hex SHA-1 of the big-endian image.
    // True only for the exact US 1.0 dump that BanjoRecomp / DK64 Recompiled target.
    // The asset offsets the bridge uses are only known to be right for those dumps.
    bool supported = false;
};

const char* game_name(Game game);
const char* byte_order_name(ByteOrder order);

ByteOrder detect_byte_order(std::span<const uint8_t> rom);

// Rewrites `rom` in place into big-endian order. Returns false if the byte order
// isn't recognised or the size isn't a multiple of 4.
bool normalize_to_big_endian(std::vector<uint8_t>& rom);

// Lowercase hex SHA-1 digest.
std::string sha1_hex(std::span<const uint8_t> data);

// Expected SHA-1 of the supported US 1.0 dumps (from the recomps' build guides).
inline constexpr const char* kBanjoKazooieUs10Sha1 = "1fe1632098865f639e22c11b9a81ee8f29c75d7a";
inline constexpr const char* kDonkeyKong64UsSha1 = "cf806ff2603640a748fca5026ded28802f1f4a50";

// Identifies a big-endian image. Call normalize_to_big_endian first.
RomInfo identify_big_endian(std::span<const uint8_t> rom, ByteOrder original_order);

// A ROM held in memory in big-endian order.
class Rom {
public:
    // Reads a .z64/.v64/.n64 file from disk and normalises it.
    static std::optional<Rom> load_file(const std::string& path, std::string* error);
    // Takes ownership of an in-memory image in any of the three byte orders.
    static std::optional<Rom> from_bytes(std::vector<uint8_t> bytes, std::string* error);

    const RomInfo& info() const { return info_; }
    std::span<const uint8_t> data() const { return data_; }

private:
    std::vector<uint8_t> data_;
    RomInfo info_;
};

} // namespace jiggy
