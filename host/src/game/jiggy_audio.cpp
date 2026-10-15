// Banjo-Kazooie sound effects and jingles, played from the player's Banjo-Kazooie ROM and
// mixed into DK64's audio (see include/jiggy_audio.h).
//
// A play request renders the whole sound up front with the Jiggy Bridge (a sound effect is a
// few hundred milliseconds, so this is cheap) at the game's output rate, keeps the result in a
// small cache, and hands it to one of a fixed set of voices. The audio thread only steps
// through finished buffers, so it never waits on the ROM or the decoder.

#include "jiggy_audio.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <tuple>

#include "donk_sound.h"
#include "jiggy/bk_sound.hpp"
#include "jiggy_host.h"
#include "librecomp/helpers.hpp"
#include "recomp.h"

namespace {
    using Rendered = std::shared_ptr<const jiggy::BkRenderedSfx>;

    // Sound banks and rendered sounds. Rendering happens on the game thread under this lock.
    std::mutex bank_mutex;
    uint32_t bank_generation = 0; // jiggy_host::banjo_rom_generation() the banks came from.
    std::optional<jiggy::BkSoundBank> sfx_bank;
    std::optional<jiggy::BkSoundBank> music_bank;
    bool sfx_bank_failed = false;
    bool music_bank_failed = false;
    // Rendered sounds by (kind, id, pitch and volume in thousandths, rate). Footsteps and the
    // like replay the same few variations, so a small cache saves most of the work.
    using CacheKey = std::tuple<int, uint32_t, int32_t, int32_t, uint32_t>;
    std::map<CacheKey, Rendered> render_cache;
    constexpr size_t max_cached_renders = 96;

    // The rate the audio thread last mixed at: sounds are rendered at it.
    std::atomic<uint32_t> game_sample_rate{0};
    constexpr uint32_t fallback_sample_rate = 32000;

    // The voices the audio thread mixes.
    struct Voice {
        Rendered sound;
        double position = 0.0; // Frames into the sound, at the sound's own rate.
        float gain = 1.0f;
        int32_t handle = 0;
    };
    constexpr size_t voice_count = 16;
    std::mutex voice_mutex;
    std::array<Voice, voice_count> voices;
    int32_t next_handle = 1;

    // Expects bank_mutex to be held. Drops everything made from a previous ROM.
    void sync_with_rom() {
        const uint32_t generation = jiggy_host::banjo_rom_generation();
        if (generation == bank_generation) {
            return;
        }
        bank_generation = generation;
        sfx_bank.reset();
        music_bank.reset();
        sfx_bank_failed = false;
        music_bank_failed = false;
        render_cache.clear();
    }

    // Expects bank_mutex to be held.
    const jiggy::BkSoundBank* get_bank(bool music) {
        auto& bank = music ? music_bank : sfx_bank;
        bool& failed = music ? music_bank_failed : sfx_bank_failed;
        if (bank || failed) {
            return bank ? &*bank : nullptr;
        }
        const jiggy::BkSoundfontLocation& where = music ? jiggy::kBkUs10MusicSoundfont : jiggy::kBkUs10SfxSoundfont;
        std::string error = "no Banjo-Kazooie ROM";
        auto bytes = jiggy_host::banjo_rom_bytes(where.ctl, where.end - where.ctl);
        if (bytes) {
            const uint32_t ctl_size = where.tbl - where.ctl;
            bank = jiggy::BkSoundBank::parse(std::span<const uint8_t>(*bytes).first(ctl_size),
                                             std::span<const uint8_t>(*bytes).subspan(ctl_size), &error);
        }
        if (!bank) {
            failed = true; // Don't retry every frame; a new ROM resets this.
            if (bytes) {
                fprintf(stderr, "[Jiggy] Banjo-Kazooie %s bank unusable: %s\n", music ? "music" : "sound effect",
                        error.c_str());
            }
            return nullptr;
        }
        return &*bank;
    }

    uint32_t render_rate() {
        const uint32_t rate = game_sample_rate.load(std::memory_order_relaxed);
        return rate >= 4000 && rate <= 192000 ? rate : fallback_sample_rate;
    }

