#include "jiggy/bk_sound.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "byte_io.hpp"

namespace jiggy {

namespace {

void set_error(std::string* error, std::string message) {
    if (error) {
        *error = std::move(message);
    }
}

constexpr uint16_t kBankRevision = 0x4231; // 'B1'
// Real banks hold a few hundred sounds; these only reject garbage.
constexpr int kMaxBanks = 16;
constexpr int kMaxInstruments = 256;
constexpr int kMaxSounds = 4096;
constexpr int kMaxPredictors = 16;
constexpr int kMaxOrder = 8;

// The synthesizer's resampler can't play faster than this (libaudio MAX_RATIO).
constexpr double kMaxPitchRatio = 1.99996;
// BK's sound player measures delays in 1/30 s (0x8235 microseconds).
constexpr double kFrameMicros = 33333.0;
// A chain that hasn't ended after this many sounds is treated as a loop in the data.
constexpr size_t kMaxChain = 32;

std::string hex(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%X", v);
    return buf;
}

int16_t clamp16(int32_t v) {
    return int16_t(std::clamp<int32_t>(v, -32768, 32767));
}

std::optional<BkWave> parse_wave(std::span<const uint8_t> ctl, uint32_t at, std::string* error) {
    if (!range_ok(ctl, at, 20)) {
        set_error(error, "wavetable " + hex(at) + " is outside the bank");
        return std::nullopt;
    }
    BkWave wave;
    wave.base = read_be32(ctl, at);
    wave.length = read_be32(ctl, at + 4);
    const uint8_t type = ctl[at + 8];
    if (type > 1) {
        set_error(error, "wavetable " + hex(at) + " has unknown type " + std::to_string(type));
        return std::nullopt;
    }
    wave.type = BkWaveType(type);
    const uint32_t loop = read_be32(ctl, at + 12);
    if (loop != 0) {
        if (!range_ok(ctl, loop, 12)) {
            set_error(error, "wave loop " + hex(loop) + " is outside the bank");
            return std::nullopt;
        }
        wave.loop_start = read_be32(ctl, loop);
        wave.loop_end = read_be32(ctl, loop + 4);
        wave.loop_count = read_be32(ctl, loop + 8);
        wave.has_loop = wave.loop_count != 0 && wave.loop_end > wave.loop_start;
    }
    if (wave.type == BkWaveType::Adpcm) {
        const uint32_t book = read_be32(ctl, at + 16);
        if (book == 0 || !range_ok(ctl, book, 8)) {
            set_error(error, "ADPCM wave " + hex(at) + " has no codebook");
            return std::nullopt;
        }
        wave.order = int32_t(read_be32(ctl, book));
        wave.predictors = int32_t(read_be32(ctl, book + 4));
        if (wave.order < 1 || wave.order > kMaxOrder || wave.predictors < 1 || wave.predictors > kMaxPredictors) {
            set_error(error, "ADPCM wave " + hex(at) + " has an implausible codebook");
            return std::nullopt;
        }
        const size_t count = size_t(wave.order) * size_t(wave.predictors) * 8;
        if (!range_ok(ctl, uint64_t(book) + 8, count * 2)) {
            set_error(error, "ADPCM codebook " + hex(book) + " runs past the bank");
            return std::nullopt;
        }
        wave.book.resize(count);
        for (size_t i = 0; i < count; i++) {
            wave.book[i] = int16_t(read_be16(ctl, book + 8 + i * 2));
        }
    }
    return wave;
}

std::optional<BkSound> parse_sound(std::span<const uint8_t> ctl, uint32_t at, std::string* error) {
    if (!range_ok(ctl, at, 15)) {
        set_error(error, "sound " + hex(at) + " is outside the bank");
        return std::nullopt;
    }
    const uint32_t env = read_be32(ctl, at);
    const uint32_t keymap = read_be32(ctl, at + 4);
    const uint32_t wavetable = read_be32(ctl, at + 8);
    if (!range_ok(ctl, env, 14) || !range_ok(ctl, keymap, 6)) {
        set_error(error, "sound " + hex(at) + " points outside the bank");
        return std::nullopt;
    }
    BkSound sound;
    sound.envelope.attack_time = int32_t(read_be32(ctl, env));
    sound.envelope.decay_time = int32_t(read_be32(ctl, env + 4));
    sound.envelope.release_time = int32_t(read_be32(ctl, env + 8));
    sound.envelope.attack_volume = ctl[env + 12];
    sound.envelope.decay_volume = ctl[env + 13];
    sound.key_map.velocity_min = ctl[keymap];
    sound.key_map.velocity_max = ctl[keymap + 1];
    sound.key_map.key_min = ctl[keymap + 2];
    sound.key_map.key_max = ctl[keymap + 3];
    sound.key_map.key_base = ctl[keymap + 4];
    sound.key_map.detune = int8_t(ctl[keymap + 5]);
    auto wave = parse_wave(ctl, wavetable, error);
    if (!wave) {
        return std::nullopt;
    }
    sound.wave = std::move(*wave);
    sound.pan = ctl[at + 12];
    sound.volume = ctl[at + 13];
    sound.flags = ctl[at + 14];
    return sound;
}

} // namespace

float bk_cents_to_ratio(float cents) {
    return float(std::pow(2.0, double(cents) / 1200.0));
}

void bk_vadpcm_decode_frame(const uint8_t frame[9], const int16_t* book, int32_t order, int32_t predictors,
                            int16_t* state, int16_t out[16]) {
    const int scale = frame[0] >> 4;
    int predictor = frame[0] & 0xF;
    if (predictor >= predictors) {
        predictor = 0; // Corrupt data; keep going rather than read past the book.
    }
    const int16_t* coefs = book + size_t(predictor) * size_t(order) * 8;
    const int16_t* last_row = coefs + size_t(order - 1) * 8;

    for (int half = 0; half < 2; half++) {
        int32_t residual[8];
        for (int i = 0; i < 8; i++) {
            const uint8_t byte = frame[1 + half * 4 + i / 2];
            int32_t nibble = (i & 1) ? (byte & 0xF) : (byte >> 4);
            if (nibble >= 8) {
                nibble -= 16;
            }
            residual[i] = nibble * (1 << scale);
        }
        int16_t* dst = out + half * 8;
        for (int i = 0; i < 8; i++) {
            // Row j weighs the sample `order - j` back; the last row also folds in this
            // group's earlier residuals (the RSP's closed form of the recursive predictor).
            int64_t total = 0;
            for (int j = 0; j < order; j++) {
                total += int64_t(coefs[j * 8 + i]) * state[j];
            }
            for (int k = 0; k < i; k++) {
                total += int64_t(last_row[i - 1 - k]) * residual[k];
            }
            total += int64_t(residual[i]) << 11;
            dst[i] = clamp16(int32_t(total >> 11));
        }
        // The new state is this group's last `order` samples.
        for (int j = 0; j < order; j++) {
            state[j] = dst[8 - order + j];
        }
    }
}

std::optional<std::vector<int16_t>> bk_decode_wave(const BkWave& wave, std::span<const uint8_t> tbl,
                                                   std::string* error) {
    if (!range_ok(tbl, wave.base, wave.length)) {
        set_error(error, "wave data " + hex(wave.base) + "+" + hex(wave.length) + " is outside the wavetable");
        return std::nullopt;
    }
    const auto data = tbl.subspan(wave.base, wave.length);
    std::vector<int16_t> pcm;
    if (wave.type == BkWaveType::Raw16) {
        pcm.resize(data.size() / 2);
        for (size_t i = 0; i < pcm.size(); i++) {
            pcm[i] = int16_t(read_be16(data, i * 2));
        }
        return pcm;
    }
    if (wave.order < 1 || wave.order > kMaxOrder ||
        wave.book.size() != size_t(wave.order) * size_t(wave.predictors) * 8) {
        set_error(error, "ADPCM wave has no usable codebook");
        return std::nullopt;
    }
    const size_t frames = data.size() / 9;
    pcm.resize(frames * 16);
    int16_t state[kMaxOrder] = {};
    for (size_t f = 0; f < frames; f++) {
        bk_vadpcm_decode_frame(&data[f * 9], wave.book.data(), wave.order, wave.predictors, state, &pcm[f * 16]);
    }
    return pcm;
}

std::optional<BkSoundBank> BkSoundBank::parse(std::span<const uint8_t> ctl, std::span<const uint8_t> tbl,
                                              std::string* error) {
    if (!range_ok(ctl, 0, 8) || read_be16(ctl, 0) != kBankRevision) {
        set_error(error, "not a libaudio sound bank (no 'B1' header)");
        return std::nullopt;
    }
    const int16_t bank_count = int16_t(read_be16(ctl, 2));
    if (bank_count < 1 || bank_count > kMaxBanks) {
        set_error(error, "sound bank file has an implausible bank count");
        return std::nullopt;
    }
    // BK only ever uses bankArray[0].
    const uint32_t bank = read_be32(ctl, 4);
    if (!range_ok(ctl, bank, 12)) {
        set_error(error, "sound bank " + hex(bank) + " is outside the file");
        return std::nullopt;
    }
    const int16_t inst_count = int16_t(read_be16(ctl, bank));
    if (inst_count < 1 || inst_count > kMaxInstruments || !range_ok(ctl, uint64_t(bank) + 12, uint64_t(inst_count) * 4)) {
        set_error(error, "sound bank has an implausible instrument count");
        return std::nullopt;
    }
    BkSoundBank out;
    out.sample_rate_ = int32_t(read_be32(ctl, bank + 4));
    for (int i = 0; i < inst_count; i++) {
        const uint32_t inst = read_be32(ctl, bank + 12 + uint32_t(i) * 4);
        BkInstrument instrument;
        if (inst == 0) {
            out.instruments_.push_back(std::move(instrument)); // An unused program slot.
            continue;
        }
        if (!range_ok(ctl, inst, 16)) {
            set_error(error, "instrument " + std::to_string(i) + " is outside the bank");
            return std::nullopt;
        }
        instrument.volume = ctl[inst];
        instrument.pan = ctl[inst + 1];
        instrument.priority = ctl[inst + 2];
        instrument.bend_range = int16_t(read_be16(ctl, inst + 12));
        const int16_t sound_count = int16_t(read_be16(ctl, inst + 14));
        if (sound_count < 0 || sound_count > kMaxSounds ||
            !range_ok(ctl, uint64_t(inst) + 16, uint64_t(sound_count) * 4)) {
            set_error(error, "instrument " + std::to_string(i) + " has an implausible sound count");
            return std::nullopt;
        }
        instrument.sounds.reserve(size_t(sound_count));
        for (int s = 0; s < sound_count; s++) {
            std::string sound_error;
            auto sound = parse_sound(ctl, read_be32(ctl, inst + 16 + uint32_t(s) * 4), &sound_error);
            if (!sound) {
                set_error(error, "instrument " + std::to_string(i) + " sound " + std::to_string(s) + ": " + sound_error);
                return std::nullopt;
            }
            instrument.sounds.push_back(std::move(*sound));
        }
        out.instruments_.push_back(std::move(instrument));
    }
    out.tbl_ = std::make_shared<std::vector<uint8_t>>(tbl.begin(), tbl.end());
    return out;
}

std::optional<BkSoundBank> BkSoundBank::load(std::span<const uint8_t> rom, const BkSoundfontLocation& where,
                                             std::string* error) {
    if (where.ctl >= where.tbl || where.tbl >= where.end || !range_ok(rom, where.ctl, where.end - where.ctl)) {
        set_error(error, "sound bank location is outside the ROM");
        return std::nullopt;
    }
    return parse(rom.subspan(where.ctl, where.tbl - where.ctl), rom.subspan(where.tbl, where.end - where.tbl), error);
}

const std::vector<int16_t>* BkSoundBank::pcm(size_t inst, size_t sound, std::string* error) const {
    if (inst >= instruments_.size() || sound >= instruments_[inst].sounds.size()) {
        set_error(error, "no sound " + std::to_string(sound) + " in instrument " + std::to_string(inst));
        return nullptr;
    }
    const auto key = std::make_pair(inst, sound);
    auto it = pcm_cache_.find(key);
    if (it != pcm_cache_.end()) {
        return &it->second;
    }
    auto decoded = bk_decode_wave(instruments_[inst].sounds[sound].wave, *tbl_, error);
    if (!decoded) {
        return nullptr;
    }
    return &pcm_cache_.emplace(key, std::move(*decoded)).first->second;
}

namespace {

// A wave as the synthesizer plays it: the loop unrolled `loop_count` times (forever for
// 0xFFFFFFFF), then the samples after the loop end.
struct UnrolledWave {
    const std::vector<int16_t>* pcm = nullptr;
    bool looping = false;
    bool forever = false;
    int64_t loop_start = 0;
    int64_t loop_end = 0;
    int64_t loop_len = 0;
    int64_t unrolled_end = 0; // loop_end + loop_len * loop_count when finite.
    int64_t length = 0;       // Unrolled samples (unused when forever).

