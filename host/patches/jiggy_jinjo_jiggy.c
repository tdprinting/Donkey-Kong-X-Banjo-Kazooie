// The Jinjos' Jiggy: as in Banjo-Kazooie, collecting all five Jinjos on a map gives a Jiggy.
//
// Banjo-Kazooie plays COMUSIC_30_5TH_JINJO_COLLECTED for the fifth Jinjo in place of the
// usual Jinjo jingle, and spawns the level's Jinjo Jiggy where the Jinjo stood, playing
// COMUSIC_3D_JIGGY_SPAWN (chJinjo_update and jiggy_spawn in src/core2). Here the Jiggy is a
// real DK64 Golden Banana (drawn as a Jiggy by jiggy_collectibles.c): an ACTOR_GOLDEN_BANANA
// spawned the way DK64 drops one for a reward, func_global_asm_806A5DF0 (as the vulture's
// banana and the battle arenas' crowns are), standing still (control state 0) on the floor
// where the fifth Jinjo was. The player collects it like any Golden Banana: DK64 counts it and
// sets the permanent flag it was spawned with, which DK64 saves with the file.
//
// The flags are past the last one the game uses (PERMFLAG_PROGRESS_K_LUMSY_FREE, 0x315) and
// still inside the save's permanent block (bits 0 to 0x31F), and no global flag counts them
// (D_global_asm_80756024's ranges):
//   0x31E  The Training Grounds' Jinjo Jiggy.
//   0x31F  Jungle Japes' Jinjo Jiggy.
// Worlds past those have no DK64 flag of their own: their Jinjo Jiggy is a jiggy_rewards.c reward
// (an id from the world's range in docs/GAME_PLAN.md), which keeps its own flag and save key:
//   reward 0x40  Gloomy Galleon's Jinjo Jiggy.
//   reward 0x70  Creepy Castle's Jinjo Jiggy.
//   reward 0x30  Frantic Factory's Jinjo Jiggy.
//   reward 0x52  Fungi Forest's Jinjo Jiggy.
//   reward 0x00  DK Isles' Jinjo Jiggy.
//
// The reward is kept with the save file (jiggy_save_get/set) so it's given once: earned, and
// where it stands. While it's earned and its flag isn't set, it's spawned again on every visit,
// so a Jiggy left behind is there next time. jiggy_jinjos.c calls in here when the fifth Jinjo
// is collected, and each frame while a map's Jinjos are out.

#include "common_structs.h"
#include "jiggy.h"

// Banjo-Kazooie's music (comusic_e), at its volumes.
#define JINGLE_FIFTH_JINJO 0x30 // COMUSIC_30_5TH_JINJO_COLLECTED, 28000/32767
#define JINGLE_JIGGY_SPAWN 0x3D // COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767
// DK64's reward Golden Banana (func_global_asm_806A5DF0's actor ids are ACTOR_* values).
#define REWARD_GOLDEN_BANANA 0x2D // ACTOR_GOLDEN_BANANA
// func_global_asm_80689250 keeps the spawner's type 0x10 below the actor id.
#define REWARD_SPAWNER_TYPE (REWARD_GOLDEN_BANANA - 0x10)
#define FLAG_TYPE_PERMANENT 0
// The Jiggy appears this long after the fifth Jinjo (game frames), once the player has
// stepped this far (across) from the spot, so it's seen before it's collected; and it floats
// this high over the floor (Banjo-Kazooie's 50 units over the Jinjo, scaled as BK_TO_DK64).
#define JIGGY_APPEAR_FRAMES 30
#define JIGGY_APPEAR_CLEARANCE 30.0f
#define JIGGY_HEIGHT 12.5f
// A collected reward banana turns into a Golden Banana prop that DK64 collects a frame or so
// later, setting the flag then (func_global_asm_806F73A0): so once its spawner has gone, the
// flag is given this long (game frames) before the banana is spawned again.
#define JIGGY_RESPAWN_FRAMES 90

