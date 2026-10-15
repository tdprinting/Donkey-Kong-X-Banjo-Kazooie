// jiggy-inspect: checks the player's ROMs and browses the files the Jiggy Bridge can serve.
//
//   jiggy-inspect info <rom>...
//   jiggy-inspect list <rom> [dk64-table]
//   jiggy-inspect extract <rom> <bk-asset-id | dk64-table:index> <out-file>
//   jiggy-inspect sprite <bk-rom> <bk-asset-id> <frame> <out.png>
//   jiggy-inspect model <bk-rom> <bk-asset-id> <out-dir>   (OBJ + MTL + PNG textures)
//   jiggy-inspect sounds <bk-rom> [music]                   (the sound effect or music bank)
//   jiggy-inspect sfx <bk-rom> <sfx-id> <out.wav> [pitch] [volume] [rate]
//   jiggy-inspect jingle <bk-rom> <comusic-id> <out.wav> [volume] [rate]
//
// Extracted files are for local research only. Never commit them: they are copyrighted game data.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "jiggy/bk_assets.hpp"
#include "jiggy/bk_model.hpp"
#include "jiggy/bk_sound.hpp"
#include "jiggy/bk_sprite.hpp"
#include "jiggy/dk64_tables.hpp"
#include "jiggy/inflate.hpp"
#include "jiggy/rom.hpp"
#include "miniz.h"

