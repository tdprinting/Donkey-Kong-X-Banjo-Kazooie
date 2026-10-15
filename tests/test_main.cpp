// Bridge tests. Every ROM here is synthetic: built in memory from made-up bytes so the
// tests run in CI without anyone's game dumps.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "jiggy/bk_anim.hpp"
#include "jiggy/bk_assets.hpp"
#include "jiggy/bk_gfx.hpp"
#include "jiggy/bk_model.hpp"
#include "jiggy/bk_pose.hpp"
#include "jiggy/bk_sprite.hpp"
#include "jiggy/bridge.h"
#include "jiggy/bridge.hpp"
#include "jiggy/dk64_tables.hpp"
#include "jiggy/inflate.hpp"
#include "jiggy/rom.hpp"
#include "jiggy/rom_import.hpp"
#include "miniz.h"
#include "test_framework.hpp"

namespace {

using Bytes = std::vector<uint8_t>;

void put_be32(Bytes& b, size_t at, uint32_t v) {
    b[at] = uint8_t(v >> 24);
    b[at + 1] = uint8_t(v >> 16);
    b[at + 2] = uint8_t(v >> 8);
    b[at + 3] = uint8_t(v);
}

uint32_t read_be32_at(const Bytes& b, size_t at) {
    return (uint32_t(b[at]) << 24) | (uint32_t(b[at + 1]) << 16) | (uint32_t(b[at + 2]) << 8) | b[at + 3];
}

void put_be16(Bytes& b, size_t at, uint16_t v) {
    b[at] = uint8_t(v >> 8);
    b[at + 1] = uint8_t(v);
}

void put_le32(Bytes& b, uint32_t v) {
    for (int i = 0; i < 4; i++) {
        b.push_back(uint8_t(v >> (8 * i)));
    }
}

Bytes pattern(size_t size, uint8_t seed) {
    Bytes out(size);
    for (size_t i = 0; i < size; i++) {
        out[i] = uint8_t((i * 7 + seed) ^ (i >> 3));
    }
    return out;
}

Bytes deflate_raw(const Bytes& data) {
    size_t out_len = 0;
    void* out = tdefl_compress_mem_to_heap(data.data(), data.size(), &out_len, TDEFL_DEFAULT_MAX_PROBES);
    Bytes result(static_cast<uint8_t*>(out), static_cast<uint8_t*>(out) + out_len);
    mz_free(out);
    return result;
}

Bytes rarezip(const Bytes& data) {
    Bytes out = {0x11, 0x72, 0, 0, 0, 0};
    put_be32(out, 2, uint32_t(data.size()));
    const Bytes body = deflate_raw(data);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

Bytes gzip(const Bytes& data, bool with_name = false) {
    Bytes out = {0x1F, 0x8B, 0x08, uint8_t(with_name ? 0x08 : 0x00), 0, 0, 0, 0, 0, 3};
    if (with_name) {
        for (char c : std::string("file.bin")) {
            out.push_back(uint8_t(c));
        }
        out.push_back(0);
    }
    const Bytes body = deflate_raw(data);
    out.insert(out.end(), body.begin(), body.end());
    put_le32(out, uint32_t(mz_crc32(MZ_CRC32_INIT, data.data(), data.size())));
    put_le32(out, uint32_t(data.size()));
    return out;
}

// A big-endian image with a valid N64 header carrying the given game code.
Bytes make_rom(size_t size, const char* code, const char* title) {
    Bytes rom(size, 0);
    put_be32(rom, 0, 0x80371240);
    std::memset(rom.data() + 0x20, ' ', 20);
    std::memcpy(rom.data() + 0x20, title, std::strlen(title));
    std::memcpy(rom.data() + 0x3B, code, 4);
    return rom;
}

constexpr uint32_t kBkBase = 0x100;
constexpr uint32_t kDkBase = 0x200;

// Banjo-style asset bank with: 0 = stored, 1 = rarezip, 2 = empty stub.
struct BkFixture {
    Bytes rom;
    Bytes asset0 = pattern(100, 1);
    Bytes asset1 = pattern(5000, 2);
};

BkFixture make_bk_rom() {
    BkFixture f;
    f.rom = make_rom(0x4000, "NBKE", "Banjo-Kazooie");
    const Bytes stored1 = rarezip(f.asset1);
    const uint32_t count = 4; // three assets plus the terminator
    const uint32_t data_base = kBkBase + 8 + count * 8;
    const uint32_t offsets[4] = {0, uint32_t(f.asset0.size()), uint32_t(f.asset0.size() + stored1.size()),
                                 uint32_t(f.asset0.size() + stored1.size())};
    const uint16_t flags[4] = {0, 1, 0, 0};
    put_be32(f.rom, kBkBase, count);
    for (uint32_t i = 0; i < count; i++) {
        put_be32(f.rom, kBkBase + 8 + i * 8, offsets[i]);
        put_be16(f.rom, kBkBase + 8 + i * 8 + 4, flags[i]);
        put_be16(f.rom, kBkBase + 8 + i * 8 + 6, uint16_t(0x10 + i));
    }
    std::memcpy(f.rom.data() + data_base, f.asset0.data(), f.asset0.size());
    std::memcpy(f.rom.data() + data_base + offsets[1], stored1.data(), stored1.size());
    return f;
}

// DK64-style pointer tables. Table 0: 0 = gzip, 1 = stored, 2 = shared with file 0.
// Table 5: one stored file. Every other table is left pointing at garbage.
struct DkFixture {
    Bytes rom;
    Bytes file0 = pattern(3000, 3);
    Bytes file1 = pattern(64, 4);
    Bytes model = pattern(40, 5);
    uint32_t shared_at = 0; // base-relative offset of file 2's redirect header
};

DkFixture make_dk_rom() {
    DkFixture f;
    f.rom = make_rom(0x8000, "NDOE", "DONKEY KONG 64");
    // Unused tables point past the end of the ROM so they parse as empty.
    for (uint32_t t = 0; t < 32; t++) {
        put_be32(f.rom, kDkBase + t * 4, 0x7FFF0000);
    }

    const Bytes gz0 = gzip(f.file0, true);
    const uint32_t table0 = 0x80;          // relative to base
    const uint32_t data0 = table0 + 4 * 4; // 3 files + terminator
    const uint32_t file1_at = data0 + uint32_t(gz0.size());
    const uint32_t shared_at = file1_at + uint32_t(f.file1.size());
    const uint32_t end0 = shared_at + 8;
    put_be32(f.rom, kDkBase + 0 * 4, table0);
    put_be32(f.rom, kDkBase + table0 + 0, data0);
    put_be32(f.rom, kDkBase + table0 + 4, file1_at);
    put_be32(f.rom, kDkBase + table0 + 8, shared_at | 0x80000000);
    put_be32(f.rom, kDkBase + table0 + 12, end0);
    std::memcpy(f.rom.data() + kDkBase + data0, gz0.data(), gz0.size());
    std::memcpy(f.rom.data() + kDkBase + file1_at, f.file1.data(), f.file1.size());
    put_be16(f.rom, kDkBase + shared_at, 0); // file 2 shares file 0
    f.shared_at = shared_at;

    const uint32_t table5 = (end0 + 15) & ~15u;
    const uint32_t data5 = table5 + 2 * 4;
    put_be32(f.rom, kDkBase + 5 * 4, table5);
    put_be32(f.rom, kDkBase + table5, data5);
    put_be32(f.rom, kDkBase + table5 + 4, data5 + uint32_t(f.model.size()));
    std::memcpy(f.rom.data() + kDkBase + data5, f.model.data(), f.model.size());
    return f;
}

jiggy::Rom to_rom(Bytes bytes) {
    std::string error;
    auto rom = jiggy::Rom::from_bytes(std::move(bytes), &error);
    if (!rom) {
        std::fprintf(stderr, "  fixture ROM rejected: %s\n", error.c_str());
        std::abort();
    }
    return std::move(*rom);
}

std::string sha1_of(const char* text) {
    return jiggy::sha1_hex({reinterpret_cast<const uint8_t*>(text), std::strlen(text)});
}

} // namespace

TEST(sha1_known_vectors) {
    CHECK(sha1_of("") == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    CHECK(sha1_of("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(sha1_of("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    const Bytes million(1000000, 'a');
    CHECK(jiggy::sha1_hex(million) == "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

TEST(byte_orders_normalise) {
    const Bytes z64 = make_rom(0x100, "NBKE", "Banjo-Kazooie");
    Bytes v64 = z64, n64 = z64;
    for (size_t i = 0; i < v64.size(); i += 2) std::swap(v64[i], v64[i + 1]);
    for (size_t i = 0; i < n64.size(); i += 4) {
        std::swap(n64[i], n64[i + 3]);
        std::swap(n64[i + 1], n64[i + 2]);
    }
    CHECK(jiggy::detect_byte_order(z64) == jiggy::ByteOrder::BigEndian);
    CHECK(jiggy::detect_byte_order(v64) == jiggy::ByteOrder::ByteSwapped);
    CHECK(jiggy::detect_byte_order(n64) == jiggy::ByteOrder::LittleEndian);
    CHECK(jiggy::normalize_to_big_endian(v64) && v64 == z64);
    CHECK(jiggy::normalize_to_big_endian(n64) && n64 == z64);

    std::string error;
    auto rom = jiggy::Rom::from_bytes(n64, &error);
    CHECK(rom.has_value());
    if (rom) {
        CHECK(rom->info().original_order == jiggy::ByteOrder::BigEndian);
    }
    Bytes garbage(0x100, 0xAB);
    CHECK(!jiggy::Rom::from_bytes(garbage, &error).has_value());
}

TEST(identify_games) {
    auto bk = to_rom(make_rom(0x100, "NBKE", "Banjo-Kazooie"));
    CHECK(bk.info().game == jiggy::Game::BanjoKazooie);
    CHECK(bk.info().game_code == "NBKE");
    CHECK(bk.info().internal_name == "Banjo-Kazooie");
    CHECK(!bk.info().supported); // synthetic, so its SHA-1 can't match the real dump

    auto dk = to_rom(make_rom(0x100, "NDOE", "DONKEY KONG 64"));
    CHECK(dk.info().game == jiggy::Game::DonkeyKong64);

    auto other = to_rom(make_rom(0x100, "NSME", "SUPER MARIO 64"));
    CHECK(other.info().game == jiggy::Game::Unknown);
}

TEST(rarezip_round_trip) {
    const Bytes data = pattern(10000, 9);
    const Bytes packed = rarezip(data);
    CHECK(jiggy::is_rarezip(packed));
    CHECK(jiggy::rarezip_uncompressed_size(packed) == uint32_t(data.size()));
    std::string error;
    auto out = jiggy::inflate_rarezip(packed, &error);
    CHECK(out && *out == data);

    Bytes lying = packed;
    put_be32(lying, 2, uint32_t(data.size() + 1));
    CHECK(!jiggy::inflate_rarezip(lying, &error).has_value());
}

TEST(gzip_round_trip_and_crc) {
    const Bytes data = pattern(7000, 11);
    Bytes packed = gzip(data, true);
    packed.resize(packed.size() + 13, 0); // ROM files are padded after the member
    std::string error;
    auto out = jiggy::inflate_gzip(packed, &error);
    CHECK(out && *out == data);

    Bytes corrupt = gzip(data);
    corrupt[corrupt.size() - 8] ^= 0xFF; // break the CRC
    CHECK(!jiggy::inflate_gzip(corrupt, &error).has_value());
    CHECK(error.find("CRC") != std::string::npos);
}

TEST(bk_asset_table) {
    const auto f = make_bk_rom();
    std::string error;
    auto table = jiggy::BkAssetTable::parse(f.rom, kBkBase, &error);
    CHECK(table.has_value());
    if (!table) {
        return;
    }
    CHECK(table->size() == 3);
    CHECK(!table->entry(0).compressed);
    CHECK(table->entry(1).compressed);
    CHECK(table->entry(2).stored_size == 0);
    CHECK(table->entry(1).type == 0x11);

    CHECK(table->uncompressed_size(f.rom, 1, &error) == uint32_t(f.asset1.size()));
    auto a0 = table->load(f.rom, 0, &error);
    auto a1 = table->load(f.rom, 1, &error);
    auto a2 = table->load(f.rom, 2, &error);
    CHECK(a0 && *a0 == f.asset0);
    CHECK(a1 && *a1 == f.asset1);
    CHECK(a2 && a2->empty());
    CHECK(!table->load(f.rom, 3, &error).has_value());
}

TEST(bk_asset_names) {
    CHECK(std::strcmp(jiggy::bk_asset_name(0x34E), "MODEL_BANJOKAZOOIE_HIGH_POLY") == 0);
    CHECK(std::strcmp(jiggy::bk_asset_name(0x1), "ANIM_BSCROUCH_ENTER") == 0);
    CHECK(jiggy::bk_asset_name(0) == nullptr);
    CHECK(jiggy::bk_asset_name(0xFFFFFF) == nullptr);
}

TEST(bk_asset_table_rejects_garbage) {
    Bytes rom(0x200, 0);
    put_be32(rom, 0x10, 0xFFFFFFFF);
    std::string error;
    CHECK(!jiggy::BkAssetTable::parse(rom, 0x10, &error).has_value());
    CHECK(!jiggy::BkAssetTable::parse(rom, 0x1000, &error).has_value());

    // An entry pointing past the ROM is marked invalid instead of failing the whole bank.
    auto f = make_bk_rom();
    put_be32(f.rom, kBkBase + 8 + 2 * 8, 0x00FFFFFF);
    auto table = jiggy::BkAssetTable::parse(f.rom, kBkBase, &error);
    CHECK(table.has_value());
    if (table) {
        CHECK(table->entry(0).valid);
        CHECK(!table->entry(1).valid);
        CHECK(!table->load(f.rom, 1, &error).has_value());
    }
}

TEST(dk64_pointer_tables) {
    const auto f = make_dk_rom();
    std::string error;
    auto tables = jiggy::Dk64PointerTables::parse(f.rom, kDkBase, &error);
    CHECK(tables.has_value());
    if (!tables) {
        return;
    }
    CHECK(tables->file_count(0) == 3);
    CHECK(tables->file_count(5) == 1);
    CHECK(tables->file_count(1) == 0);
    CHECK(tables->file_count(99) == 0);

    auto f0 = tables->load(f.rom, 0, 0, &error);
    auto f1 = tables->load(f.rom, 0, 1, &error);
    auto f2 = tables->load(f.rom, 0, 2, &error);
    auto m = tables->load(f.rom, 5, 0, &error);
    CHECK(f0 && *f0 == f.file0);
    CHECK(f1 && *f1 == f.file1);
    CHECK(f2 && *f2 == f.file0);
    CHECK(m && *m == f.model);

    auto shared = tables->resolve(f.rom, 0, 2, &error);
    CHECK(shared && shared->resolved_index == 0 && shared->index == 2);
    CHECK(!tables->load(f.rom, 0, 3, &error).has_value());
}

TEST(dk64_redirect_loop_is_an_error) {
    auto f = make_dk_rom();
    put_be16(f.rom, kDkBase + f.shared_at, 2); // file 2 now redirects to itself
    std::string error;
    auto tables = jiggy::Dk64PointerTables::parse(f.rom, kDkBase, &error);
    CHECK(tables.has_value());
    if (tables) {
        CHECK(!tables->load(f.rom, 0, 2, &error).has_value());
        CHECK(error.find("loop") != std::string::npos);
    }
}

TEST(bridge_cpp_api) {
    jiggy::BridgeOptions options;
    options.require_known_dumps = false;
    options.bk_asset_base = kBkBase;
    options.dk64_table_base = kDkBase;
    const auto bkf = make_bk_rom();
    const auto dkf = make_dk_rom();

    std::string error;
    auto bridge = jiggy::Bridge::open(to_rom(bkf.rom), to_rom(dkf.rom), options, &error);
    CHECK(bridge != nullptr);
    if (!bridge) {
        std::fprintf(stderr, "  %s\n", error.c_str());
        return;
    }
    auto a1 = bridge->load_bk_asset(1, &error);
    CHECK(a1 && *a1 == bkf.asset1);
    auto f0 = bridge->load_dk64_file(0, 0, &error);
    CHECK(f0 && *f0 == dkf.file0);

    // ROMs passed the wrong way round are rejected with a clear message.
    auto swapped = jiggy::Bridge::open(to_rom(dkf.rom), to_rom(bkf.rom), options, &error);
    CHECK(swapped == nullptr);
    CHECK(error.find("expected a Banjo-Kazooie ROM") != std::string::npos);

    // With the default options, unknown dumps are refused.
    auto strict = jiggy::Bridge::open(to_rom(bkf.rom), to_rom(dkf.rom), {}, &error);
    CHECK(strict == nullptr);
    CHECK(error.find("not the US 1.0 dump") != std::string::npos);
}

TEST(bridge_c_api_rejects_unknown_dumps) {
    const auto bkf = make_bk_rom();
    const auto dkf = make_dk_rom();
    char error[256] = {};
    JiggyBridge* bridge =
        jiggy_bridge_open_memory(bkf.rom.data(), bkf.rom.size(), dkf.rom.data(), dkf.rom.size(), error, sizeof(error));
    CHECK(bridge == nullptr);
    CHECK(std::strstr(error, "not the US 1.0 dump") != nullptr);
    CHECK(jiggy_bk_asset_count(nullptr) == 0);
    CHECK(jiggy_dk64_file_read(nullptr, 0, 0, nullptr, 0) == -1);
    CHECK(jiggy_bridge_open("/nonexistent/bk.z64", "/nonexistent/dk.z64", error, sizeof(error)) == nullptr);
    CHECK(std::strstr(error, "could not open") != nullptr);
}

namespace {

// Builds a sprite the way the game lays one out (see bk_sprite.hpp).
struct SpriteChunk {
    int16_t x, y, w, h;
    Bytes texels;
};

void pad8(Bytes& b) {
    while (b.size() % 8) {
        b.push_back(0xEE);
    }
}

void push_be16(Bytes& b, uint16_t v) {
    b.push_back(uint8_t(v >> 8));
    b.push_back(uint8_t(v));
}

Bytes make_frame(int16_t w, int16_t h, const std::vector<uint16_t>& palette, const std::vector<SpriteChunk>& chunks,
                 size_t start) {
    // Alignment is relative to the sprite, so build with the right number of leading bytes and strip them.
    Bytes b(start, 0);
    for (int16_t v : {int16_t(0), int16_t(0), w, h, int16_t(chunks.size()), int16_t(0), int16_t(0), int16_t(0),
                      int16_t(0), int16_t(0)}) {
        push_be16(b, uint16_t(v));
    }
    if (!palette.empty()) {
        pad8(b);
        for (uint16_t c : palette) {
            push_be16(b, c);
        }
    }
    for (const auto& c : chunks) {
        for (int16_t v : {c.x, c.y, c.w, c.h}) {
            push_be16(b, uint16_t(v));
        }
        pad8(b);
        b.insert(b.end(), c.texels.begin(), c.texels.end());
    }
    return Bytes(b.begin() + long(start), b.end());
}

Bytes make_sprite(uint16_t type, const std::vector<Bytes>& frames_for_offsets) {
    Bytes b;
    push_be16(b, uint16_t(frames_for_offsets.size()));
    push_be16(b, type);
    b.resize(0x10, 0);
    const size_t table_end = 0x10 + 4 * frames_for_offsets.size();
    b.resize(table_end, 0);
    size_t offset = 0;
    for (size_t i = 0; i < frames_for_offsets.size(); i++) {
        put_be32(b, 0x10 + 4 * i, uint32_t(offset));
        b.insert(b.end(), frames_for_offsets[i].begin(), frames_for_offsets[i].end());
        offset += frames_for_offsets[i].size();
    }
    return b;
}

const uint8_t* pixel(const jiggy::BkSpriteImage& img, int x, int y) {
    return &img.rgba[(size_t(y) * img.width + size_t(x)) * 4];
}

} // namespace

TEST(bk_sprite_ci4_two_frames_and_chunks) {
    // Palette: 0 transparent, 1 opaque red, 2 opaque white.
    std::vector<uint16_t> palette(16, 0);
    palette[1] = 0xF801;
    palette[2] = 0xFFFF;
    // Frame 0: 4x3 canvas, a 3x2 chunk (odd width: 2 bytes per row) and a 1x1 chunk at (3,2).
    SpriteChunk a{0, 0, 3, 2, {0x12, 0x10, 0x21, 0x00}};
    SpriteChunk b{3, 2, 1, 1, {0x20}};
    // Frames start at the sprite's frame table end (0x18 for two frames), so pass that for alignment.
    Bytes f0 = make_frame(4, 3, palette, {a, b}, 0x18);
    Bytes f1 = make_frame(2, 1, palette, {SpriteChunk{0, 0, 2, 1, {0x21}}}, 0x18 + f0.size());
    Bytes sprite = make_sprite(0x001, {f0, f1});

    std::string error;
    auto parsed = jiggy::BkSprite::parse(sprite, &error);
    CHECK(parsed.has_value());
    CHECK(parsed->frame_count() == 2);
    CHECK(std::string(jiggy::bk_sprite_format_name(parsed->type())) == "CI4");

    auto img = parsed->decode_frame(0, &error);
    CHECK(img.has_value());
    CHECK(img->width == 4 && img->height == 3);
    CHECK(pixel(*img, 0, 0)[0] == 0xFF && pixel(*img, 0, 0)[1] == 0 && pixel(*img, 0, 0)[3] == 0xFF); // red
    CHECK(pixel(*img, 1, 0)[1] == 0xFF && pixel(*img, 1, 0)[3] == 0xFF);                               // white
    CHECK(pixel(*img, 2, 0)[0] == 0xFF && pixel(*img, 2, 0)[1] == 0);                                  // red
    CHECK(pixel(*img, 0, 1)[1] == 0xFF);                                                               // white
    CHECK(pixel(*img, 2, 1)[3] == 0);                                                                  // index 0
    CHECK(pixel(*img, 3, 2)[1] == 0xFF && pixel(*img, 3, 2)[3] == 0xFF);                               // second chunk
    CHECK(pixel(*img, 3, 0)[3] == 0);                                                                  // uncovered

    auto img1 = parsed->decode_frame(1, &error);
    CHECK(img1.has_value());
    CHECK(img1->width == 2 && pixel(*img1, 0, 0)[1] == 0xFF && pixel(*img1, 1, 0)[1] == 0);
    CHECK(!parsed->decode_frame(2, &error).has_value());
}

TEST(bk_sprite_direct_formats) {
    std::string error;
    {
        // RGBA16: one blue opaque texel, one transparent.
        Bytes f = make_frame(2, 1, {}, {SpriteChunk{0, 0, 2, 1, {0x00, 0x3F, 0x00, 0x00}}}, 0x14);
        auto s = jiggy::BkSprite::parse(make_sprite(0x400, {f}), &error);
        CHECK(s.has_value());
        auto img = s->decode_frame(0, &error);
        CHECK(img.has_value());
        CHECK(pixel(*img, 0, 0)[2] == 0xFF && pixel(*img, 0, 0)[3] == 0xFF && pixel(*img, 1, 0)[3] == 0);
    }
    {
        // IA8: intensity 0xF, alpha 0x8.
        Bytes f = make_frame(1, 1, {}, {SpriteChunk{0, 0, 1, 1, {0xF8}}}, 0x14);
        auto img = jiggy::BkSprite::parse(make_sprite(0x100, {f}), &error)->decode_frame(0, &error);
        CHECK(img.has_value());
        CHECK(pixel(*img, 0, 0)[0] == 0xFF && pixel(*img, 0, 0)[3] == 0x88);
    }
    {
        // RGBA32, with a chunk hanging off the canvas that must be clipped.
        Bytes f = make_frame(1, 1, {}, {SpriteChunk{0, 0, 2, 1, {1, 2, 3, 4, 5, 6, 7, 8}}}, 0x14);
        auto img = jiggy::BkSprite::parse(make_sprite(0x800, {f}), &error)->decode_frame(0, &error);
        CHECK(img.has_value());
        CHECK(img->rgba.size() == 4 && img->rgba[0] == 1 && img->rgba[3] == 4);
    }
}

TEST(bk_sprite_rejects_bad_data) {
    std::string error;
    CHECK(!jiggy::BkSprite::parse(Bytes(4, 0), &error).has_value());
    Bytes unknown = make_sprite(0x200, {make_frame(1, 1, {}, {}, 0x14)});
    CHECK(!jiggy::BkSprite::parse(unknown, &error).has_value());

    // Chunk claims more texels than exist.
    Bytes f = make_frame(8, 8, {}, {SpriteChunk{0, 0, 8, 8, {0x00, 0x01}}}, 0x14);
    auto s = jiggy::BkSprite::parse(make_sprite(0x400, {f}), &error);
    CHECK(s.has_value());
    CHECK(!s->decode_frame(0, &error).has_value());
    CHECK(error.find("past the end") != std::string::npos);
}

TEST(rgba5551_packing) {
    const Bytes rgba = {0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x10};
    const Bytes packed = jiggy::rgba8888_to_rgba5551(rgba);
    CHECK(packed.size() == 4);
    CHECK(packed[0] == 0xF8 && packed[1] == 0x01);
    CHECK(packed[2] == 0x00 && packed[3] == 0x3E);
}

// A model the way the game lays one out (see bk_model.hpp): one 2x2 CI4 texture and a
// quad drawn as G_TRI1 plus a G_TRI2 that reuses cached vertices.
// A small model: one CI4 2x2 texture, four vertices and a display list exercising each
// command the reader handles.
Bytes make_test_bk_model() {
    Bytes m(0x38, 0);
    put_be32(m, 0, 0x0000000B);
    // Texture list: one CI4 2x2 texture, palette (32 bytes) then 2 bytes of texels.
    const size_t tex = m.size();
    put_be16(m, 0x08, uint16_t(tex));
    m.resize(tex + 8 + 0x10 + 32 + 8, 0);
    put_be32(m, tex, 32 + 2);
    put_be16(m, tex + 4, 1);
    put_be32(m, tex + 8, 0);
    put_be16(m, tex + 12, 0x01);
    m[tex + 16] = 2;
    m[tex + 17] = 2;
    const size_t palette = tex + 8 + 0x10;
    put_be16(m, palette + 0, 0xF801); // red
    put_be16(m, palette + 2, 0x07C1); // green
    m[palette + 32] = 0x01;           // texels: 0 1 / 1 0
    m[palette + 33] = 0x10;

    const size_t gfx = m.size();
    put_be32(m, 0x0C, uint32_t(gfx));
    const std::vector<std::pair<uint32_t, uint32_t>> cmds = {
        {0xBB000001, 0xFFFFFFFF},                         // G_TEXTURE, full scale
        {0xFD100000, 0x02000000},                         // G_SETTIMG palette
        {0xFD500000, 0x02000020},                         // G_SETTIMG texels: texture 0
        {0x04000000u | (3 << 10) | (3 * 16 - 1), 0x01000000}, // G_VTX 3 at slot 0
        {0xBF000000, 0x00000204},                         // G_TRI1 0 1 2
        {0x04060000u | (1 << 10) | (16 - 1), 0x01000030}, // G_VTX vertex 3 into slot 3
        {0xB1000206, 0x00020406},                         // G_TRI2 (0 1 3) (1 2 3)
        {0xFD100000, 0x0B000000},                         // animated texture: untextured
        {0xBF000000, 0x00000204},
        {0xB8000000, 0},
    };
    m.resize(gfx + 8 + cmds.size() * 8, 0);
    put_be32(m, gfx, uint32_t(cmds.size()));
    for (size_t i = 0; i < cmds.size(); i++) {
        put_be32(m, gfx + 8 + i * 8, cmds[i].first);
        put_be32(m, gfx + 8 + i * 8 + 4, cmds[i].second);
    }

    const size_t vtx = m.size();
    put_be32(m, 0x10, uint32_t(vtx));
    put_be16(m, 0x30, 5);
    m.resize(vtx + 0x18 + 4 * 16, 0);
    put_be16(m, vtx + 0x14, 4);
    const int16_t pos[4][2] = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
    for (size_t i = 0; i < 4; i++) {
        const size_t at = vtx + 0x18 + i * 16;
        put_be16(m, at, uint16_t(pos[i][0]));
        put_be16(m, at + 2, uint16_t(pos[i][1]));
        put_be16(m, at + 8, uint16_t(pos[i][0] ? 64 : 0)); // 2 texels in s10.5
        put_be16(m, at + 10, uint16_t(pos[i][1] ? 64 : 0));
        m[at + 12] = 0xFF;
        m[at + 15] = 0xFF;
    }

    return m;
}

TEST(bk_model_triangles_and_textures) {
    Bytes m = make_test_bk_model();
    std::string error;
    auto model = jiggy::BkModel::parse(m, &error);
    CHECK(model.has_value());
    if (!model) {
        return;
    }
    CHECK(model->vertices().size() == 4);
    CHECK(model->textures().size() == 1);
    CHECK(model->triangles().size() == 4);
    CHECK(model->header_triangle_count() == 5);
    const auto& t0 = model->triangles()[0];
    CHECK(t0.texture == 0);
    CHECK(t0.vertices[0] == 0 && t0.vertices[1] == 1 && t0.vertices[2] == 2);
    CHECK(t0.uv[1][0] == 1.0f && t0.uv[1][1] == 0.0f);
    const auto& t1 = model->triangles()[1];
    CHECK(t1.vertices[0] == 0 && t1.vertices[1] == 1 && t1.vertices[2] == 3);
    CHECK(model->triangles()[2].vertices[2] == 3);
    CHECK(model->triangles()[3].texture == -1);

    auto image = model->decode_texture(0, &error);
    CHECK(image.has_value());
    if (image) {
        CHECK(image->width == 2 && image->height == 2);
        CHECK(image->rgba[0] == 0xFF && image->rgba[1] == 0 && image->rgba[3] == 0xFF); // red
        CHECK(image->rgba[4] == 0 && image->rgba[5] == 0xFF);                          // green
        CHECK(image->rgba[12] == 0xFF && image->rgba[13] == 0);                        // red
    }
    CHECK(!model->decode_texture(1, &error));
    CHECK(std::string(jiggy::bk_texture_format_name(0x01)) == "CI4");
}

TEST(bk_model_to_f3dex2_converts_commands) {
    std::string error;
    auto model = jiggy::BkModel::parse(make_test_bk_model(), &error);
    CHECK(model.has_value());
    if (!model) {
        return;
    }
    const uint32_t base = 0x80400000;
    auto block = jiggy::bk_model_to_f3dex2(*model, base, &error);
    CHECK(block.has_value());
    if (!block) {
        return;
    }
    // 6 prologue + 7 epilogue + 10 model commands, then 13 render modes, 4 vertices and the
    // 34 bytes of texture data, padded to 8.
    const size_t dl = 23 * 8, vertices = dl + 13 * 16, textures = vertices + 4 * 16;
    CHECK(block->display_list_commands == 23);
    CHECK(block->dropped_commands == 0);
    CHECK(block->prologue_offset == 0 && block->epilogue_offset == 6 * 8 && block->commands_offset == 13 * 8);
    CHECK(block->bytes.size() == ((textures + 34 + 7) & ~size_t(7)));
    auto w = [&](size_t at) { return read_be32_at(block->bytes, at); };
    auto cmd = [&](size_t i, uint32_t w0, uint32_t w1) { return w(i * 8) == w0 && w(i * 8 + 4) == w1; };
    CHECK(cmd(5, 0xDF000000, 0));                                   // prologue's end
    CHECK(cmd(12, 0xDF000000, 0));                                  // epilogue's end
    CHECK(cmd(13, 0xD7000002, 0xFFFFFFFF));                         // G_TEXTURE
    CHECK(cmd(14, 0xFD100000, base + uint32_t(textures)));          // palette
    CHECK(cmd(15, 0xFD500000, base + uint32_t(textures) + 0x20));   // texels
    CHECK(cmd(16, 0x01003006, base + uint32_t(vertices)));          // G_VTX 3 at slot 0
    CHECK(cmd(17, 0x05000204, 0));                                  // G_TRI1
    CHECK(cmd(18, 0x01001008, base + uint32_t(vertices) + 0x30));   // G_VTX 1 at slot 3
    CHECK(cmd(19, 0x06000206, 0x00020406));                         // G_TRI2
    CHECK(cmd(20, 0xFD100000, base + uint32_t(textures)));          // animated: frame 0 stand-in
    CHECK(cmd(22, 0xDF000000, 0));                                  // model's ENDDL
    // Render mode table entry 1, then the vertices and texture data.
    CHECK(w(dl + 16) == 0xE200001C && w(dl + 20) == 0x0C192078 && w(dl + 24) == 0xDF000000);
    CHECK(w(vertices + 16) == 0x00640000);                          // vertex 1: x 100, y 0
    CHECK(block->bytes[textures] == 0xF8 && block->bytes[textures + 32] == 0x01);

    CHECK(!jiggy::bk_model_to_f3dex2(*model, base + 4, &error).has_value());
}

// The test model plus a two-bone skeleton and a geo list: a bone with a skinned joint, then
// a selector choosing between two sub-lists.
Bytes make_test_bk_model_with_bones() {
    Bytes m = make_test_bk_model();
    auto put_f32 = [&m](size_t at, float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, 4);
        put_be32(m, at, bits);
    };
    const size_t skeleton = m.size();
    put_be32(m, 0x18, uint32_t(skeleton));
    m.resize(skeleton + 8 + 2 * 16, 0);
    put_f32(skeleton, 10.0f);
    put_be16(m, skeleton + 4, 2);
    put_be16(m, skeleton + 8 + 12, 1);       // entry 0: bone 1 at the origin, no parent
    put_be16(m, skeleton + 8 + 14, 0xFFFF);
    put_f32(skeleton + 24, 100.0f);          // entry 1: bone 2 pivoting at x 100, on entry 0
    put_be16(m, skeleton + 24 + 12, 2);
    put_be16(m, skeleton + 24 + 14, 0);

    const size_t geo = m.size();
    put_be32(m, 0x04, uint32_t(geo));
    m.resize(geo + 0x58, 0);
    put_be32(m, geo + 0x00, 2);              // BONE entry 1, its children at +0x10
    put_be32(m, geo + 0x04, 0x20);
    m[geo + 0x08] = 0x10;
    m[geo + 0x09] = 1;
    put_be32(m, geo + 0x10, 5);              // SKINNING lists 0 and 5
    put_be16(m, geo + 0x18, 0);
    put_be16(m, geo + 0x1A, 5);
    put_be32(m, geo + 0x20, 0x0C);           // SELECTOR 3 of two branches
    put_be16(m, geo + 0x28, 2);
    put_be16(m, geo + 0x2A, 3);
    put_be32(m, geo + 0x2C, 0x18);
    put_be32(m, geo + 0x30, 0x28);
    put_be32(m, geo + 0x38, 3);              // LOADDL 7
    put_be16(m, geo + 0x40, 7);
    put_be32(m, geo + 0x48, 3);              // LOADDL 8
    put_be16(m, geo + 0x50, 8);
    return m;
}

TEST(bk_pose_display_list_walks_bones) {
    std::string error;
    const Bytes bytes = make_test_bk_model_with_bones();
    auto model = jiggy::BkModel::parse(bytes, &error);
    CHECK(model.has_value());
    auto skeleton = jiggy::BkSkeleton::parse(bytes, &error);
    CHECK(skeleton.has_value() && skeleton->bones.size() == 2);
    if (!model || !skeleton) {
        return;
    }
    CHECK(skeleton->scale == 10.0f && skeleton->bones[1].pivot[0] == 100.0f && skeleton->bones[1].parent == 0);
    const uint32_t base = 0x80400000;
    auto block = jiggy::bk_model_to_f3dex2(*model, base, &error);
    CHECK(block.has_value());
    if (!block) {
        return;
    }
    const uint32_t root = 0x80500000, a = 0x80500040, b = 0x80500080;
    const uint32_t bones[] = {a, b};
    jiggy::BkSelectors selectors{};
    selectors[3] = 2;
    auto pose = jiggy::bk_pose_display_list(*model, *block, base, root, bones, selectors, &error);
    CHECK(pose.has_value());
    if (!pose) {
        return;
    }
    const uint32_t list = base + block->commands_offset;
    const std::vector<std::array<uint32_t, 2>> expected = {
        {0xDE000000, base + block->prologue_offset},
        {0xDA380002, root},     // push the root matrix
        {0xDE000000, list},     // skinning: first list with the enclosing (root) matrix
        {0xDA380003, b},
        {0xDE000000, list + 5 * 8},
        {0xDA380003, root},     // the selector's second branch, outside the bone
        {0xDE000000, list + 8 * 8},
        {0xD8380002, 0x40},
        {0xDE000000, base + block->epilogue_offset},
        {0xDF000000, 0},
    };
    CHECK(pose->commands == expected);
    CHECK(pose->sub_lists == 3);

    // Hidden selector branches and a bone the skeleton lacks.
    selectors[3] = 0;
    pose = jiggy::bk_pose_display_list(*model, *block, base, root, bones, selectors, &error);
    CHECK(pose.has_value() && pose->sub_lists == 2);
    CHECK(!jiggy::bk_pose_display_list(*model, *block, base, root, std::span<const uint32_t>(bones, 1), selectors, &error));
}

TEST(bk_animation_poses_skeleton) {
    // Frames 0-10: bone 2 turns from yaw 0 to 90 degrees and moves 1 unit (x 10 scale) in x.
    Bytes anim(8 + 4 + 8 + 4 + 4, 0);
    put_be16(anim, 2, 10);
    put_be16(anim, 4, 2);
    put_be16(anim, 8, (2 << 4) | 1);
    put_be16(anim, 10, 2);
    put_be16(anim, 12, 0);
    put_be16(anim, 14, 0);
    put_be16(anim, 16, 10);
    put_be16(anim, 18, 90 * 64);
    put_be16(anim, 20, (2 << 4) | 6);
    put_be16(anim, 22, 1);
    put_be16(anim, 24, 0);
    put_be16(anim, 26, 64);
    std::string error;
    auto animation = jiggy::BkAnimation::parse(anim, &error);
    CHECK(animation.has_value());
    auto skeleton = jiggy::BkSkeleton::parse(make_test_bk_model_with_bones(), &error);
    CHECK(skeleton.has_value());
    if (!animation || !skeleton) {
        return;
    }
    CHECK(animation->tracks().size() == 2 && animation->tracks()[0].keys[1].value == 90 * 64);
    const auto pose = animation->sample(0.5f);
    CHECK(pose.size() == 3 && pose[2].rotation[1] == 45.0f && pose[2].translation[0] == 1.0f);
    const auto matrices = jiggy::bk_skeleton_matrices(*skeleton, pose);
    CHECK(matrices.size() == 2);
    if (matrices.size() != 2) {
        return;
    }
    CHECK(matrices[0] == jiggy::bk_matrix_identity());
    auto apply = [](const jiggy::BkMatrix& m, float x, float y, float z, int axis) {
        return x * m[0][axis] + y * m[1][axis] + z * m[2][axis] + m[3][axis];
    };
    auto near = [](float a, float b) { return a - b < 0.001f && b - a < 0.001f; };
    // The pivot moves by the translation; a point beside it turns about it.
    CHECK(near(apply(matrices[1], 100, 0, 0, 0), 110.0f) && near(apply(matrices[1], 100, 0, 0, 2), 0.0f));
    CHECK(near(apply(matrices[1], 101, 0, 0, 0), 110.70711f) && near(apply(matrices[1], 101, 0, 0, 2), -0.70711f));
    // At rest, everything stays where it is modelled.
    const auto rest = jiggy::bk_skeleton_matrices(*skeleton, {});
    CHECK(rest[1] == jiggy::bk_matrix_identity());

    auto bad = anim;
    put_be16(bad, 16, 0); // keys out of order
    CHECK(!jiggy::BkAnimation::parse(bad, &error));
}

TEST(bk_matrix_to_mtx_fixed_point) {
    auto m = jiggy::bk_matrix_identity();
    m[3][0] = 1.5f;
    m[3][1] = -2.0f;
    const auto mtx = jiggy::bk_matrix_to_mtx(m);
    CHECK(mtx[0] == 0x00 && mtx[1] == 0x01 && mtx[32] == 0 && mtx[33] == 0);  // [0][0] = 1
    CHECK(mtx[24] == 0x00 && mtx[25] == 0x01 && mtx[56] == 0x80 && mtx[57] == 0x00); // [3][0] = 1.5
    CHECK(mtx[26] == 0xFF && mtx[27] == 0xFE && mtx[58] == 0x00 && mtx[59] == 0x00); // [3][1] = -2
}

TEST(bk_model_rejects_bad_data) {
    std::string error;
    CHECK(!jiggy::BkModel::parse(Bytes(0x38, 0), &error));
    Bytes m(0x38, 0);
    put_be32(m, 0, 0x0000000B);
    put_be32(m, 0x10, 0x30); // vertex list header would run past the end
    CHECK(!jiggy::BkModel::parse(m, &error));
}

TEST(rom_import_checks_and_stores) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "jiggy_rom_import_test";
    fs::remove_all(dir);

    // Wrong game, non-ROM and unknown dumps are refused and nothing is written.
    auto wrong = jiggy::import_rom(make_rom(0x100, "NBKE", "Banjo-Kazooie"), jiggy::Game::DonkeyKong64, dir, false);
    CHECK(!wrong.ok);
    CHECK(wrong.message.find("It's Banjo-Kazooie") != std::string::npos);
    CHECK(!jiggy::import_rom(Bytes(0x100, 0x42), jiggy::Game::DonkeyKong64, dir, false).ok);
    auto unknown_dump = jiggy::import_rom(make_rom(0x100, "NBKE", "Banjo-Kazooie"), jiggy::Game::BanjoKazooie, dir);
    CHECK(!unknown_dump.ok);
    CHECK(unknown_dump.message.find("US") != std::string::npos);
    CHECK(!fs::exists(dir / "jiggy_banjo.z64"));

    // A byte-swapped (.v64) dump is stored big-endian under the fixed name.
    Bytes z64 = make_rom(0x100, "NDOE", "DONKEY KONG 64");
    Bytes v64 = z64;
    for (size_t i = 0; i + 1 < v64.size(); i += 2) {
        std::swap(v64[i], v64[i + 1]);
    }
    auto ok = jiggy::import_rom(v64, jiggy::Game::DonkeyKong64, dir, false);
    CHECK(ok.ok);
    CHECK(ok.stored_path == dir / "DK64.z64");
    std::ifstream in(ok.stored_path, std::ios::binary);
    const Bytes stored{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    CHECK(stored == z64);
    CHECK(!fs::exists(dir / "DK64.z64.new"));
    fs::remove_all(dir);
}

JIGGY_TEST_MAIN