    UnrolledWave(const BkWave& wave, const std::vector<int16_t>& samples) : pcm(&samples) {
        looping = wave.has_loop && wave.loop_end <= samples.size() && wave.loop_end > wave.loop_start;
        length = int64_t(samples.size());
        if (looping) {
            loop_start = wave.loop_start;
            loop_end = wave.loop_end;
            loop_len = loop_end - loop_start;
            forever = wave.loop_count == 0xFFFFFFFF;
            if (!forever) {
                unrolled_end = loop_end + loop_len * int64_t(wave.loop_count);
                length += loop_len * int64_t(wave.loop_count);
            }
        }
    }

    float at(int64_t index) const {
        if (index < 0) {
            return 0.0f;
        }
        if (looping && index >= loop_end) {
            if (forever || index < unrolled_end) {
                index = loop_start + (index - loop_start) % loop_len;
            } else {
                index -= unrolled_end - loop_end;
            }
        }
        return index < int64_t(pcm->size()) ? float((*pcm)[size_t(index)]) : 0.0f;
    }
};

// A change that applies from `t` microseconds after the voice starts.
struct PitchStep {
    double t = 0;
    double ratio = 1; // Unclamped resampling ratio against the synthesizer's output rate.
};
struct GainStep {
    double t = 0;
    float left = 0; // Amplitude per envelope level unit, pan included.
    float right = 0;
};

// One synthesizer voice: a sound, its envelope schedule and how pitch and gain change.
struct VoicePlan {
    const BkSound* sound = nullptr;
    const std::vector<int16_t>* pcm = nullptr;
    double start = 0;      // Microseconds from the start of the render.
    double attack = 0;     // Ramp from 0 to `peak` over this long.
    double peak = 0;       // Envelope levels are 0-127.
    double decay = 0;      // Then ramp to `sustain` over this long...
    double sustain = 0;
    bool hold = false;     // ...or stay at `peak` (a decay time of -1).
    double note_off = -1;  // From the voice start, when the release begins; -1 never.
    double release_time = 0;
    bool release_scales_with_pitch = false; // BK's sfx player divides it by the pitch.
    std::vector<PitchStep> pitch; // Sorted, the first at t = 0.
    std::vector<GainStep> gain;   // Sorted, the first at t = 0.
};

// Equal-power pan, 0 hard left, 127 hard right (the synthesizer's eqpower table).
void gain_unit_from_pan(int pan, float amplitude, float* left, float* right) {
    const double angle = std::clamp(pan, 0, 127) / 127.0 * std::numbers::pi / 2.0;
    *left = float(std::cos(angle)) * amplitude;
    *right = float(std::sin(angle)) * amplitude;
}

// Plays a voice into `out` (interleaved stereo at `rate`, 1.0 = 16-bit full scale). Returns
// false when the caller's time limit cut it rather than it ending by itself.
bool render_voice(const VoicePlan& v, uint32_t rate, double stop_micros, std::vector<float>& out) {
    const UnrolledWave wave(v.sound->wave, *v.pcm);
    auto level_before_release = [&](double t) {
        if (t < v.attack) {
            return v.peak * t / v.attack;
        }
        if (v.hold) {
            return v.peak;
        }
        if (t >= v.attack + v.decay) {
            return v.sustain;
        }
        return v.peak + (v.sustain - v.peak) * (t - v.attack) / v.decay;
    };
    double release_start = v.note_off;
    double release_len = -1.0;
    double release_from = 0.0;
    bool cut = false;

    const double micros_per_frame = 1e6 / rate;
    const size_t start_frame = size_t(v.start / micros_per_frame);
    size_t pitch_index = 0;
    size_t gain_index = 0;
    double position = 0.0; // Unrolled samples into the wave.
    for (size_t frame = 0;; frame++) {
        const double t = double(frame) * micros_per_frame;
        while (pitch_index + 1 < v.pitch.size() && v.pitch[pitch_index + 1].t <= t) {
            pitch_index++;
        }
        while (gain_index + 1 < v.gain.size() && v.gain[gain_index + 1].t <= t) {
            gain_index++;
        }
        const double ratio = v.pitch[pitch_index].ratio;
        // The time limit stops a sound that hasn't been released: a hold, a loop, a long note.
        if ((release_start < 0 || release_start > t) && v.start + t >= stop_micros) {
            release_start = t;
            cut = true;
        }
        double level;
        if (release_start < 0 || t < release_start) {
            level = level_before_release(t);
        } else {
            if (release_len < 0) {
                release_len = std::max(0.0, v.release_scales_with_pitch ? v.release_time / ratio : v.release_time);
                release_from = level_before_release(release_start);
            }
            const double r = release_len > 0 ? (t - release_start) / release_len : 1.0;
            if (r >= 1.0) {
                break;
            }
            level = release_from * (1.0 - r);
        }
        if (!wave.forever && position >= double(wave.length)) {
            break; // The samples ran out; the voice goes silent.
        }

        // 4-point cubic interpolation between the wave's samples.
        const int64_t i = int64_t(position);
        const float f = float(position - double(i));
        const float p0 = wave.at(i - 1);
        const float p1 = wave.at(i);
        const float p2 = wave.at(i + 1);
        const float p3 = wave.at(i + 2);
        const float sample =
            p1 + 0.5f * f * (p2 - p0 + f * (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3 + f * (3.0f * (p1 - p2) + p3 - p0)));

        const size_t at = (start_frame + frame) * 2;
        if (out.size() < at + 2) {
            out.resize(at + 2, 0.0f);
        }
        const float value = sample * float(level) / 32768.0f;
        out[at] += value * v.gain[gain_index].left;
        out[at + 1] += value * v.gain[gain_index].right;

        position += std::min(ratio, kMaxPitchRatio) * kBkSynthOutputRate / rate;
        if (wave.forever && position >= double(wave.loop_end + wave.loop_len)) {
            position -= double(wave.loop_len); // Same sample, smaller number.
        }
    }
    return !cut;
}

// The voice BK's sound player starts for one sound of an effect (func_802432F8, case 1).
VoicePlan plan_sfx_voice(const BkSound& s, const std::vector<int16_t>& pcm, double start, float pitch, float volume) {
    const BkEnvelope& env = s.envelope;
    const bool sweep = (s.key_map.key_max & 0x20) != 0;
    const double ratio =
        bk_cents_to_ratio(float(s.key_map.key_base) * 100.0f + (sweep ? 0.0f : float(s.key_map.detune)) - 6000.0f);

    VoicePlan v;
    v.sound = &s;
    v.pcm = &pcm;
    v.start = start;
    // Each phase's time is divided by the pitch in force when it begins.
    const double p = double(pitch) * ratio;
    v.attack = std::max(0.0, env.attack_time / p);
    v.peak = env.attack_volume;
    v.hold = env.decay_time == -1;
    v.decay = v.hold ? 0.0 : std::max(0.0, env.decay_time / p);
    v.sustain = env.decay_volume;
    v.note_off = v.hold ? -1.0 : v.attack + v.decay; // The decay's end posts the note-off.
    v.release_time = env.release_time;
    v.release_scales_with_pitch = true;

    v.pitch.push_back({0.0, p});
    if (sweep) {
        // func_80243FE4: once the attack is over, the pitch is multiplied by the detune every
        // 1/30 s. Ten seconds of steps is more than any sound lasts.
        const double step = bk_cents_to_ratio(float(s.key_map.detune));
        double r = p;
        for (int n = 1; n <= 300; n++) {
            r *= step;
            v.pitch.push_back({v.attack + kFrameMicros * n, r});
        }
    }
    // Volume: attackVolume * unk34 * sampleVolume / 16129, times the channel volume (full).
    GainStep g;
    gain_unit_from_pan(s.pan, std::clamp(volume, 0.0f, 1.0f) * float(s.volume) / 127.0f / 127.0f, &g.left, &g.right);
    v.gain.push_back(g);
    return v;
}

} // namespace

std::optional<BkRenderedSfx> bk_render_sfx(const BkSoundBank& bank, uint32_t sound, const BkSfxRenderOptions& options,
                                           std::string* error) {
    if (bank.instruments().empty() || bank.instruments()[0].sounds.empty()) {
        set_error(error, "sound bank has no sound effects");
        return std::nullopt;
    }
    const auto& sounds = bank.instruments()[0].sounds;
    if (sound >= sounds.size()) {
        set_error(error, "no sound effect " + hex(sound) + " (the bank has " + std::to_string(sounds.size()) + ")");
        return std::nullopt;
    }
    if (options.output_rate < 4000 || options.output_rate > 192000) {
        set_error(error, "unsupported output rate");
        return std::nullopt;
    }

    // Walk the chain as func_80244608 does: each sound names the next and its own delay.
    struct Link {
        uint32_t index;
        double start;
    };
    std::vector<Link> chain;
    double elapsed = 0.0; // var_s3: the running total of delays, used by repeating sounds.
    uint32_t index = sound + 1;
    while (index != 0 && index - 1 < sounds.size() && chain.size() < kMaxChain) {
        const BkSound& s = sounds[index - 1];
        const double delay = s.key_map.velocity_max * kFrameMicros;
        chain.push_back({index - 1, (s.key_map.key_max & 0x10) ? elapsed : delay});
        elapsed += delay;
        index = uint32_t(s.key_map.velocity_min) + uint32_t(s.key_map.key_min & 0xC0) * 4;
    }

    BkRenderedSfx out;
    out.sample_rate = options.output_rate;
    out.chain_length = chain.size();
    const double stop = std::max(0.0f, options.max_seconds) * 1e6;
    for (size_t i = 0; i < chain.size(); i++) {
        const std::vector<int16_t>* pcm = bank.pcm(0, chain[i].index, error);
        if (!pcm) {
            return std::nullopt;
        }
        // The pitch event reaches only the voice func_80244608 returns, the chain's last; the
        // volume event is passed down the whole chain.
        const float pitch = i + 1 == chain.size() && options.pitch > 0.0f ? options.pitch : 1.0f;
        const VoicePlan v = plan_sfx_voice(sounds[chain[i].index], *pcm, chain[i].start, pitch, options.volume);
        if (!render_voice(v, options.output_rate, stop, out.stereo)) {
            out.sustained = true;
        }
    }
    return out;
}

namespace {

constexpr uint8_t kSeqBlockCode = 0xFE;     // AL_CMIDI_BLOCK_CODE: a back-reference follows.
constexpr uint8_t kSeqMeta = 0xFF;
constexpr uint8_t kSeqMetaTempo = 0x51;
constexpr uint8_t kSeqMetaEndOfTrack = 0x2F;
constexpr uint8_t kSeqMetaLoopStart = 0x2E;
constexpr uint8_t kSeqMetaLoopEnd = 0x2D;
constexpr size_t kSeqHeaderSize = 0x44;   // 16 track offsets and the division.
constexpr size_t kMaxSeqEvents = 20000;    // Per track; a loop forever stops here.

// Reads one track the way n_csq.c's __getTrackByte does, back-references included.
class TrackReader {
public:
    TrackReader(std::vector<uint8_t>& data, size_t start) : data_(data), loc_(start) {}