    Rendered cached(const CacheKey& key) {
        auto it = render_cache.find(key);
        return it != render_cache.end() ? it->second : nullptr;
    }

    void remember(const CacheKey& key, const Rendered& sound) {
        if (render_cache.size() >= max_cached_renders) {
            render_cache.clear();
        }
        render_cache.emplace(key, sound);
    }

    Rendered render_sfx(uint32_t sfx_id, float volume, float pitch) {
        std::lock_guard lock{bank_mutex};
        sync_with_rom();
        const bool music = sfx_id >= jiggy::kBkMusicBankSfxBase;
        const uint32_t rate = render_rate();
        const CacheKey key{0, sfx_id, int32_t(std::lround(pitch * 1000.0f)), int32_t(std::lround(volume * 1000.0f)), rate};
        if (Rendered hit = cached(key)) {
            return hit;
        }
        const jiggy::BkSoundBank* bank = get_bank(music);
        if (!bank) {
            return nullptr;
        }
        jiggy::BkSfxRenderOptions options;
        options.pitch = pitch;
        options.volume = volume;
        options.output_rate = rate;
        std::string error;
        auto sfx = jiggy::bk_render_sfx(*bank, music ? sfx_id - jiggy::kBkMusicBankSfxBase : sfx_id, options, &error);
        if (!sfx) {
            fprintf(stderr, "[Jiggy] Banjo-Kazooie sound effect 0x%X: %s\n", sfx_id, error.c_str());
            return nullptr;
        }
        Rendered out = std::make_shared<const jiggy::BkRenderedSfx>(std::move(*sfx));
        remember(key, out);
        return out;
    }

    Rendered render_jingle(uint32_t track, float volume) {
        std::lock_guard lock{bank_mutex};
        sync_with_rom();
        const uint32_t rate = render_rate();
        const CacheKey key{1, track, 0, int32_t(std::lround(volume * 1000.0f)), rate};
        if (Rendered hit = cached(key)) {
            return hit;
        }
        const jiggy::BkSoundBank* bank = get_bank(true);
        if (!bank) {
            return nullptr;
        }
        auto sequence = jiggy_host::banjo_asset(jiggy::kBkUs10MusicTrackAssetBase + track);
        if (!sequence) {
            return nullptr;
        }
        jiggy::BkSequenceRenderOptions options;
        options.volume = volume;
        options.output_rate = rate;
        options.max_seconds = 8.0f; // Jingles are a few seconds; level music loops forever.
        std::string error;
        auto out = jiggy::bk_render_sequence(*bank, *sequence, options, &error);
        if (!out) {
            fprintf(stderr, "[Jiggy] Banjo-Kazooie music track %u: %s\n", track, error.c_str());
            return nullptr;
        }
        Rendered rendered = std::make_shared<const jiggy::BkRenderedSfx>(std::move(*out));
        remember(key, rendered);
        return rendered;
    }

    // Starts a voice, taking a free one or else the one nearest its end. Returns its handle.
    int32_t start_voice(Rendered sound, float gain) {
        if (!sound || sound->frames() == 0) {
            return 0;
        }
        std::lock_guard lock{voice_mutex};
        Voice* pick = nullptr;
        double least_left = 0.0;
        for (Voice& v : voices) {
            if (!v.sound) {
                pick = &v;
                break;
            }
            const double left = double(v.sound->frames()) - v.position;
            if (!pick || left < least_left) {
                pick = &v;
                least_left = left;
            }
        }
        pick->sound = std::move(sound);
        pick->position = 0.0;
        pick->gain = gain;
        pick->handle = next_handle;
        next_handle = next_handle == INT32_MAX ? 1 : next_handle + 1;
        return pick->handle;
    }