// Save keys: the reward's state (JINJO_JIGGY_* bits, the spot's height in the top half), and
// the spot's x (top half) and z (bottom half), as whole units.
#define JINJO_JIGGY_SAVE_KEY(map) (0x4AFFFF00 | (u32)(map))
#define JINJO_JIGGY_SPOT_KEY(map) (0x4AFFFE00 | (u32)(map))
#define JINJO_JIGGY_EARNED 0x1
#define JINJO_JIGGY_COLLECTED 0x2

extern Maps current_map;
extern u8 current_file;
extern u32 object_timer;
extern ActorSpawner *actor_spawner_pointer;
u8 isFlagSet(s16 flagIndex, u8 flagType);
void func_global_asm_806A5DF0(s16 actor, f32 x, f32 y, f32 z, s16 angle, u8 control_state, s16 flag, s32 arg7);

// Each map's DK64 flag, or -1 and its jiggy_rewards.c reward id.
#define NO_FLAG -1
#define NO_REWARD -1
static const struct {
    Maps map;
    s16 flag;
    s16 reward;
} jinjo_jiggy_flags[] = {
    {MAP_TRAINING_GROUNDS, 0x31E, NO_REWARD},
    {MAP_JAPES, 0x31F, NO_REWARD},
    {MAP_GALLEON, NO_FLAG, 0x40},
    {MAP_CASTLE, NO_FLAG, 0x70},
    {MAP_FACTORY, NO_FLAG, 0x30},
    {MAP_FUNGI, NO_FLAG, 0x52},
    {MAP_DK_ISLES_OVERWORLD, NO_FLAG, 0x00},
};

static s32 jiggy_map = -1;
static u32 jiggy_state;
static f32 jiggy_spot[3];
static s16 jiggy_flag;
static s16 jiggy_reward; // A jiggy_rewards.c id when the map has no flag, or NO_REWARD.
// The Golden Banana's spawner this visit (NULL until spawned), and frames until it appears.
static ActorSpawner *jiggy_spawner;
static s32 jiggy_wait;
static s32 jiggy_fanfare;
static u32 jiggy_last_timer;

static void jiggy_map_load(void) {
    u32 spot;
    s32 i;

    jiggy_map = current_map;
    jiggy_state = jiggy_save_get(current_file, JINJO_JIGGY_SAVE_KEY(current_map));
    spot = jiggy_save_get(current_file, JINJO_JIGGY_SPOT_KEY(current_map));
    jiggy_spot[0] = (s16)(spot >> 16);
    jiggy_spot[1] = (s16)(jiggy_state >> 16);
    jiggy_spot[2] = (s16)(spot & 0xFFFF);
    jiggy_flag = NO_FLAG;
    jiggy_reward = NO_REWARD;
    for (i = 0; i < (s32)(sizeof(jinjo_jiggy_flags) / sizeof(jinjo_jiggy_flags[0])); i++) {
        if (jinjo_jiggy_flags[i].map == current_map) {
            jiggy_flag = jinjo_jiggy_flags[i].flag;
            jiggy_reward = jinjo_jiggy_flags[i].reward;
        }
    }
    jiggy_spawner = NULL;
    jiggy_wait = 0;
    jiggy_fanfare = FALSE;
    jiggy_last_timer = object_timer;
}

// The fifth Jinjo of the map has been collected, standing at `pos` (on the floor): the Jiggy
// is earned, and appears there in a moment. With `fanfare`, plays Banjo-Kazooie's fifth-Jinjo
// fanfare and returns TRUE if it's playing; FALSE means the caller plays the usual Jinjo sound.
// Without, the Jiggy just appears (a save whose Jinjos were all found before they gave one).
s32 jiggy_jinjo_jiggy_earn(f32 pos[3], s32 fanfare) {
    if ((s32)current_map != jiggy_map) {
        jiggy_map_load();
    }
    if ((jiggy_flag < 0 && jiggy_reward < 0) || (jiggy_state & JINJO_JIGGY_EARNED)) {
        return FALSE;
    }
    jiggy_spot[0] = (s16)pos[0];
    jiggy_spot[1] = (s16)pos[1];
    jiggy_spot[2] = (s16)pos[2];
    jiggy_state = JINJO_JIGGY_EARNED | ((u32)(u16)(s16)jiggy_spot[1] << 16);
    jiggy_save_set(current_file, JINJO_JIGGY_SPOT_KEY(current_map),
                   ((u32)(u16)(s16)jiggy_spot[0] << 16) | (u16)(s16)jiggy_spot[2]);
    jiggy_save_set(current_file, JINJO_JIGGY_SAVE_KEY(current_map), jiggy_state);
    if (!fanfare) {
        return FALSE;
    }
    jiggy_wait = JIGGY_APPEAR_FRAMES;
    jiggy_fanfare = TRUE;
    return jiggy_bk_jingle_play(JINGLE_FIFTH_JINJO, 28000.0f / 32767.0f) > 0;
}

