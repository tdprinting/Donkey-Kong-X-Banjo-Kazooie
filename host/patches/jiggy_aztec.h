#ifndef __JIGGY_AZTEC_H__
#define __JIGGY_AZTEC_H__

// Angry Aztec fused with Gobi's Valley: what the world's Banjo-Kazooie characters share
// (jiggy_aztec.c): the text box, the cutscene lock, drawing a model, the floor and the player's
// attacks. The characters are jiggy_gobi.c, jiggy_trunker.c, jiggy_grabba.c,
// jiggy_ancient_ones.c and the Slappas (jiggy_slappas.c).
//
// Gobi's Valley's characters are drawn with their models at rest: their animations move the
// vertices themselves (the model's animated vertex list, animVerticesList_transform), which
// the host's posing doesn't do, so each file moves its model as a whole instead (rising,
// leaning, bobbing, growing).

#include "common_structs.h"
#include "jiggy.h"

// Reward ids (docs/GAME_PLAN.md: Angry Aztec has 0x20-0x2F).
#define AZTEC_REWARD_GOBI 0x20         // Gobi freed from his rock.
#define AZTEC_REWARD_TRUNKER 0x21      // Trunker watered by Gobi.
#define AZTEC_REWARD_GRABBA 0x22       // Grabba beaten.
#define AZTEC_REWARD_ANCIENT_ONES 0x23 // All five Ancient Ones raised.
#define AZTEC_REWARD_JINJOS 0x24       // The five Jinjos (jiggy_jinjo_jiggy.c).

// Save keys (jiggy_save_get/set): Angry Aztec's prefix 0x51, the character in bits 16-23.
#define AZTEC_SAVE_KEY(character) (0x51000000u | ((u32)(character) << 16) | (u32)MAP_AZTEC)
#define AZTEC_CHARACTER_GOBI 0
#define AZTEC_CHARACTER_TRUNKER 1
#define AZTEC_CHARACTER_GRABBA 2
#define AZTEC_CHARACTER_ANCIENT_ONES 3

// Gobi's save bits (read by jiggy_trunker.c too).
#define GOBI_SAVE_TALKED 0x1  // His first talk.
#define GOBI_SAVE_FREED 0x2   // The rock broken: he waits by Trunker.
#define GOBI_SAVE_WATERED 0x4 // He's spat his water on Trunker.

// Where Trunker stands in the first sand area (Gobi goes beside him once he's free).
#define TRUNKER_X 2600.0f
#define TRUNKER_Y 120.0f
#define TRUNKER_Z 1180.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define AZTEC_BK_TO_DK64 0.25f
#define AZTEC_GAME_FPS 30.0f
// The player is on the ground when no higher than this over the floor.
#define AZTEC_AIRBORNE_HEIGHT 6.0f

// Banjo-Kazooie sounds and jingles the characters share.
#define AZTEC_JINGLE_JIGGY_SPAWN 0x3D    // COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767
#define AZTEC_JINGLE_PUZZLE_SOLVED 0x2D  // COMUSIC_2D_PUZZLE_SOLVED_FANFARE, 28000/32767
#define AZTEC_JINGLE_DING 0x2B           // COMUSIC_2B_DING_B, 28000/32767
#define AZTEC_SFX_SAND_RUMBLE 0x3EC      // SFX_3EC_CCW_DOOR_OPENING: things rising from the sand
#define AZTEC_SFX_SAND_POP 0x3F5         // SFX_3F5_UNKNOWN: a hand ducking into the sand

#ifndef __cplusplus
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.

// The text box (one at a time, whoever's talking). `portrait` is a Banjo-Kazooie sprite
// (its first `mouth_frames` frames move the mouth), `voice` the sounds played while the text
// types out, at `voice_volume` and about `voice_pitch`. The cutscene lock holds the player
// until the last page is closed with A. `owner` tells the characters apart.
void aztec_talk_start(s32 owner, u32 portrait, s32 mouth_frames, const char *name, const char *const *pages, s32 count,
                      const u16 *voice, s32 voice_count, f32 voice_volume, f32 voice_pitch);
// A frame of the open text box; returns TRUE on the frame its owner's last page is closed.
s32 aztec_talk_update(s32 owner);
// The owner of the open text box, or -1.
s32 aztec_talk_owner(void);
// Closes the text box at once and lets the player go (leaving the map).
void aztec_talk_cancel(void);
// The game's cutscene lock (as jiggy_bottles.c holds the player while he talks).
void aztec_lock_player(s32 locked);
s32 aztec_player_locked(void);

f32 aztec_distance_xz(f32 ax, f32 az, f32 bx, f32 bz);
// The floor under (x, z), looked for from `probe_above` over `y`; `y` if there's none within
// `max_step` of it.
f32 aztec_floor(f32 x, f32 y, f32 z, f32 probe_above, f32 max_step);
// Turns `*yaw` (12-bit angle units) a share of the way toward facing (tx, tz) from (x, z).
void aztec_turn_toward(f32 *yaw, f32 tx, f32 tz, f32 x, f32 z, f32 rate, u32 frames);
// Whether one of the player's attacks reaches a thing `radius` wide standing at pos this
// frame: punches (in front of the player), rolls, aerial attacks, the Simian Slam (a little
// further), Banjo's Rat-a-tat Rap and Beak Buster.
s32 aztec_player_attacks(Actor *player, const f32 pos[3], f32 radius);
// Whether the player's Simian Slam (or Banjo's Beak Buster) lands on a thing at pos.
s32 aztec_player_slams(Actor *player, const f32 pos[3], f32 radius);
// Whether one of the player's shots (a gun's, or Banjo's eggs) is in a thing at pos.
s32 aztec_player_shot(Actor *player, const f32 pos[3], f32 radius, f32 height);
// Hurts the player through DK64's own enemy hit, knocked away from (x, z).
void aztec_hurt_player(Actor *player, f32 x, f32 z);
// A Banjo-Kazooie sound from (x, z), fading out with the player's distance as Banjo-Kazooie's
// positional sounds do (1250 to 2500 units, scaled).
void aztec_sfx_at(Actor *player, f32 x, f32 z, u32 sfx, f32 volume, f32 pitch);
// Game frames since *last (0 to 4; more counts as one, after a pause), updating it.
u32 aztec_frames(u32 *last);
// Builds a model into block; FALSE if it's missing or doesn't fit.
s32 aztec_model_load(u32 asset, void *block, u32 size);
// Draws a built model at pos turned to yaw (12-bit angle units), scaled by AZTEC_BK_TO_DK64
// times `scale` (and `stretch_y` up and down), pitched forward by `pitch` degrees about its
// feet, with `offset` (model units, or NULL) taken off first so a model built off its origin
// stands at pos.
Gfx *aztec_model_draw(Gfx *dl, JiggyBkPose *pose, s32 model, void *block, const f32 pos[3], f32 yaw, f32 scale,
                      f32 stretch_y, f32 pitch, const f32 *offset, Gfx *gfx, u32 gfx_capacity, Mtx *mtx,
                      u32 mtx_capacity);
// Selectors as Banjo-Kazooie's modelRender resets them (branch 1 of selector 1).
void aztec_pose_init(JiggyBkPose *pose);
#endif

#endif
