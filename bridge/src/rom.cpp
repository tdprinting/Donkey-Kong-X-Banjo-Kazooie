#include "jiggy/rom.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <utility>

namespace jiggy {

namespace {

// Largest retail N64 cartridge is 64 MiB; anything bigger isn't a ROM.
constexpr size_t kMaxRomSize = 64 * 1024 * 1024;
constexpr size_t kHeaderSize = 0x40;

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

std::string header_string(std::span<const uint8_t> rom, size_t offset, size_t length) {
    std::string out;
    for (size_t i = 0; i < length; i++) {
        const uint8_t c = rom[offset + i];
        out.push_back((c >= 0x20 && c < 0x7F) ? char(c) : ' ');
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

// Plain FIPS 180-1 SHA-1, only used to recognise known dumps.
class Sha1 {
public:
    void update(std::span<const uint8_t> data) {
        for (uint8_t byte : data) {
            block_[block_len_++] = byte;
            if (block_len_ == 64) {
                process_block();
                block_len_ = 0;
            }
        }
        total_bits_ += uint64_t(data.size()) * 8;
    }

    std::array<uint8_t, 20> finish() {
        const uint64_t total_bits = total_bits_;
        const uint8_t pad_start = 0x80;
        update({&pad_start, 1});
        const uint8_t zero = 0;
        while (block_len_ != 56) {
            update({&zero, 1});
        }
        std::array<uint8_t, 8> length{};
        for (int i = 0; i < 8; i++) {
            length[i] = uint8_t(total_bits >> (56 - 8 * i));
        }
        update(length);

        std::array<uint8_t, 20> digest{};
        for (int i = 0; i < 5; i++) {
            for (int j = 0; j < 4; j++) {
                digest[i * 4 + j] = uint8_t(h_[i] >> (24 - 8 * j));
            }
        }
        return digest;
    }

private:
    static uint32_t rotl(uint32_t value, int bits) { return (value << bits) | (value >> (32 - bits)); }

    void process_block() {
        uint32_t w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = (uint32_t(block_[i * 4]) << 24) | (uint32_t(block_[i * 4 + 1]) << 16) |
                   (uint32_t(block_[i * 4 + 2]) << 8) | uint32_t(block_[i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++) {
            w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            const uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = temp;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
    }

    uint32_t h_[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t block_[64] = {};
    size_t block_len_ = 0;
    uint64_t total_bits_ = 0;
};

} // namespace

const char* game_name(Game game) {
    switch (game) {
        case Game::BanjoKazooie:
            return "Banjo-Kazooie";
        case Game::DonkeyKong64:
            return "Donkey Kong 64";
        case Game::Unknown:
            break;
    }
    return "Unknown";
}

const char* byte_order_name(ByteOrder order) {
    switch (order) {
        case ByteOrder::BigEndian:
            return "big-endian (.z64)";
        case ByteOrder::ByteSwapped:
            return "byte-swapped (.v64)";
        case ByteOrder::LittleEndian:
            return "little-endian (.n64)";
        case ByteOrder::Unknown:
            break;
    }
    return "unknown";
}

ByteOrder detect_byte_order(std::span<const uint8_t> rom) {
    if (rom.size() < 4) {
        return ByteOrder::Unknown;
    }
    const uint32_t magic = (uint32_t(rom[0]) << 24) | (uint32_t(rom[1]) << 16) | (uint32_t(rom[2]) << 8) | rom[3];
    switch (magic) {
        case 0x80371240:
            return ByteOrder::BigEndian;
        case 0x37804012:
            return ByteOrder::ByteSwapped;
        case 0x40123780:
            return ByteOrder::LittleEndian;
        default:
            return ByteOrder::Unknown;
    }
}

bool normalize_to_big_endian(std::vector<uint8_t>& rom) {
    if (rom.size() % 4 != 0) {
        return false;
    }
    switch (detect_byte_order(rom)) {
        case ByteOrder::BigEndian:
            return true;
        case ByteOrder::ByteSwapped:
            for (size_t i = 0; i < rom.size(); i += 2) {
                std::swap(rom[i], rom[i + 1]);
            }
            return true;
        case ByteOrder::LittleEndian:
            for (size_t i = 0; i < rom.size(); i += 4) {
                std::swap(rom[i], rom[i + 3]);
                std::swap(rom[i + 1], rom[i + 2]);
            }
            return true;
        case ByteOrder::Unknown:
            break;
    }
    return false;
}

std::string sha1_hex(std::span<const uint8_t> data) {
    Sha1 sha;
    sha.update(data);
    const auto digest = sha.finish();
    std::string out;
    char buf[3];
    for (uint8_t byte : digest) {
        std::snprintf(buf, sizeof(buf), "%02x", byte);
        out += buf;
    }
    return out;
}

RomInfo identify_big_endian(std::span<const uint8_t> rom, ByteOrder original_order) {
    RomInfo info;
    info.original_order = original_order;
    if (rom.size() < kHeaderSize) {
        return info;
    }
    info.internal_name = header_string(rom, 0x20, 20);
    info.game_code = header_string(rom, 0x3B, 4);
    info.revision = rom[0x3F];
    info.sha1 = sha1_hex(rom);

    // The middle two characters of the game code identify the title; the last is the region.
    const std::string title = info.game_code.size() == 4 ? info.game_code.substr(1, 2) : "";
    if (title == "BK") {
        info.game = Game::BanjoKazooie;
        info.supported = info.sha1 == kBanjoKazooieUs10Sha1;
    } else if (title == "DO") {
        info.game = Game::DonkeyKong64;
        info.supported = info.sha1 == kDonkeyKong64UsSha1;
    }
    return info;
}

std::optional<Rom> Rom::from_bytes(std::vector<uint8_t> bytes, std::string* error) {
    if (bytes.size() < kHeaderSize || bytes.size() > kMaxRomSize) {
        set_error(error, "file is not the size of an N64 ROM");
        return std::nullopt;
    }
    const ByteOrder order = detect_byte_order(bytes);
    if (order == ByteOrder::Unknown || !normalize_to_big_endian(bytes)) {
        set_error(error, "file is not an N64 ROM (unrecognised header)");
        return std::nullopt;
    }
    Rom rom;
    rom.info_ = identify_big_endian(bytes, order);
    rom.data_ = std::move(bytes);
    return rom;
}

std::optional<Rom> Rom::load_file(const std::string& path, std::string* error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        set_error(error, "could not open " + path);
        return std::nullopt;
    }
    const std::streamoff size = file.tellg();
    if (size < 0 || size_t(size) > kMaxRomSize) {
        set_error(error, path + " is not the size of an N64 ROM");
        return std::nullopt;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size)) {
        set_error(error, "could not read " + path);
        return std::nullopt;
    }
    return from_bytes(std::move(bytes), error);
}

} // namespace jiggy
