// Grabba: Gobi's Valley's sand-dwelling hand, read from the player's Banjo-Kazooie ROM, as a
// boss in Angry Aztec's east sand room (by the bonus barrel, past the tunnel from the first
// area), where the two worlds have collided.
//
// As in Gobi's Valley (chGrabba, src/GV/ch/grabba.c), Grabba (ASSET_371_MODEL_GRABBA) waits
// under the sand and rises out of it with a rumble (SFX_3EC, as his sfx source plays it while
// he moves) when the player comes into his room; the first time, he boasts in a text box with
// his portrait (ASSET_7FF_SPRITE_GRABBA) and voice (SFX_D7_GRABBA_DEATH, as gczoombox plays
// it). Up out of the sand he turns to the player, and when they come in reach he winds up and
// slaps the ground in front of him (as the Slappas do, SFX_3_DULL_CANNON_SHOT): caught by it,
// the player is hurt through DK64's own enemy hit. After a while he ducks back into the sand
// (SFX_3F5) and comes up at another of his four holes.
//
// He's beaten by three hits while he's up: the player's attacks (as the Grublins are hit) or
// shots (DK64's guns, and Banjo's eggs). Each hit sends him under with a howl; the third beats
// him: he sinks slowly with Banjo-Kazooie's "ohww" (SFX_8D, as func_8038B988 plays it), admits
// it in the text box, and a Golden Banana (drawn as a Jiggy) appears
// (jiggy_rewards.c, reward 0x22) in the middle of his room, with Banjo-Kazooie's Jiggy jingle.
//
// Gobi's Valley's models move their vertices themselves (jiggy_aztec.h), so he's drawn at rest
// and moved as a whole: rising, swaying, leaning into his slap.
//
// His boast and his defeat are kept with the save file (AZTEC_SAVE_KEY, character 2); a fight
// left half way starts again on the next visit. Drawn right after the player's own model
// (func_global_asm_80630DCC in patches_draw.c); the text box is jiggy_aztec.c's. Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "jiggy_aztec.h"

#define GRABBA_MODEL 0x371    // ASSET_371_MODEL_GRABBA
#define GRABBA_PORTRAIT 0x7FF // ASSET_7FF_SPRITE_GRABBA (64 pixels across, 10 frames)
#define GRABBA_MOUTH_FRAMES 10
// Grabba converts to about 5 KiB with 25 skeleton entries, posed in under 16 commands.
#define GRABBA_BLOCK_MAX 0x1800
#define GRABBA_MTX_MAX 26
#define GRABBA_GFX_MAX 24

#define GRABBA_SFX_VOICE 0xD7  // SFX_D7_GRABBA_DEATH: his voice (gczoombox: 30000/32767, 0.7)
#define GRABBA_SFX_OHWW 0x8D   // SFX_8D_BOGGY_OHWW: beaten, 32000/32767, 0.9
#define GRABBA_SFX_SLAP 0x03   // SFX_3_DULL_CANNON_SHOT: the slap landing, 32000/32767, 1.0
static const u16 grabba_voice[] = {GRABBA_SFX_VOICE};

// The arena: the west half of the sand room east of the tunnel's end (the bonus barrel and its
// banana bunches are further east, at x 4150-4330), floor at 120. He fights while the player is
// within the arena's reach of its middle, and comes up at one of these holes.
static const f32 arena_middle[3] = {3920.0f, 120.0f, 2410.0f};
#define ARENA_RADIUS 260.0f
#define ARENA_WAKE_RADIUS 200.0f
static const f32 grabba_holes[][3] = {
    {3870.0f, 120.0f, 2330.0f},
    {4010.0f, 120.0f, 2370.0f},
    {3950.0f, 120.0f, 2470.0f},
    {3830.0f, 120.0f, 2470.0f},
};
#define HOLE_COUNT 4

