// jiggy-inspect: checks the player's ROMs and browses the files the Jiggy Bridge can serve.
//
//   jiggy-inspect info <rom>...
//   jiggy-inspect list <rom> [dk64-table]
//   jiggy-inspect extract <rom> <bk-asset-id | dk64-table:index> <out-file>
//
// Extracted files are for local research only. Never commit them: they are copyrighted game data.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "jiggy/bk_assets.hpp"
#include "jiggy/dk64_tables.hpp"
#include "jiggy/inflate.hpp"
#include "jiggy/rom.hpp"

namespace {

bool g_allow_unverified = false;

void usage() {
    std::fprintf(stderr,
                 "usage:\n"
                 "  jiggy-inspect info <rom>...\n"
                 "  jiggy-inspect list <rom> [dk64-table]\n"
                 "  jiggy-inspect extract <rom> <bk-asset-id | dk64-table:index> <out-file>\n"
                 "options:\n"
                 "  --allow-unverified   use the US 1.0 offsets on a dump with a different SHA-1\n");
}

std::optional<jiggy::Rom> load(const std::string& path) {
    std::string error;
    auto rom = jiggy::Rom::load_file(path, &error);
    if (!rom) {
        std::fprintf(stderr, "%s: %s\n", path.c_str(), error.c_str());
    }
    return rom;
}

bool usable(const jiggy::Rom& rom, const std::string& path) {
    const auto& info = rom.info();
    if (info.game == jiggy::Game::Unknown) {
        std::fprintf(stderr, "%s: not a Banjo-Kazooie or Donkey Kong 64 ROM\n", path.c_str());
        return false;
    }
    if (!info.supported && !g_allow_unverified) {
        std::fprintf(stderr, "%s: not the US 1.0 dump (SHA-1 %s); pass --allow-unverified to try anyway\n",
                     path.c_str(), info.sha1.c_str());
        return false;
    }
    return true;
}

int cmd_info(const std::vector<std::string>& paths) {
    int status = 0;
    for (const auto& path : paths) {
        auto rom = load(path);
        if (!rom) {
            status = 1;
            continue;
        }
        const auto& info = rom->info();
        std::printf("%s\n", path.c_str());
        std::printf("  game:       %s\n", jiggy::game_name(info.game));
        std::printf("  title:      %s\n", info.internal_name.c_str());
        std::printf("  code:       %s rev %u\n", info.game_code.c_str(), info.revision);
        std::printf("  byte order: %s\n", jiggy::byte_order_name(info.original_order));
        std::printf("  size:       %zu bytes\n", rom->data().size());
        std::printf("  sha1:       %s\n", info.sha1.c_str());
        std::printf("  supported:  %s\n", info.supported ? "yes (matches the US 1.0 dump the recomps target)" : "no");
        if (!info.supported) {
            status = 1;
        }
    }
    return status;
}

int list_bk(const jiggy::Rom& rom) {
    std::string error;
    auto table = jiggy::BkAssetTable::parse(rom.data(), jiggy::BkAssetTable::kUs10RomOffset, &error);
    if (!table) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("%-6s %-10s %-10s %-10s %-4s %-6s %s\n", "id", "rom", "stored", "size", "comp", "type", "name");
    size_t compressed = 0, empty = 0, bad = 0;
    for (size_t id = 0; id < table->size(); id++) {
        const auto& e = table->entry(id);
        if (!e.valid) {
            bad++;
            std::printf("0x%04zx invalid\n", id);
            continue;
        }
        if (e.stored_size == 0) {
            empty++;
            continue;
        }
        auto size = table->uncompressed_size(rom.data(), id, &error);
        char size_text[16] = "?";
        if (size) {
            std::snprintf(size_text, sizeof(size_text), "0x%08x", *size);
        }
        compressed += e.compressed;
        const char* name = jiggy::bk_asset_name(uint32_t(id));
        std::printf("0x%04zx 0x%08x 0x%08x %-10s %-4s 0x%04x %s\n", id, e.rom_offset, e.stored_size, size_text,
                    e.compressed ? "yes" : "no", e.type, name ? name : "");
    }
    std::printf("%zu assets (%zu compressed, %zu empty slots, %zu invalid)\n", table->size(), compressed, empty, bad);
    return bad ? 1 : 0;
}

int list_dk64(const jiggy::Rom& rom, int only_table) {
    std::string error;
    auto tables = jiggy::Dk64PointerTables::parse(rom.data(), jiggy::Dk64PointerTables::kUsRomOffset, &error);
    if (!tables) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (only_table < 0) {
        std::printf("%-5s %-22s %s\n", "table", "name", "files");
        for (uint32_t t = 0; t < jiggy::Dk64PointerTables::kTableCount; t++) {
            std::printf("%-5u %-22s %zu\n", t, jiggy::dk64_table_name(t), tables->file_count(t));
        }
        return 0;
    }
    const uint32_t t = uint32_t(only_table);
    std::printf("%-6s %-10s %-10s %-6s %s\n", "index", "rom", "stored", "gzip", "shared-with");
    for (uint32_t i = 0; i < tables->file_count(t); i++) {
        auto entry = tables->resolve(rom.data(), t, i, &error);
        if (!entry) {
            std::printf("%-6u error: %s\n", i, error.c_str());
            continue;
        }
        const auto stored = rom.data().subspan(entry->rom_offset, entry->stored_size);
        const bool gz = jiggy::is_gzip(stored);
        std::printf("%-6u 0x%08x 0x%08x %-6s", i, entry->rom_offset, entry->stored_size, gz ? "yes" : "no");
        if (entry->resolved_index != i) {
            std::printf(" %u", entry->resolved_index);
        }
        std::printf("\n");
    }
    return 0;
}

int cmd_list(const std::string& path, int table) {
    auto rom = load(path);
    if (!rom || !usable(*rom, path)) {
        return 1;
    }
    if (rom->info().game == jiggy::Game::BanjoKazooie) {
        return list_bk(*rom);
    }
    return list_dk64(*rom, table);
}

int cmd_extract(const std::string& path, const std::string& id, const std::string& out_path) {
    auto rom = load(path);
    if (!rom || !usable(*rom, path)) {
        return 1;
    }
    std::string error;
    std::optional<std::vector<uint8_t>> data;
    if (rom->info().game == jiggy::Game::BanjoKazooie) {
        auto table = jiggy::BkAssetTable::parse(rom->data(), jiggy::BkAssetTable::kUs10RomOffset, &error);
        if (table) {
            data = table->load(rom->data(), std::strtoul(id.c_str(), nullptr, 0), &error);
        }
    } else {
        const auto colon = id.find(':');
        if (colon == std::string::npos) {
            std::fprintf(stderr, "DK64 files are addressed as table:index\n");
            return 1;
        }
        auto tables = jiggy::Dk64PointerTables::parse(rom->data(), jiggy::Dk64PointerTables::kUsRomOffset, &error);
        if (tables) {
            data = tables->load(rom->data(), std::strtoul(id.substr(0, colon).c_str(), nullptr, 0),
                                std::strtoul(id.substr(colon + 1).c_str(), nullptr, 0), &error);
        }
    }
    if (!data) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::ofstream out(out_path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data->data()), std::streamsize(data->size()));
    if (!out) {
        std::fprintf(stderr, "could not write %s\n", out_path.c_str());
        return 1;
    }
    std::printf("wrote %zu bytes to %s\n", data->size(), out_path.c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--allow-unverified") == 0) {
            g_allow_unverified = true;
        } else {
            args.emplace_back(argv[i]);
        }
    }
    if (args.empty()) {
        usage();
        return 2;
    }
    const std::string& cmd = args[0];
    if (cmd == "info" && args.size() >= 2) {
        return cmd_info({args.begin() + 1, args.end()});
    }
    if (cmd == "list" && (args.size() == 2 || args.size() == 3)) {
        return cmd_list(args[1], args.size() == 3 ? std::atoi(args[2].c_str()) : -1);
    }
    if (cmd == "extract" && args.size() == 4) {
        return cmd_extract(args[1], args[2], args[3]);
    }
    usage();
    return 2;
}
