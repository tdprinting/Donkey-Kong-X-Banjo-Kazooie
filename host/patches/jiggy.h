#ifndef __JIGGY_H__
#define __JIGGY_H__

// Banjo-Kazooie assets from the player's ROM, served by the host (src/game/jiggy_api.cpp).
// Asset ids are the Banjo-Kazooie decompilation's asset_e values.

#include "patch_helpers.h"

// 1 if the player has given us a Banjo-Kazooie ROM, 0 otherwise.
DECLARE_FUNC(s32, jiggy_bk_available);
// Decompressed size of an asset, or -1 if it doesn't exist.
DECLARE_FUNC(s32, jiggy_bk_asset_size, u32 asset_id);
// Copies an asset into dst. Returns the bytes copied, or -1 if it is missing or doesn't fit.
DECLARE_FUNC(s32, jiggy_bk_asset_read, u32 asset_id, void* dst, u32 dst_len);
// Frames in a sprite asset (e.g. a dialogue portrait), or -1 if the asset isn't a sprite.
DECLARE_FUNC(s32, jiggy_bk_sprite_frame_count, u32 asset_id);
// Decodes a sprite frame to RGBA16 texels, width padded to a multiple of 4. Returns
// (width << 16) | height, or -1. Pass dst NULL to get the size without writing.
DECLARE_FUNC(s32, jiggy_bk_sprite_frame_rgba16, u32 asset_id, u32 frame, void* dst, u32 dst_len);
// Banjo-Kazooie's HUD number `digit` (0-9, the bold font in its orange gradient) as RGBA16
// texels, like jiggy_bk_sprite_frame_rgba16.
DECLARE_FUNC(s32, jiggy_bk_bold_number_rgba16, u32 digit, void* dst, u32 dst_len);

// Values Jiggy Jungle keeps with each DK64 save file (collected notes and the like), stored
// beside DK64's own save data. Unset values read as 0.
DECLARE_FUNC(u32, jiggy_save_get, u32 file, u32 key);
DECLARE_FUNC(void, jiggy_save_set, u32 file, u32 key, u32 value);
// Forgets a save file's values; called when the game starts that file afresh.
DECLARE_FUNC(void, jiggy_save_clear, u32 file);
// Converts a model to a block DK64 can draw, built to live at dst (8-byte aligned): its
// F3DEX2 display lists, vertices, textures and render modes. Returns the size, writing only
// if it fits (pass dst NULL to ask), or -1 if it isn't a model. Draw it with
// jiggy_bk_model_pose.
DECLARE_FUNC(s32, jiggy_bk_model_build, u32 asset_id, void* dst, u32 dst_len);

// Banjo-Kazooie's appendage selectors (modelRender_setAppendageVisibility).
#define JIGGY_BK_SELECTOR_COUNT 0x2A

#ifndef __cplusplus
// A request to draw a built model in a pose. Matches the offsets in jiggy_api.cpp.
typedef struct {
    /* 0x00 */ s32 model;       // Asset id given to jiggy_bk_model_build.
    /* 0x04 */ void *block;     // Where it was built.
    /* 0x08 */ s32 animation;   // Animation asset id, or -1 for the rest pose.
    /* 0x0C */ f32 progress;    // 0 to 1 through the animation.
    /* 0x10 */ f32 world[4][4]; // Where the model stands (MtxF, as guMtxCatF builds).
    /* 0x50 */ s32 selectors[JIGGY_BK_SELECTOR_COUNT]; // 0 hides, n draws branch n.
    /* 0xF8 */ Gfx *gfx;        // Display list to write (ends with gSPEndDisplayList).
    /* 0xFC */ u32 gfx_capacity; // In commands.
    /* 0x100 */ Mtx *mtx;       // Matrices it loads: the root, then one per bone.
    /* 0x104 */ u32 mtx_capacity;
} JiggyBkPose;
#endif

// Fills pose->gfx and pose->mtx. Returns the commands written, or -1 (model not built at
// pose->block, or the buffers are too small). Call gSPDisplayList on pose->gfx after.
DECLARE_FUNC(s32, jiggy_bk_model_pose, void *pose); // A JiggyBkPose.

// Banjo-Kazooie sounds from the player's ROM, mixed into DK64's audio (src/game/jiggy_audio.cpp).
// Plays sound effect `sfx_id` (the decompilation's sfx_e; 0x3E9 and up are music-bank sounds)
// at `volume` (BK's 0-32767 volume argument / 32767.0f, so 1.0f is full) and `pitch` (BK's
// pitch argument, 1.0f as recorded). Returns a handle (> 0), or 0 without a Banjo-Kazooie
// ROM or for an unknown id, so callers can fall back to a DK64 sound.
DECLARE_FUNC(s32, jiggy_bk_sfx_play, u32 sfx_id, f32 volume, f32 pitch);
// Plays a short Banjo-Kazooie music track, such as a pickup jingle (comusic_e, below) at
// `volume` (BK's sequence volume / 32767.0f). At most 8 seconds. Returns like jiggy_bk_sfx_play.
DECLARE_FUNC(s32, jiggy_bk_jingle_play, u32 track, f32 volume);
// Stops a sound started by either of the above. Returns 1 if it was still playing.
DECLARE_FUNC(s32, jiggy_bk_sound_stop, s32 handle);