    bool byte(uint8_t* out) {
        if (backup_len_ > 0) {
            if (backup_ >= data_.size()) {
                return false;
            }
            *out = data_[backup_++];
            backup_len_--;
            return true;
        }
        if (loc_ >= data_.size()) {
            return false;
        }
        uint8_t b = data_[loc_++];
        if (b == kSeqBlockCode) {
            if (loc_ >= data_.size()) {
                return false;
            }
            const uint8_t next = data_[loc_++];
            if (next != kSeqBlockCode) {
                if (loc_ + 2 > data_.size()) {
                    return false;
                }
                const size_t back = (size_t(next) << 8) | data_[loc_];
                const uint8_t len = data_[loc_ + 1];
                loc_ += 2;
                if (back + 4 > loc_ || len == 0) {
                    return false;
                }
                backup_ = loc_ - (back + 4);
                backup_len_ = len;
                return byte(out);
            }
        }
        *out = b;
        return true;
    }

    bool var_len(uint32_t* out) {
        uint8_t c;
        if (!byte(&c)) {
            return false;
        }
        uint32_t value = c;
        if (value & 0x80) {
            value &= 0x7F;
            int guard = 0;
            do {
                if (!byte(&c) || ++guard > 4) {
                    return false;
                }
                value = (value << 7) + (c & 0x7F);
            } while (c & 0x80);
        }
        *out = value;
        return true;
    }

