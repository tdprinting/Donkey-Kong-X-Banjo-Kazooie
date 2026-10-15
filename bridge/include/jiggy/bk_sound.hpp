// Banjo-Kazooie's sound banks: parsing the libaudio ALBankFile (.ctl) and its wavetable
// (.tbl), decoding a sound's VADPCM samples, and rendering a sound effect the way BK's sound
// player plays it.
//
// Where the banks are (from the CC0 Banjo-Kazooie decompilation, decompressed.us.v10.yaml and
// src/core2/code_AE290.c sfxInstruments_init / src/core1/code_11AC0.c musicInstruments_init):
//
//   soundfont1 (sound effects) ctl 0xD846C0, tbl 0xD954B0
//   soundfont2 (music)         ctl 0xEA3EB0, tbl 0xEADE60   (ends where core1 starts, 0xF19250)
//
// These lie before the compressed code overlays, so the offsets are the same in the retail
// US 1.0 ROM. alBnkfNew turns the ctl's offsets into pointers; here they stay offsets from
// the start of the ctl (wave data offsets are from the start of the tbl).
//
//   ALBankFile  s16 revision (0x4231 'B1'), s16 bankCount, u32 bankArray[bankCount]
//   ALBank      s16 instCount, u8 flags, u8 pad, s32 sampleRate, u32 percussion, u32 instArray[]
//   ALInstrument u8 volume, pan, priority, flags, 8 tremolo/vibrato bytes, s16 bendRange,
//               s16 soundCount, u32 soundArray[soundCount]
//   ALSound     u32 envelope, u32 keyMap, u32 wavetable, u8 samplePan, u8 sampleVolume, u8 flags
//   ALEnvelope  s32 attackTime, decayTime, releaseTime (microseconds), u8 attackVolume, decayVolume
//   ALKeyMap    u8 velocityMin, velocityMax, keyMin, keyMax, keyBase, s8 detune
//   ALWaveTable u32 base, s32 len, u8 type (0 ADPCM, 1 RAW16), u8 flags, 2 pad, u32 loop, u32 book
//   ALADPCMBook s32 order, s32 npredictors, s16 book[order * npredictors * 8]
//   ALADPCMloop u32 start, end, count, s16 state[16]   (ALRawLoop: start, end, count)
//
// BK's sound effect player (src/core1/code_5650.c) uses only instrument 0 of the sfx bank, and
// repurposes the key map fields:
//   sound for sfx id N            soundArray[N] (func_80244608 is passed N + 1)
//   pitch ratio                   alCents2Ratio(keyBase * 100 + detune - 6000), so keyBase 60 plays
//                                 the samples at the synthesizer's output rate (22 kHz). With
//                                 keyMax & 0x20 detune is left out and instead multiplies the pitch
//                                 every 1/30 s once the attack is over (a pitch sweep).
//   start delay                   velocityMax frames of 1/30 s
//   next sound in a chain         velocityMin + (keyMin & 0xC0) * 4 (1-based, 0 ends the chain)
//   channel volume group          keyMin & 0x3F
//   flags                         keyMax & 0xF0 (0x10 repeats, 0x20 pitch sweep); reverb keyMax & 0xF
//   sustained (looping) sound     envelope decayTime == -1
// Ids from 0x3E9 up play the music bank's instrument 0 instead (code_85800.c func_8030CBD0).
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace jiggy {

struct BkSoundfontLocation {
    uint32_t ctl = 0;
    uint32_t tbl = 0;
    uint32_t end = 0; // Where the tbl ends.
};

inline constexpr BkSoundfontLocation kBkUs10SfxSoundfont{0xD846C0, 0xD954B0, 0xEA3EB0};
inline constexpr BkSoundfontLocation kBkUs10MusicSoundfont{0xEA3EB0, 0xEADE60, 0xF19250};
// Sound effect ids at or above this play sound (id - kBkMusicBankSfxBase) of the music bank.
inline constexpr uint32_t kBkMusicBankSfxBase = 0x3E9;

// BK asks the AI for 22000 Hz (src/core1/code_1D00.c); the NTSC DAC gives 48681812 / 2213.
inline constexpr double kBkSynthOutputRate = 48681812.0 / 2213.0;

struct BkEnvelope {
    int32_t attack_time = 0; // Microseconds.
    int32_t decay_time = 0;  // -1 holds the attack volume until the sound is stopped.
    int32_t release_time = 0;
    uint8_t attack_volume = 0; // 0-127
    uint8_t decay_volume = 0;
};

struct BkKeyMap {
    uint8_t velocity_min = 0;
    uint8_t velocity_max = 0;
    uint8_t key_min = 0;
    uint8_t key_max = 0;
    uint8_t key_base = 0;
    int8_t detune = 0;
};

enum class BkWaveType : uint8_t {
    Adpcm = 0,
    Raw16 = 1,
};

struct BkWave {
    uint32_t base = 0;   // Offset into the tbl.
    uint32_t length = 0; // Bytes.
    BkWaveType type = BkWaveType::Adpcm;
    bool has_loop = false;
    uint32_t loop_start = 0; // Samples.
    uint32_t loop_end = 0;
    uint32_t loop_count = 0; // 0xFFFFFFFF loops forever.
    int32_t order = 0;       // ADPCM predictor order and count, and the codebook.
    int32_t predictors = 0;
    std::vector<int16_t> book; // order * predictors * 8 coefficients.
};

struct BkSound {
    BkEnvelope envelope;
    BkKeyMap key_map;
    BkWave wave;
    uint8_t pan = 64;     // 0 left, 127 right.
    uint8_t volume = 127; // 0-127
    uint8_t flags = 0;
};

struct BkInstrument {
    uint8_t volume = 0;
    uint8_t pan = 0;
    uint8_t priority = 0;
    int16_t bend_range = 0;
    std::vector<BkSound> sounds;
};

// Decodes a whole wave to 16-bit PCM at its own rate (no loop unrolling).
std::optional<std::vector<int16_t>> bk_decode_wave(const BkWave& wave, std::span<const uint8_t> tbl,
                                                   std::string* error);

// Decodes one 9-byte VADPCM frame into 16 samples. `state` holds the previous decoded samples
// (state[order - 1] the most recent) and is updated.
void bk_vadpcm_decode_frame(const uint8_t frame[9], const int16_t* book, int32_t order, int32_t predictors,
                            int16_t* state, int16_t out[16]);

float bk_cents_to_ratio(float cents);

class BkSoundBank {
public:
    // ctl and tbl are the two files, each starting at its own offset 0. Keeps a copy of both.
    static std::optional<BkSoundBank> parse(std::span<const uint8_t> ctl, std::span<const uint8_t> tbl,
                                            std::string* error);
    static std::optional<BkSoundBank> load(std::span<const uint8_t> rom, const BkSoundfontLocation& where,
                                           std::string* error);

    int32_t sample_rate() const { return sample_rate_; }
    const std::vector<BkInstrument>& instruments() const { return instruments_; }
    std::span<const uint8_t> tbl() const { return *tbl_; }

    // The decoded samples of instrument `inst`'s sound `sound`, decoded once and kept.
    // Not thread-safe: guard a shared bank with a lock.
    const std::vector<int16_t>* pcm(size_t inst, size_t sound, std::string* error) const;

private:
    int32_t sample_rate_ = 0;
    std::vector<BkInstrument> instruments_;
    std::shared_ptr<std::vector<uint8_t>> tbl_;
    mutable std::map<std::pair<size_t, size_t>, std::vector<int16_t>> pcm_cache_;
};

struct BkSfxRenderOptions {
    float pitch = 1.0f;  // BK's sfx "pitch" argument (func_8030E760's second one).
    float volume = 1.0f; // BK's sfx volume argument / 32767 (0x7FFF is 1.0).
    uint32_t output_rate = 48000;
    // Sustained sounds (decay time -1) and loops play this long before fading out.
    float max_seconds = 3.0f;
};

struct BkRenderedSfx {
    uint32_t sample_rate = 0;
    std::vector<float> stereo; // Interleaved L R; 1.0 is 16-bit full scale, as the N64 mixes.
    size_t chain_length = 0;   // Sounds played (one, or more for a chained effect; notes for music).
    bool sustained = false;    // Cut at max_seconds instead of ending by itself.

    size_t frames() const { return stereo.size() / 2; }
};

// Renders a sound effect of instrument 0, with its chain, envelope, pitch and pan, the way BK's
// sound player schedules it (src/core1/code_5650.c func_80244608 / func_802432F8).
// `sound` is the 0-based index into the instrument's sounds, i.e. the sfx id (or id - 0x3E9
// for the music bank).
std::optional<BkRenderedSfx> bk_render_sfx(const BkSoundBank& bank, uint32_t sound, const BkSfxRenderOptions& options,
                                           std::string* error);

// Music jingles (the note and Jiggy pickups) are COMUSIC tracks: compact MIDI sequences
// (libaudio ALCSeq, src/core1/n_audio/n_csq.c) in the asset bank, played on the music
// soundfont by BK's n_csplayer.c. Track N is asset kBkUs10MusicTrackAssetBase + N.
inline constexpr uint32_t kBkUs10MusicTrackAssetBase = 0x1516;

struct BkSequenceRenderOptions {
    float volume = 1.0f; // The sequence player's volume / 32767 (coMusicPlayer_playMusic's).
    uint32_t output_rate = 48000;
    float max_seconds = 12.0f; // Looping music stops (fading each note's release) here.
};

// Renders a compact MIDI sequence on a bank the way BK's sequence player plays it: program
// changes pick instruments, notes pick sounds by key and velocity, pitch from the key against
// the sound's keyBase, channel volume, pan, sustain and pitch bend. No tremolo or vibrato
// (BK's music players have no oscillators) and no voice limit. chain_length counts notes.
std::optional<BkRenderedSfx> bk_render_sequence(const BkSoundBank& bank, std::span<const uint8_t> sequence,
                                                const BkSequenceRenderOptions& options, std::string* error);

// Encodes interleaved stereo floats as a 16-bit PCM WAV file.
std::vector<uint8_t> bk_stereo_to_wav(std::span<const float> stereo, uint32_t sample_rate);

} // namespace jiggy