    float as_float(uint32_t bits) {
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    float sanitize(float value, float fallback, float lo, float hi) {
        if (!std::isfinite(value) || value <= 0.0f) {
            return fallback;
        }
        return std::clamp(value, lo, hi);
    }
}

void jiggy_host::audio::mix(float* stereo, size_t frames, uint32_t sample_rate, float gain) {
    game_sample_rate.store(sample_rate, std::memory_order_relaxed);
    if (sample_rate == 0) {
        return;
    }
    std::lock_guard lock{voice_mutex};
    for (Voice& v : voices) {
        if (!v.sound) {
            continue;
        }
        const jiggy::BkRenderedSfx& s = *v.sound;
        const size_t length = s.frames();
        // Sounds are rendered at the game's rate, so this is normally exactly one frame.
        const double step = double(s.sample_rate) / double(sample_rate);
        const float g = v.gain * gain;
        size_t i = 0;
        for (; i < frames; i++) {
            const size_t at = size_t(v.position);
            if (at >= length) {
                break;
            }
            const float frac = float(v.position - double(at));
            const size_t next = at + 1 < length ? at + 1 : at;
            const float l = s.stereo[at * 2] + (s.stereo[next * 2] - s.stereo[at * 2]) * frac;
            const float r = s.stereo[at * 2 + 1] + (s.stereo[next * 2 + 1] - s.stereo[at * 2 + 1]) * frac;
            stereo[i * 2] += l * g;
            stereo[i * 2 + 1] += r * g;
            v.position += step;
        }
        if (size_t(v.position) >= length) {
            v.sound.reset(); // The cache may still hold it; the voice is free.
        }
    }
}

// s32 jiggy_bk_sfx_play(u32 sfx_id, f32 volume, f32 pitch): plays Banjo-Kazooie sound effect
// `sfx_id` (the decompilation's sfx_e) at `volume` (BK's 0-32767 argument / 32767, so 1.0 is
// full) and `pitch` (1.0 as recorded). Returns a handle for jiggy_bk_sound_stop, or 0 when
// there's no Banjo-Kazooie ROM or no such sound, so the caller can fall back to a DK64 sound.
extern "C" void jiggy_bk_sfx_play(uint8_t* rdram, recomp_context* ctx) {
    const u32 id = _arg<0, u32>(rdram, ctx);
    // o32: after an integer argument, floats travel in a1 and a2.
    const float volume = sanitize(as_float(_arg<1, u32>(rdram, ctx)), 0.0f, 0.0f, 1.0f);
    const float pitch = sanitize(as_float(_arg<2, u32>(rdram, ctx)), 1.0f, 0.05f, 8.0f);
    if (volume <= 0.0f || !jiggy_host::banjo_rom_loaded()) {
        _return<s32>(ctx, 0);
        return;
    }
    const float setting = std::clamp(dk64::get_sfx_volume(), 0, 100) / 100.0f;
    _return<s32>(ctx, start_voice(render_sfx(id, volume, pitch), setting));
}

// s32 jiggy_bk_jingle_play(u32 track, f32 volume): plays Banjo-Kazooie music track `track`
// (the decompilation's comusic_e, e.g. 9 for the note pickup) at `volume` (BK's 0-32767
// sequence volume / 32767). Up to 8 seconds; meant for the short pickup jingles. Returns a
// handle, or 0 like jiggy_bk_sfx_play.
extern "C" void jiggy_bk_jingle_play(uint8_t* rdram, recomp_context* ctx) {
    const u32 track = _arg<0, u32>(rdram, ctx);
    const float volume = sanitize(as_float(_arg<1, u32>(rdram, ctx)), 0.0f, 0.0f, 1.0f);
    if (volume <= 0.0f || !jiggy_host::banjo_rom_loaded()) {
        _return<s32>(ctx, 0);
        return;
    }
    const float setting = std::clamp(dk64::get_bgm_volume(), 0, 100) / 100.0f;
    _return<s32>(ctx, start_voice(render_jingle(track, volume), setting));
}

// s32 jiggy_bk_sound_stop(s32 handle): stops a sound started by the two above. Returns 1 if
// it was still playing.
extern "C" void jiggy_bk_sound_stop(uint8_t* rdram, recomp_context* ctx) {
    const s32 handle = _arg<0, s32>(rdram, ctx);
    s32 stopped = 0;
    if (handle > 0) {
        std::lock_guard lock{voice_mutex};
        for (Voice& v : voices) {
            if (v.sound && v.handle == handle) {
                v.sound.reset();
                stopped = 1;
            }
        }
    }
    _return<s32>(ctx, stopped);
}