    // Loop end: the count bytes live in the sequence and the player rewrites them.
    bool loop_end() {
        if (loc_ + 6 > data_.size()) {
            return false;
        }
        const uint8_t loop_count = data_[loc_];
        const uint8_t current = data_[loc_ + 1];
        if (current == 0) {
            data_[loc_ + 1] = loop_count;
            loc_ += 6;
            return true;
        }
        if (current != 0xFF) {
            data_[loc_ + 1] = uint8_t(current - 1);
        }
        const size_t offset = (size_t(data_[loc_ + 2]) << 24) | (size_t(data_[loc_ + 3]) << 16) |
                              (size_t(data_[loc_ + 4]) << 8) | data_[loc_ + 5];
        const size_t after = loc_ + 6;
        if (offset > after) {
            return false;
        }
        loc_ = after - offset;
        return true;
    }

private:
    std::vector<uint8_t>& data_;
    size_t loc_;
    size_t backup_ = 0;
    size_t backup_len_ = 0;
};

struct SeqEvent {
    uint64_t tick = 0;
    uint32_t track = 0;
    uint32_t order = 0; // Position within the track, to keep simultaneous events in order.
    uint8_t status = 0; // MIDI status, or kSeqMeta for a tempo change.
    uint8_t byte1 = 0;
    uint8_t byte2 = 0;
    uint32_t value = 0; // Note duration in ticks, or the tempo in microseconds per quarter note.
};

} // namespace

std::optional<BkRenderedSfx> bk_render_sequence(const BkSoundBank& bank, std::span<const uint8_t> sequence,
                                                const BkSequenceRenderOptions& options, std::string* error) {
    if (sequence.size() < kSeqHeaderSize) {
        set_error(error, "sequence is too short for a compact MIDI header");
        return std::nullopt;
    }
    if (options.output_rate < 4000 || options.output_rate > 192000) {
        set_error(error, "unsupported output rate");
        return std::nullopt;
    }
    const uint32_t division = read_be32(sequence, 0x40);
    if (division == 0 || division > 0x7FFF) {
        set_error(error, "sequence has an implausible division");
        return std::nullopt;
    }
    const double max_micros = std::max(0.0f, options.max_seconds) * 1e6;

    // Read every track to its end (or far enough past the time limit, for loops).
    std::vector<SeqEvent> events;
    for (uint32_t track = 0; track < 16; track++) {
        const uint32_t offset = read_be32(sequence, track * 4);
        if (offset == 0) {
            continue;
        }
        if (offset < kSeqHeaderSize || offset >= sequence.size()) {
            set_error(error, "sequence track " + std::to_string(track) + " starts outside the data");
            return std::nullopt;
        }
        std::vector<uint8_t> data(sequence.begin(), sequence.end()); // Loops rewrite their counters.
        TrackReader reader(data, offset);
        uint64_t tick = 0;
        uint8_t running = 0;
        uint32_t delta = 0;
        if (!reader.var_len(&delta)) {
            set_error(error, "sequence track " + std::to_string(track) + " is truncated");
            return std::nullopt;
        }
        for (uint32_t order = 0;; order++) {
            tick += delta;
            if (order >= kMaxSeqEvents) {
                break; // A loop forever: far more than the time limit can play.
            }
            uint8_t status;
            if (!reader.byte(&status)) {
                set_error(error, "sequence track " + std::to_string(track) + " is truncated");
                return std::nullopt;
            }
            SeqEvent e;
            e.tick = tick;
            e.track = track;
            e.order = order;
            bool ok = true;
            if (status == kSeqMeta) {
                uint8_t type;
                ok = reader.byte(&type);
                if (ok && type == kSeqMetaTempo) {
                    uint8_t a = 0, b = 0, c = 0;
                    ok = reader.byte(&a) && reader.byte(&b) && reader.byte(&c);
                    e.status = kSeqMeta;
                    e.value = (uint32_t(a) << 16) | (uint32_t(b) << 8) | c;
                    if (ok && e.value > 0) {
                        events.push_back(e);
                    }
                } else if (ok && type == kSeqMetaEndOfTrack) {
                    break;
                } else if (ok && type == kSeqMetaLoopStart) {
                    uint8_t skip;
                    ok = reader.byte(&skip) && reader.byte(&skip);
                } else if (ok && type == kSeqMetaLoopEnd) {
                    ok = reader.loop_end();
                } else if (ok) {
                    set_error(error, "sequence track " + std::to_string(track) + " has unknown meta event " + hex(type));
                    return std::nullopt;
                }
                running = 0; // Running status isn't kept across meta events.
            } else {
                if (status & 0x80) {
                    e.status = status;
                    ok = reader.byte(&e.byte1);
                    running = status;
                } else {
                    if (running == 0) {
                        set_error(error, "sequence track " + std::to_string(track) + " uses running status with none set");
                        return std::nullopt;
                    }
                    e.status = running;
                    e.byte1 = status;
                }
                const uint8_t kind = e.status & 0xF0;
                if (ok && kind != 0xC0 && kind != 0xD0) {
                    ok = reader.byte(&e.byte2);
                    if (ok && kind == 0x90) {
                        ok = reader.var_len(&e.value);
                    }
                }
                if (ok) {
                    events.push_back(e);
                }
            }
            if (!ok || !reader.var_len(&delta)) {
                set_error(error, "sequence track " + std::to_string(track) + " is truncated");
                return std::nullopt;
            }
        }
    }
    // n_alCSeqNextEvent takes the earliest event, the lowest track first on a tie.
    std::stable_sort(events.begin(), events.end(), [](const SeqEvent& a, const SeqEvent& b) {
        return a.tick != b.tick ? a.tick < b.tick : a.track < b.track;
    });

    // Tempo map, so a tick (a note's start or end) converts to microseconds. Note-offs are
    // rescaled on a tempo change (__CSPHandleMetaMsg), so ticks are the true unit for them.
    struct TempoPoint {
        uint64_t tick;
        double micros;
        double uspt;
    };
    std::vector<TempoPoint> tempo{{0, 0.0, 500000.0 / division}}; // 120 bpm until told otherwise.
    for (const auto& e : events) {
        if (e.status == kSeqMeta) {
            const TempoPoint& last = tempo.back();
            const double at = last.micros + double(e.tick - last.tick) * last.uspt;
            if (e.tick == last.tick) {
                tempo.back().uspt = double(e.value) / division;
            } else {
                tempo.push_back({e.tick, at, double(e.value) / division});
            }
        }
    }
    auto micros_at = [&](uint64_t tick) {
        auto it = std::upper_bound(tempo.begin(), tempo.end(), tick,
                                   [](uint64_t t, const TempoPoint& p) { return t < p.tick; });
        const TempoPoint& p = *(it - 1);
        return p.micros + double(tick - p.tick) * p.uspt;
    };

    // Play the events through n_csplayer.c's channel logic, collecting voices.
    struct Channel {
        const BkInstrument* instrument = nullptr;
        int pan = 64;
        int vol = 127;
        int expression = 127; // BK's controller 0x7D (unkA).
        int sustain = 0;
        int bend_range = 200;
        double bend = 1.0;
        bool enabled = true;
    };
    std::array<Channel, 16> channels;
    if (!bank.instruments().empty()) {
        // A channel with no program change plays instrument 0.
        for (auto& c : channels) {
            c.instrument = &bank.instruments()[0];
        }
    }
    struct ActiveVoice {
        VoicePlan plan;
        uint8_t channel;
        uint8_t key;
        uint8_t velocity;
        uint64_t off_tick;
        bool sustained_off = false; // Note-off held by the sustain pedal.
        bool released = false;
    };
    std::vector<ActiveVoice> voices;
    const int seq_volume = int(std::clamp(options.volume, 0.0f, 1.0f) * 32767.0f);

    // __n_vsVol with envGain 127, turned into amplitude per envelope level unit.
    auto voice_gain = [&](const ActiveVoice& av, double t) {
        const Channel& c = channels[av.channel];
        const BkSound& s = *av.plan.sound;
        const uint32_t t1 = (127u * av.velocity * 127u * uint32_t(c.expression)) >> 13;
        const uint32_t t2 = (uint32_t(s.volume) * uint32_t(seq_volume) * uint32_t(c.vol)) >> 14;
        const double vol = double((uint64_t(t1) * t2) >> 15);
        GainStep g;
        g.t = t;
        gain_unit_from_pan(std::clamp(c.pan - 64 + int(s.pan), 0, 127), float(vol / 127.0 / 32767.0), &g.left, &g.right);
        return g;
    };
    auto release = [&](ActiveVoice& av, double now) {
        if (!av.released) {
            av.released = true;
            av.plan.note_off = now - av.plan.start;
        }
    };
    auto process_note_offs = [&](uint64_t up_to_tick, bool inclusive) {
        for (auto& av : voices) {
            if (av.released || av.sustained_off || av.off_tick == UINT64_MAX) {
                continue;
            }
            if (av.off_tick < up_to_tick || (inclusive && av.off_tick == up_to_tick)) {
                if (channels[av.channel].sustain > 64) {
                    av.sustained_off = true;
                } else {
                    release(av, micros_at(av.off_tick));
                }
            }
        }
    };

    for (const auto& e : events) {
        const double now = micros_at(e.tick);
        if (now > max_micros) {
            break;
        }
        // Note-offs posted earlier fire before events at a later tick.
        process_note_offs(e.tick, false);
        if (e.status == kSeqMeta) {
            continue;
        }
        const uint8_t kind = e.status & 0xF0;
        Channel& c = channels[e.status & 0x0F];
        switch (kind) {
        case 0x90: {
            if (e.byte2 == 0 || !c.enabled || !c.instrument || c.instrument->sounds.empty()) {
                break;
            }
            // __n_lookupSoundQuick: a binary search over the key and velocity ranges.
            const auto& sounds = c.instrument->sounds;
            int l = 1;
            int r = int(sounds.size());
            const BkSound* found = nullptr;
            size_t found_index = 0;
            while (r >= l) {
                const int i = (l + r) / 2;
                const BkKeyMap& km = sounds[size_t(i - 1)].key_map;
                if (e.byte1 >= km.key_min && e.byte1 <= km.key_max && e.byte2 >= km.velocity_min &&
                    e.byte2 <= km.velocity_max) {
                    found = &sounds[size_t(i - 1)];
                    found_index = size_t(i - 1);
                    break;
                }
                if (e.byte1 < km.key_min || (e.byte2 < km.velocity_min && e.byte1 <= km.key_max)) {
                    r = i - 1;
                } else {
                    l = i + 1;
                }
            }
            if (!found) {
                break;
            }
            const size_t inst_index = size_t(c.instrument - bank.instruments().data());
            const std::vector<int16_t>* pcm = bank.pcm(inst_index, found_index, error);
            if (!pcm) {
                return std::nullopt;
            }
            ActiveVoice av;
            av.channel = e.status & 0x0F;
            av.key = e.byte1;
            av.velocity = e.byte2;
            av.off_tick = e.value ? e.tick + e.value : UINT64_MAX;
            VoicePlan& v = av.plan;
            v.sound = found;
            v.pcm = pcm;
            v.start = now;
            const BkEnvelope& env = found->envelope;
            v.attack = std::max(0, env.attack_time);
            v.peak = env.attack_volume;
            v.hold = env.decay_time == -1;
            v.decay = std::max(0, env.decay_time);
            v.sustain = env.decay_volume;
            v.release_time = env.release_time;
            const double cents = (int(e.byte1) - int(found->key_map.key_base)) * 100 + found->key_map.detune;
            v.pitch.push_back({0.0, bk_cents_to_ratio(float(cents)) * c.bend});
            v.gain.push_back(voice_gain(av, 0.0));
            voices.push_back(std::move(av));
            break;
        }
        case 0xB0:
            switch (e.byte1) {
            case 0x07: // Volume
            case 0x0A: // Pan
            case 0x7D: // BK's second volume
                if (e.byte1 == 0x07) {
                    c.vol = e.byte2;
                } else if (e.byte1 == 0x0A) {
                    c.pan = e.byte2;
                } else {
                    c.expression = e.byte2;
                }
                for (auto& av : voices) {
                    if (&channels[av.channel] == &c && !av.released) {
                        av.plan.gain.push_back(voice_gain(av, now - av.plan.start));
                    }
                }
                break;
            case 0x40: // Sustain pedal
                c.sustain = e.byte2;
                if (c.sustain <= 64) {
                    for (auto& av : voices) {
                        if (&channels[av.channel] == &c && av.sustained_off) {
                            av.sustained_off = false;
                            release(av, now);
                        }
                    }
                }
                break;
            case 0x7E: // BK: mute a channel
                if (e.byte2 < 16) {
                    channels[e.byte2].enabled = false;
                    for (auto& av : voices) {
                        if (av.channel == e.byte2) {
                            release(av, now);
                        }
                    }
                }
                break;
            case 0x7F: // BK: unmute a channel
                if (e.byte2 < 16) {
                    channels[e.byte2].enabled = true;
                }
                break;
            default:
                break;
            }
            break;
        case 0xC0:
            if (e.byte1 < bank.instruments().size()) {
                const BkInstrument& inst = bank.instruments()[e.byte1];
                c.instrument = &inst;
                c.pan = inst.pan;
                c.vol = inst.volume;
                c.bend_range = inst.bend_range;
            }
            break;
        case 0xE0: {
            const int bend = ((int(e.byte2) << 7) + e.byte1) - 8192;
            c.bend = bk_cents_to_ratio(float((c.bend_range * bend) / 8192));
            for (auto& av : voices) {
                if (&channels[av.channel] == &c && av.plan.sound) {
                    const double cents = (int(av.key) - int(av.plan.sound->key_map.key_base)) * 100 +
                                         av.plan.sound->key_map.detune;
                    av.plan.pitch.push_back({now - av.plan.start, bk_cents_to_ratio(float(cents)) * c.bend});
                }
            }
            break;
        }
        default:
            break;
        }
    }
    process_note_offs(UINT64_MAX, true);

    BkRenderedSfx out;
    out.sample_rate = options.output_rate;
    out.chain_length = voices.size();
    for (const auto& av : voices) {
        if (av.plan.start >= max_micros) {
            continue;
        }
        if (!render_voice(av.plan, options.output_rate, max_micros, out.stereo)) {
            out.sustained = true;
        }
    }
    return out;
}

std::vector<uint8_t> bk_stereo_to_wav(std::span<const float> stereo, uint32_t sample_rate) {
    const uint32_t data_size = uint32_t(stereo.size() / 2 * 4);
    std::vector<uint8_t> out;
    out.reserve(44 + data_size);
    auto put = [&](const char* tag) { out.insert(out.end(), tag, tag + 4); };
    auto le32 = [&](uint32_t v) {
        for (int i = 0; i < 4; i++) {
            out.push_back(uint8_t(v >> (8 * i)));
        }
    };
    auto le16 = [&](uint16_t v) {
        out.push_back(uint8_t(v));
        out.push_back(uint8_t(v >> 8));
    };
    put("RIFF");
    le32(36 + data_size);
    put("WAVE");
    put("fmt ");
    le32(16);
    le16(1); // PCM
    le16(2);
    le32(sample_rate);
    le32(sample_rate * 4);
    le16(4);
    le16(16);
    put("data");
    le32(data_size);
    for (size_t i = 0; i < stereo.size() / 2 * 2; i++) {
        le16(uint16_t(clamp16(int32_t(std::lround(stereo[i] * 32767.0f)))));
    }
    return out;
}

} // namespace jiggy