static const char *const grabba_meet[] = {
    "GRRR! WHO STOMPS ON GRABBA'S SAND? NO MONKEY OR BEAR GETS PAST MY MIGHTY FINGERS!",
    "COME CLOSER... GRABBA WILL GRAB YOU AND SLAP YOU FLAT!",
};
static const char *const grabba_defeat[] = {
    "OWW! MY KNUCKLES! ALL RIGHT, YOU WIN... TAKE GRABBA'S SHINY TREASURE AND LEAVE ME BE!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

#define GRABBA_SAVE_MET 0x1
#define GRABBA_SAVE_DEFEATED 0x2
#define GRABBA_OWNER AZTEC_CHARACTER_GRABBA

// Sizes and timings, in DK64 units and seconds (chGrabba's animations: rise 1.6, hide 0.55).
#define GRABBA_SCALE 1.0f
#define GRABBA_SINK 85.0f // How far under the sand he hides (his hand's height, and more).
#define GRABBA_RISE_SECONDS 1.2f
#define GRABBA_UP_SECONDS 4.0f
#define GRABBA_HIDE_SECONDS 0.55f
#define GRABBA_UNDER_SECONDS 1.4f
#define GRABBA_WINDUP_SECONDS 0.5f
#define GRABBA_SLAP_SECONDS 0.35f
#define GRABBA_DOWN_SECONDS 1.2f // Lying flat after a slap.
// How far forward the slap leans him: short of flat, so his arm (under the sand, pivoting about
// the sand's surface) stays under it.
#define GRABBA_SLAP_PITCH 70.0f
#define GRABBA_DEFEAT_SINK_SPEED (7.0f * AZTEC_BK_TO_DK64 * 30.0f) // func_8038C488's 7 a frame
#define GRABBA_REACH 55.0f   // He slaps when the player's this close.
#define GRABBA_SLAP_RADIUS 34.0f // The slap lands this far in front of him, hurting within this.
#define GRABBA_SLAP_FORWARD 30.0f
#define GRABBA_BODY_RADIUS 22.0f
#define GRABBA_BODY_HEIGHT 80.0f
#define GRABBA_HITS 3
#define GRABBA_TURN_RATE 0.15f
#define REWARD_HEIGHT 12.5f
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
s32 func_global_asm_806EB400(Actor *player); // Whether the player can be hurt now.

typedef enum {
    GRABBA_UNLOADED,
    GRABBA_READY,
    GRABBA_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} GrabbaLoadState;

typedef enum {
    GRABBA_DORMANT,  // Under the sand, the player out of his room.
    GRABBA_RISING,
    GRABBA_UP,
    GRABBA_WINDUP,
    GRABBA_SLAPPING,
    GRABBA_DOWN,     // Lying flat after a slap.
    GRABBA_HIDING,
    GRABBA_UNDER,    // Between holes.
    GRABBA_SINKING,  // Beaten.
    GRABBA_BEATEN,   // Gone; his Golden Banana's out.
} GrabbaAction;

static u8 grabba_block[GRABBA_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx grabba_gfx[2][GRABBA_GFX_MAX];
static Mtx grabba_mtx[2][GRABBA_MTX_MAX];
static JiggyBkPose grabba_pose;
static GrabbaLoadState grabba_state = GRABBA_UNLOADED;

static s32 grabba_last_map = -1;
static u32 grabba_last_timer;
static s32 grabba_placed;
static u32 grabba_saved;
static GrabbaAction grabba_action;
static f32 grabba_pos[3];
static f32 grabba_yaw;
static f32 grabba_time;
static f32 grabba_depth; // How far under the sand, 0 to GRABBA_SINK (and more while beaten).
static s32 grabba_hole;
static s32 grabba_hits;
static s32 grabba_rumble;
static s32 grabba_met_pending; // Boast once he's up.
static f32 reward_pos[3];
static u32 grabba_random_state = 0x47524142;

static f32 grabba_random(void) {
    grabba_random_state = grabba_random_state * 1664525 + 1013904223;
    return (grabba_random_state >> 8) * (1.0f / 16777216.0f);
}

static void grabba_load(void) {
    grabba_state = GRABBA_UNAVAILABLE;
    if (!jiggy_bk_available() || !aztec_model_load(GRABBA_MODEL, grabba_block, sizeof(grabba_block))) {
        return;
    }
    aztec_pose_init(&grabba_pose);
    grabba_state = GRABBA_READY;
}

static void grabba_save(u32 bits) {
    grabba_saved |= bits;
    jiggy_save_set(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_GRABBA), grabba_saved);
}

static void grabba_talk(const char *const *pages, s32 count) {
    aztec_talk_start(GRABBA_OWNER, GRABBA_PORTRAIT, GRABBA_MOUTH_FRAMES, "GRABBA", pages, count, grabba_voice,
                     ARRAY_COUNT_OF(grabba_voice), 30000.0f / 32767.0f, 0.7f);
}

static void grabba_stop_rumble(void) {
    if (grabba_rumble > 0) {
        jiggy_bk_sound_stop(grabba_rumble);
        grabba_rumble = 0;
    }
}

static void grabba_start(GrabbaAction action) {
    grabba_action = action;
    grabba_time = 0.0f;
}

// Moves him (under the sand) to a hole: not the one he's at, and not one the player stands on.
static void grabba_pick_hole(Actor *player) {
    s32 tries;

    for (tries = 0; tries < 8; tries++) {
        s32 hole = (s32)(grabba_random() * HOLE_COUNT) % HOLE_COUNT;

        if (hole == grabba_hole) {
            continue;
        }
        grabba_hole = hole;
        if (aztec_distance_xz(grabba_holes[hole][0], grabba_holes[hole][2], player->x_position, player->z_position) > 45.0f) {
            break;
        }
    }
    grabba_pos[0] = grabba_holes[grabba_hole][0];
    grabba_pos[1] = aztec_floor(grabba_holes[grabba_hole][0], grabba_holes[grabba_hole][1], grabba_holes[grabba_hole][2],
                                FLOOR_PROBE, FLOOR_MAX_STEP);
    grabba_pos[2] = grabba_holes[grabba_hole][2];
    grabba_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, grabba_pos[0], grabba_pos[2]) & 0xFFF;
}