// Banjo-Kazooie sound effect ids (sfx_e) and how BK plays them: id, volume, pitch.
#define JIGGY_BK_SFX_FOOTSTEP 0x08          // SFX_8_BANJO_LANDING_04: Talon Trot and Wonderwing footfalls on
                                            // ground, 9000/32767, pitch alternating 1.2 and 1.4 (trot) or
                                            // 1.0 and 1.2 (Wonderwing) (+-0.025)
#define JIGGY_BK_SFX_EGGSHELL_BREAK 0x0D    // SFX_D_EGGSHELL_BREAKING: egg hits, 13000/32767, 1.6-1.7
#define JIGGY_BK_SFX_SHOCKSPRING_BOING 0x0E // SFX_E_SHOCKSPRING_BOING: Shock Spring Jump launch, 22000/32767, 1.0
#define JIGGY_BK_SFX_PULLING_NOISE 0x2C     // SFX_2C_PULLING_NOISE: Shock Spring Jump crouch, 22000/32767,
                                            // pitch 1.4 sliding down
#define JIGGY_BK_SFX_BANJO_AHOO 0x33        // SFX_33_BANJO_AHOO: jump off a Shock Spring Jump pad, 0x7FF8/32767, 1.0
#define JIGGY_BK_SFX_EGG_POOP 0x3E          // SFX_3E_POOP_NOISE: egg out the back, 28000/32767, 1.4
#define JIGGY_BK_SFX_KAZOOIE_AUW 0x44       // SFX_44_KAZOOIE_AUW: Wonderwing and Shock Spring Jump start,
                                            // 22000/32767, 1.117
#define JIGGY_BK_SFX_KAZOOIE_CHOKE 0x46     // SFX_46_KAZOOIE_CHOKING_UP: egg from the mouth (wind-up), 1.0, 1.0
#define JIGGY_BK_SFX_KAZOOIE_HEUGH 0x47     // SFX_47_KAZOOIE_HEUGH: Feathery Flap accent, 22000/32767, 1.0
#define JIGGY_BK_SFX_KAZOOIE_RA 0x49        // SFX_49_KAZOOIE_RA: every Talon Trot step, 22000/32767,
                                            // pitch 0.96 (walk anim 0.2781) and 1.04 (0.7781)
#define JIGGY_BK_SFX_FLAP 0x4E              // SFX_4E_KAZOOIE_BRUIII: Feathery Flap, 20000/32767, 1.24
#define JIGGY_BK_SFX_BANJO_HUIII 0x53       // SFX_53_BANJO_HUIII: Shock Spring Jump, 22000/32767, 1.0
#define JIGGY_BK_SFX_KAZOOIE_HEGH 0x57      // SFX_57_KAZOOIE_HEGH: egg from the mouth (the shot), 1.0, 1.0
#define JIGGY_BK_SFX_SWITCH_PRESS 0x90      // SFX_90_SWITCH_PRESS: an empty Honeycomb piece settling in
                                            // its HUD ring, 0x7FF8/32767, 1.3
#define JIGGY_BK_SFX_FEATHER_SPENT 0x3E9    // SFX_3E9_UNKNOWN: a gold feather spent in the Wonderwing,
                                            // 28000/32767, 0.8
// Banjo-Kazooie music tracks (comusic_e) for jiggy_bk_jingle_play, with BK's volume.
#define JIGGY_BK_JINGLE_NOTE 0x09           // COMUSIC_9_NOTE_COLLECTED, 16000/32767
#define JIGGY_BK_JINGLE_EGG 0x0C            // COMUSIC_C_EGG_COLLECTED, 15000/32767 (track default)
#define JIGGY_BK_JINGLE_JINJO 0x0A           // COMUSIC_A_JINJO_COLLECTED
#define JIGGY_BK_JINGLE_JIGGY 0x0D          // COMUSIC_D_JINGLE_JIGGY_COLLECTED, 28000/32767
#define JIGGY_BK_JINGLE_HONEYCOMB 0x16      // COMUSIC_16_HONEYCOMB_COLLECTED, 28000/32767
#define JIGGY_BK_JINGLE_EMPTY_HONEYCOMB 0x17 // COMUSIC_17_EMPTY_HONEYCOMB_COLLECTED, 28000/32767
#define JIGGY_BK_JINGLE_GOLD_FEATHERS 0x25  // COMUSIC_25_USING_GOLD_FEATHERS: the Wonderwing, 28000/32767
#define JIGGY_BK_JINGLE_BUZZER 0x2C         // COMUSIC_2C_BUZZER: out of an item, 22000/32767

#endif
