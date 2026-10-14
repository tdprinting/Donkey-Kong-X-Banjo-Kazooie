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

#include "jiggy/bk_assets.hpp"
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