// His Golden Banana goes in the middle of his arena.
static void grabba_reward_place(void) {
    reward_pos[0] = arena_middle[0];
    reward_pos[1] = aztec_floor(arena_middle[0], arena_middle[1], arena_middle[2], FLOOR_PROBE, FLOOR_MAX_STEP) + REWARD_HEIGHT;
    reward_pos[2] = arena_middle[2];
}

static void grabba_place(Actor *player) {
    grabba_hole = -1;
    grabba_pick_hole(player);
    grabba_depth = GRABBA_SINK;
    grabba_hits = 0;
    grabba_met_pending = FALSE;
    if (grabba_saved & GRABBA_SAVE_DEFEATED) {
        // Beaten: his Golden Banana waits at the arena's middle until it's collected.
        grabba_start(GRABBA_BEATEN);
        grabba_reward_place();
    } else {
        grabba_start(GRABBA_DORMANT);
    }
    grabba_placed = TRUE;
}

static s32 grabba_player_in_arena(Actor *player, f32 radius) {
    f32 dy = player->y_position - arena_middle[1];

    return aztec_distance_xz(arena_middle[0], arena_middle[2], player->x_position, player->z_position) <= radius &&
           dy < 80.0f && dy > -40.0f;
}

// Hit by the player while he's out of the sand.
static s32 grabba_hit(Actor *player) {
    f32 body[3];

    if (grabba_depth > GRABBA_SINK * 0.5f) {
        return FALSE;
    }
    body[0] = grabba_pos[0];
    body[1] = grabba_pos[1];
    body[2] = grabba_pos[2];
    return aztec_player_attacks(player, body, GRABBA_BODY_RADIUS) ||
           aztec_player_shot(player, body, GRABBA_BODY_RADIUS, GRABBA_BODY_HEIGHT);
}