// Whether the Golden Banana spawned this visit is still in the game's spawner list (it goes
// when the map is loaded again, such as after a death).
static s32 jiggy_spawner_alive(void) {
    ActorSpawner *spawner;

    for (spawner = actor_spawner_pointer; spawner != NULL; spawner = spawner->next_spawner) {
        if (spawner == jiggy_spawner) {
            return spawner->actor_type == REWARD_SPAWNER_TYPE;
        }
    }
    return FALSE;
}

// Each frame while the map's Jinjos are out (jiggy_jinjos.c): spawns the earned Jiggy if it
// hasn't been collected and isn't out yet.
void jiggy_jinjo_jiggy_update(Actor *player) {
    u32 frames;
    f32 dx, dz;

    if ((s32)current_map != jiggy_map) {
        jiggy_map_load();
    }
    frames = object_timer - jiggy_last_timer;
    jiggy_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    if ((jiggy_flag < 0 && jiggy_reward < 0) || !(jiggy_state & JINJO_JIGGY_EARNED) ||
        (jiggy_state & JINJO_JIGGY_COLLECTED)) {
        return;
    }
    if (jiggy_flag < 0) {
        // A jiggy_rewards.c reward: it spawns, respawns and records the Golden Banana itself.
        if (jiggy_wait > 0) {
            jiggy_wait -= frames;
            return;
        }
        if (jiggy_reward_update(jiggy_reward, jiggy_spot[0], jiggy_spot[1] + JIGGY_HEIGHT, jiggy_spot[2], player, frames)) {
            jiggy_state |= JINJO_JIGGY_COLLECTED;
            jiggy_save_set(current_file, JINJO_JIGGY_SAVE_KEY(current_map), jiggy_state);
            return;
        }
        if (jiggy_fanfare) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
            jiggy_fanfare = FALSE;
        }
        return;
    }
    // Collected: DK64 set the flag when the player picked it up.
    if (isFlagSet(jiggy_flag, FLAG_TYPE_PERMANENT)) {
        jiggy_state |= JINJO_JIGGY_COLLECTED;
        jiggy_save_set(current_file, JINJO_JIGGY_SAVE_KEY(current_map), jiggy_state);
        return;
    }
    if (jiggy_spawner != NULL) {
        if (jiggy_spawner_alive()) {
            return;
        }
        // Collected (the flag's coming), or gone with the map (reloaded, such as after a death).
        jiggy_spawner = NULL;
        jiggy_wait = JIGGY_RESPAWN_FRAMES;
    }
    if (jiggy_wait > 0) {
        jiggy_wait -= frames;
        return;
    }
    dx = player->x_position - jiggy_spot[0];
    dz = player->z_position - jiggy_spot[2];
    if (dx * dx + dz * dz < JIGGY_APPEAR_CLEARANCE * JIGGY_APPEAR_CLEARANCE) {
        return;
    }
    func_global_asm_806A5DF0(REWARD_GOLDEN_BANANA, jiggy_spot[0], jiggy_spot[1] + JIGGY_HEIGHT, jiggy_spot[2],
                             0, 0, jiggy_flag, 0);
    // func_global_asm_80689250 puts the new spawner at the head of the list.
    jiggy_spawner = actor_spawner_pointer;
    if (jiggy_fanfare) {
        jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        jiggy_fanfare = FALSE;
    }
}
