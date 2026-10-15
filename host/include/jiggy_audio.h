#ifndef __JIGGY_AUDIO_H__
#define __JIGGY_AUDIO_H__

// Banjo-Kazooie sound effects and jingles mixed into DK64's audio output.
//
// The game patches start sounds through the jiggy_bk_sfx_play / jiggy_bk_jingle_play imports
// (patches/jiggy.h). Each sound is rendered once from the player's Banjo-Kazooie ROM by the
// Jiggy Bridge (bridge/include/jiggy/bk_sound.hpp) and played by a small fixed set of voices
// that queue_samples (src/main/main.cpp) mixes into every chunk of game audio.

#include <cstddef>
#include <cstdint>

namespace jiggy_host::audio {
    // Adds the playing voices into `stereo` (interleaved L R floats, `frames` frames at
    // `sample_rate`, the game's own output rate before resampling), scaled by `gain`.
    // Called from the audio thread; safe alongside the play/stop imports on the game thread.
    void mix(float* stereo, size_t frames, uint32_t sample_rate, float gain);
}

#endif