static void grabba_update(Actor *player, u32 frames) {
    f32 seconds = frames / AZTEC_GAME_FPS;
    s32 talking = aztec_talk_owner() == GRABBA_OWNER;
    s32 finished = FALSE;
    s32 in_arena = grabba_player_in_arena(player, ARENA_RADIUS);
    f32 distance = aztec_distance_xz(grabba_pos[0], grabba_pos[2], player->x_position, player->z_position);

    if (talking) {
        finished = aztec_talk_update(GRABBA_OWNER);
    }
    if (frames == 0) {
        return;
    }
    grabba_time += seconds;
    // Hits come first, whenever he's out and fighting.
    if (!talking && grabba_action >= GRABBA_RISING && grabba_action <= GRABBA_DOWN && grabba_hit(player)) {
        grabba_hits++;
        grabba_stop_rumble();
        if (grabba_hits >= GRABBA_HITS) {
            grabba_save(GRABBA_SAVE_DEFEATED | GRABBA_SAVE_MET);
            aztec_sfx_at(player, grabba_pos[0], grabba_pos[2], GRABBA_SFX_OHWW, 32000.0f / 32767.0f, 0.9f);
            grabba_rumble = jiggy_bk_sfx_play(AZTEC_SFX_SAND_RUMBLE, 0.5f, 0.8f);
            aztec_lock_player(TRUE);
            grabba_start(GRABBA_SINKING);
        } else {
            aztec_sfx_at(player, grabba_pos[0], grabba_pos[2], GRABBA_SFX_VOICE, 1.0f, 1.0f + 0.15f * grabba_hits);
            grabba_start(GRABBA_HIDING);
        }
        return;
    }
    switch (grabba_action) {
        case GRABBA_DORMANT:
            if (grabba_player_in_arena(player, ARENA_WAKE_RADIUS) && player->y_position - player->floor <= AZTEC_AIRBORNE_HEIGHT &&
                !aztec_player_locked()) {
                grabba_pick_hole(player);
                grabba_met_pending = !(grabba_saved & GRABBA_SAVE_MET);
                grabba_rumble = jiggy_bk_sfx_play(AZTEC_SFX_SAND_RUMBLE, 0.5f, 1.0f);
                grabba_start(GRABBA_RISING);
            }
            break;
        case GRABBA_RISING:
            aztec_turn_toward(&grabba_yaw, player->x_position, player->z_position, grabba_pos[0], grabba_pos[2],
                              GRABBA_TURN_RATE, frames);
            grabba_depth = GRABBA_SINK * (1.0f - grabba_time / GRABBA_RISE_SECONDS);
            if (grabba_time >= GRABBA_RISE_SECONDS) {
                grabba_depth = 0.0f;
                grabba_stop_rumble();
                grabba_start(GRABBA_UP);
                if (grabba_met_pending) {
                    grabba_met_pending = FALSE;
                    grabba_save(GRABBA_SAVE_MET);
                    grabba_talk(grabba_meet, ARRAY_COUNT_OF(grabba_meet));
                }
            }
            break;
        case GRABBA_UP:
            if (talking) {
                grabba_time = 0.0f;
                break;
            }
            aztec_turn_toward(&grabba_yaw, player->x_position, player->z_position, grabba_pos[0], grabba_pos[2],
                              GRABBA_TURN_RATE, frames);
            if (!in_arena || grabba_time >= GRABBA_UP_SECONDS) {
                grabba_start(GRABBA_HIDING);
            } else if (distance <= GRABBA_REACH) {
                grabba_start(GRABBA_WINDUP);
            }
            break;
        case GRABBA_WINDUP:
            aztec_turn_toward(&grabba_yaw, player->x_position, player->z_position, grabba_pos[0], grabba_pos[2],
                              GRABBA_TURN_RATE, frames);
            if (grabba_time >= GRABBA_WINDUP_SECONDS) {
                grabba_start(GRABBA_SLAPPING);
            }
            break;
        case GRABBA_SLAPPING:
            if (grabba_time >= GRABBA_SLAP_SECONDS) {
                f32 x = grabba_pos[0] + func_global_asm_80612794((s16)grabba_yaw) * GRABBA_SLAP_FORWARD;
                f32 z = grabba_pos[2] + func_global_asm_80612790((s16)grabba_yaw) * GRABBA_SLAP_FORWARD;
                f32 dy = player->y_position - grabba_pos[1];

                aztec_sfx_at(player, grabba_pos[0], grabba_pos[2], GRABBA_SFX_SLAP, 1.0f, 0.9f);
                if (aztec_distance_xz(x, z, player->x_position, player->z_position) <= GRABBA_SLAP_RADIUS && dy < 40.0f &&
                    dy > -20.0f && func_global_asm_806EB400(player)) {
                    aztec_hurt_player(player, grabba_pos[0], grabba_pos[2]);
                }
                grabba_start(GRABBA_DOWN);
            }
            break;
        case GRABBA_DOWN:
            if (grabba_time >= GRABBA_DOWN_SECONDS) {
                grabba_start(GRABBA_HIDING);
            }
            break;
        case GRABBA_HIDING:
            grabba_depth = GRABBA_SINK * (grabba_time / GRABBA_HIDE_SECONDS);
            if (grabba_time >= GRABBA_HIDE_SECONDS) {
                grabba_depth = GRABBA_SINK;
                aztec_sfx_at(player, grabba_pos[0], grabba_pos[2], AZTEC_SFX_SAND_POP, 1.0f, 0.95f + grabba_random() * 0.1f);
                grabba_start(GRABBA_UNDER);
            }
            break;
        case GRABBA_UNDER:
            if (!in_arena) {
                grabba_hits = 0;
                grabba_start(GRABBA_DORMANT);
            } else if (grabba_time >= GRABBA_UNDER_SECONDS) {
                grabba_pick_hole(player);
                grabba_rumble = jiggy_bk_sfx_play(AZTEC_SFX_SAND_RUMBLE, 0.5f, 1.0f);
                grabba_start(GRABBA_RISING);
            }
            break;
        case GRABBA_SINKING:
            aztec_lock_player(TRUE);
            grabba_depth += GRABBA_DEFEAT_SINK_SPEED * seconds;
            if (grabba_depth >= GRABBA_SINK && !talking && aztec_talk_owner() < 0 && grabba_time < 100.0f) {
                grabba_stop_rumble();
                grabba_talk(grabba_defeat, ARRAY_COUNT_OF(grabba_defeat));
                grabba_time = 100.0f; // Said.
            }
            if (finished) {
                grabba_reward_place();
                jiggy_bk_jingle_play(AZTEC_JINGLE_JIGGY_SPAWN, 1.0f);
                grabba_start(GRABBA_BEATEN);
            }
            break;
        default:
            break;
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// Grabba once a frame, with the player.
Gfx *jiggy_grabba_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    f32 pitch = 0.0f;
    f32 pos[3];

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != grabba_last_map || object_timer < grabba_last_timer) {
        if (aztec_talk_owner() == GRABBA_OWNER) {
            aztec_talk_cancel();
        }
        if (grabba_action == GRABBA_SINKING) {
            aztec_lock_player(FALSE);
        }
        grabba_stop_rumble();
        grabba_last_map = current_map;
        grabba_placed = FALSE;
        grabba_saved = current_map == MAP_AZTEC ? jiggy_save_get(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_GRABBA)) : 0;
    }
    if (current_map != MAP_AZTEC) {
        grabba_last_timer = object_timer;
        return dl;
    }
    if (grabba_state == GRABBA_UNLOADED) {
        grabba_load();
    }
    if (grabba_state != GRABBA_READY) {
        return dl;
    }
    if (!grabba_placed) {
        grabba_last_timer = object_timer;
        if (actor->y_position - actor->floor > AZTEC_AIRBORNE_HEIGHT) {
            return dl;
        }
        grabba_place(actor);
    }
    frames = aztec_frames(&grabba_last_timer);
    if (aztec_player_locked() && aztec_talk_owner() != GRABBA_OWNER && grabba_action != GRABBA_SINKING) {
        frames = 0;
    }
    grabba_update(actor, frames);

    if (grabba_action == GRABBA_BEATEN) {
        jiggy_reward_update(AZTEC_REWARD_GRABBA, reward_pos[0], reward_pos[1], reward_pos[2], actor, frames);
        return dl;
    }
    if (grabba_action == GRABBA_DORMANT || grabba_action == GRABBA_UNDER || grabba_depth >= GRABBA_SINK) {
        return dl;
    }
    switch (grabba_action) {
        case GRABBA_UP:
            // Flexing his fingers.
            pitch = 5.0f * func_global_asm_80612794((s16)(grabba_time * 4096.0f / 1.8f));
            break;
        case GRABBA_WINDUP:
            pitch = -25.0f * (grabba_time / GRABBA_WINDUP_SECONDS);
            break;
        case GRABBA_SLAPPING:
            pitch = -25.0f + (GRABBA_SLAP_PITCH + 25.0f) * (grabba_time / GRABBA_SLAP_SECONDS);
            break;
        case GRABBA_DOWN:
            pitch = GRABBA_SLAP_PITCH * (1.0f - (grabba_time / GRABBA_DOWN_SECONDS) * (grabba_time / GRABBA_DOWN_SECONDS));
            break;
        default:
            break;
    }
    pos[0] = grabba_pos[0];
    pos[1] = grabba_pos[1] - grabba_depth;
    pos[2] = grabba_pos[2];
    dl = aztec_model_draw(dl, &grabba_pose, GRABBA_MODEL, grabba_block, pos, grabba_yaw, GRABBA_SCALE, 1.0f, pitch, NULL,
                          grabba_gfx[buffer], GRABBA_GFX_MAX, grabba_mtx[buffer], GRABBA_MTX_MAX);
    return dl;
}