namespace {

bool g_allow_unverified = false;

void usage() {
    std::fprintf(stderr,
                 "usage:\n"
                 "  jiggy-inspect info <rom>...\n"
                 "  jiggy-inspect list <rom> [dk64-table]\n"
                 "  jiggy-inspect extract <rom> <bk-asset-id | dk64-table:index> <out-file>\n"
                 "  jiggy-inspect sprite <bk-rom> <bk-asset-id> <frame> <out.png>\n"
                 "  jiggy-inspect model <bk-rom> <bk-asset-id> <out-dir>\n"
                 "  jiggy-inspect sounds <bk-rom> [music]\n"
                 "  jiggy-inspect sfx <bk-rom> <sfx-id> <out.wav> [pitch] [volume] [rate]\n"
                 "      sfx ids are the decompilation's sfx_e values; 0x3E9 and up use the music bank.\n"
                 "      volume is BK's 0-32767 argument (default 32767), rate the WAV's (default 48000)\n"
                 "  jiggy-inspect jingle <bk-rom> <comusic-id> <out.wav> [volume] [rate]\n"
                 "      a music track (comusic_e, e.g. 9 note collected, 0xD Jiggy collected)\n"
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

bool write_png(const std::vector<uint8_t>& rgba, int width, int height, const std::string& out_path) {
    size_t png_size = 0;
    void* png = tdefl_write_image_to_png_file_in_memory(rgba.data(), width, height, 4, &png_size);
    if (!png) {
        std::fprintf(stderr, "could not encode the PNG\n");
        return false;
    }
    std::ofstream out(out_path, std::ios::binary);
    out.write(static_cast<const char*>(png), std::streamsize(png_size));
    mz_free(png);
    if (!out) {
        std::fprintf(stderr, "could not write %s\n", out_path.c_str());
        return false;
    }
    return true;
}

int cmd_sprite(const std::string& path, const std::string& id, const std::string& frame, const std::string& out_path) {
    auto rom = load(path);
    if (!rom || !usable(*rom, path)) {
        return 1;
    }
    if (rom->info().game != jiggy::Game::BanjoKazooie) {
        std::fprintf(stderr, "sprites come from the Banjo-Kazooie ROM\n");
        return 1;
    }
    std::string error;
    auto table = jiggy::BkAssetTable::parse(rom->data(), jiggy::BkAssetTable::kUs10RomOffset, &error);
    std::optional<std::vector<uint8_t>> data;
    if (table) {
        data = table->load(rom->data(), std::strtoul(id.c_str(), nullptr, 0), &error);
    }
    std::optional<jiggy::BkSprite> sprite;
    if (data) {
        sprite = jiggy::BkSprite::parse(*data, &error);
    }
    std::optional<jiggy::BkSpriteImage> image;
    if (sprite) {
        image = sprite->decode_frame(std::strtoul(frame.c_str(), nullptr, 0), &error);
    }
    if (!image) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (!write_png(image->rgba, image->width, image->height, out_path)) {
        return 1;
    }
    std::printf("%s sprite, %zu frames; wrote frame %s (%ux%u) to %s\n", jiggy::bk_sprite_format_name(sprite->type()),
                sprite->frame_count(), frame.c_str(), image->width, image->height, out_path.c_str());
    return 0;
}

int cmd_model(const std::string& path, const std::string& id, const std::string& out_dir) {
    auto rom = load(path);
    if (!rom || !usable(*rom, path)) {
        return 1;
    }
    if (rom->info().game != jiggy::Game::BanjoKazooie) {
        std::fprintf(stderr, "models come from the Banjo-Kazooie ROM\n");
        return 1;
    }
    std::string error;
    auto table = jiggy::BkAssetTable::parse(rom->data(), jiggy::BkAssetTable::kUs10RomOffset, &error);
    std::optional<std::vector<uint8_t>> data;
    if (table) {
        data = table->load(rom->data(), std::strtoul(id.c_str(), nullptr, 0), &error);
    }
    std::optional<jiggy::BkModel> model;
    if (data) {
        model = jiggy::BkModel::parse(*data, &error);
    }
    if (!model) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    const std::filesystem::path dir(out_dir);

    std::ofstream mtl(dir / "model.mtl");
    mtl << "newmtl untextured\nKd 1 1 1\n";
    for (size_t t = 0; t < model->textures().size(); t++) {
        const auto& info = model->textures()[t];
        const char* format = jiggy::bk_texture_format_name(info.type);
        auto image = model->decode_texture(t, &error);
        const std::string png = "tex" + std::to_string(t) + ".png";
        if (!image || !write_png(image->rgba, image->width, image->height, (dir / png).string())) {
            std::fprintf(stderr, "texture %zu: %s\n", t, image ? "could not write" : error.c_str());
        }
        std::printf("texture %zu: %s %ux%u\n", t, format ? format : "?", info.width, info.height);
        mtl << "newmtl tex" << t << "\nKd 1 1 1\nmap_Kd " << png << "\n";
    }

    // Vertex colours go after the position, which Blender and most viewers read.
    std::ofstream obj(dir / "model.obj");
    obj << "# Banjo-Kazooie asset " << id << " (" << (jiggy::bk_asset_name(std::strtoul(id.c_str(), nullptr, 0)) ? jiggy::bk_asset_name(std::strtoul(id.c_str(), nullptr, 0)) : "unnamed")
        << "), local research only\nmtllib model.mtl\n";
    for (const auto& v : model->vertices()) {
        obj << "v " << v.position[0] << ' ' << v.position[1] << ' ' << v.position[2] << ' ' << v.color[0] / 255.0f << ' '
            << v.color[1] / 255.0f << ' ' << v.color[2] / 255.0f << "\n";
    }
    int current = -2;
    size_t uv_index = 1;
    for (const auto& tri : model->triangles()) {
        for (const auto& uv : tri.uv) {
            obj << "vt " << uv[0] << ' ' << 1.0f - uv[1] << "\n";
        }
        if (tri.texture != current) {
            current = tri.texture;
            obj << "usemtl " << (current < 0 ? std::string("untextured") : "tex" + std::to_string(current)) << "\n";
        }
        obj << "f";
        for (size_t k = 0; k < 3; k++) {
            obj << ' ' << tri.vertices[k] + 1 << '/' << uv_index++;
        }
        obj << "\n";
    }
    if (!obj || !mtl) {
        std::fprintf(stderr, "could not write to %s\n", out_dir.c_str());
        return 1;
    }
    std::printf("%zu vertices, %zu triangles (header says %u), %zu textures, %zu display list commands; wrote %s\n",
                model->vertices().size(), model->triangles().size(), model->header_triangle_count(),
                model->textures().size(), model->display_list_commands(), (dir / "model.obj").string().c_str());
    return 0;
}

std::optional<jiggy::BkSoundBank> load_bank(const std::string& path, bool music) {
    auto rom = load(path);
    if (!rom || !usable(*rom, path)) {
        return std::nullopt;
    }
    if (rom->info().game != jiggy::Game::BanjoKazooie) {
        std::fprintf(stderr, "sounds come from the Banjo-Kazooie ROM\n");
        return std::nullopt;
    }
    std::string error;
    auto bank = jiggy::BkSoundBank::load(rom->data(), music ? jiggy::kBkUs10MusicSoundfont : jiggy::kBkUs10SfxSoundfont,
                                         &error);
    if (!bank) {
        std::fprintf(stderr, "%s\n", error.c_str());
    }
    return bank;
}

int cmd_sounds(const std::string& path, bool music) {
    auto bank = load_bank(path, music);
    if (!bank) {
        return 1;
    }
    std::printf("%s bank: %zu instruments, sample rate %d\n", music ? "music" : "sfx", bank->instruments().size(),
                bank->sample_rate());
    for (size_t i = 0; i < bank->instruments().size(); i++) {
        const auto& inst = bank->instruments()[i];
        std::printf("instrument %zu: volume %u pan %u, %zu sounds\n", i, inst.volume, inst.pan, inst.sounds.size());
        std::printf("  %-6s %-5s %-8s %-8s %-6s %-4s %-4s %-4s %-4s %-5s %-4s %-4s %-3s %s\n", "sound", "type", "bytes",
                    "att_us", "dec_ms", "rel_ms", "vmin", "vmax", "kmin", "kmax", "base", "det", "vol", "loop");
        for (size_t s = 0; s < inst.sounds.size(); s++) {
            const auto& snd = inst.sounds[s];
            std::printf("  0x%04zx %-5s %-8u %-8d %-6d %-4d %-4u %-4u 0x%02x 0x%02x %-4u %-4d %-3u",
                        s, snd.wave.type == jiggy::BkWaveType::Adpcm ? "adpcm" : "raw16", snd.wave.length,
                        snd.envelope.attack_time, snd.envelope.decay_time < 0 ? -1 : snd.envelope.decay_time / 1000,
                        snd.envelope.release_time / 1000,
                        snd.key_map.velocity_min, snd.key_map.velocity_max, snd.key_map.key_min, snd.key_map.key_max,
                        snd.key_map.key_base, snd.key_map.detune, snd.volume);
            if (snd.wave.has_loop) {
                std::printf(" %u-%u x%d", snd.wave.loop_start, snd.wave.loop_end, int32_t(snd.wave.loop_count));
            }
            std::printf("\n");
        }
    }
    return 0;
}

bool write_rendered(const jiggy::BkRenderedSfx& sfx, const std::string& path, const std::string& what, bool notes) {
    float peak = 0.0f;
    double sum = 0.0;
    for (float v : sfx.stereo) {
        peak = std::max(peak, std::abs(v));
        sum += double(v) * v;
    }
    const double rms = sfx.stereo.empty() ? 0.0 : std::sqrt(sum / double(sfx.stereo.size()));
    const auto wav = jiggy::bk_stereo_to_wav(sfx.stereo, sfx.sample_rate);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(wav.data()), std::streamsize(wav.size()));
    if (!out) {
        std::fprintf(stderr, "could not write %s\n", path.c_str());
        return false;
    }
    std::printf("%s: %zu %s%s, %.3f s at %u Hz, peak %.1f dBFS, rms %.1f dBFS; wrote %s\n", what.c_str(),
                sfx.chain_length, notes ? "note(s)" : "sound(s) in its chain",
                sfx.sustained ? " (cut at the time limit)" : "", double(sfx.frames()) / sfx.sample_rate,
                sfx.sample_rate, peak > 0 ? 20.0 * std::log10(peak) : -999.0, rms > 0 ? 20.0 * std::log10(rms) : -999.0,
                path.c_str());
    return true;
}

int cmd_jingle(const std::vector<std::string>& args) {
    auto rom = load(args[1]);
    if (!rom || !usable(*rom, args[1])) {
        return 1;
    }
    if (rom->info().game != jiggy::Game::BanjoKazooie) {
        std::fprintf(stderr, "music comes from the Banjo-Kazooie ROM\n");
        return 1;
    }
    const uint32_t track = std::strtoul(args[2].c_str(), nullptr, 0);
    std::string error;
    auto table = jiggy::BkAssetTable::parse(rom->data(), jiggy::BkAssetTable::kUs10RomOffset, &error);
    std::optional<std::vector<uint8_t>> sequence;
    if (table) {
        sequence = table->load(rom->data(), jiggy::kBkUs10MusicTrackAssetBase + track, &error);
    }
    std::optional<jiggy::BkSoundBank> bank;
    if (sequence) {
        bank = jiggy::BkSoundBank::load(rom->data(), jiggy::kBkUs10MusicSoundfont, &error);
    }
    jiggy::BkSequenceRenderOptions options;
    if (args.size() > 4) {
        options.volume = std::strtof(args[4].c_str(), nullptr) / 32767.0f;
    }
    if (args.size() > 5) {
        options.output_rate = std::strtoul(args[5].c_str(), nullptr, 0);
    }
    std::optional<jiggy::BkRenderedSfx> rendered;
    if (bank) {
        rendered = jiggy::bk_render_sequence(*bank, *sequence, options, &error);
    }
    if (!rendered) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    return write_rendered(*rendered, args[3], "jingle " + args[2], true) ? 0 : 1;
}

int cmd_sfx(const std::vector<std::string>& args) {
    const uint32_t id = std::strtoul(args[2].c_str(), nullptr, 0);
    const bool music = id >= jiggy::kBkMusicBankSfxBase;
    auto bank = load_bank(args[1], music);
    if (!bank) {
        return 1;
    }
    jiggy::BkSfxRenderOptions options;
    if (args.size() > 4) {
        options.pitch = std::strtof(args[4].c_str(), nullptr);
    }
    if (args.size() > 5) {
        options.volume = std::strtof(args[5].c_str(), nullptr) / 32767.0f;
    }
    if (args.size() > 6) {
        options.output_rate = std::strtoul(args[6].c_str(), nullptr, 0);
    }
    std::string error;
    auto sfx = jiggy::bk_render_sfx(*bank, music ? id - jiggy::kBkMusicBankSfxBase : id, options, &error);
    if (!sfx) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    return write_rendered(*sfx, args[3], "sfx " + args[2], false) ? 0 : 1;
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
    if (cmd == "sprite" && args.size() == 5) {
        return cmd_sprite(args[1], args[2], args[3], args[4]);
    }
    if (cmd == "model" && args.size() == 4) {
        return cmd_model(args[1], args[2], args[3]);
    }
    if (cmd == "sounds" && (args.size() == 2 || (args.size() == 3 && args[2] == "music"))) {
        return cmd_sounds(args[1], args.size() == 3);
    }
    if (cmd == "sfx" && args.size() >= 4 && args.size() <= 7) {
        return cmd_sfx(args);
    }
    if (cmd == "jingle" && args.size() >= 4 && args.size() <= 6) {
        return cmd_jingle(args);
    }
    usage();
    return 2;
}
