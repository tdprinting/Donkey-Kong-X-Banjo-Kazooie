// Trunker: Gobi's Valley's little tree, read from the player's Banjo-Kazooie ROM, in Angry
// Aztec's first sand area; with his quest, shared with Gobi (jiggy_gobi.c).
//
// As in Gobi's Valley (chTrunker, src/GV/ch/trunker.c), Trunker (ASSET_3DF_MODEL_TRUNKER) is
// stunted and thirsty; the first time the player comes near he says so in a text box, with
// his talking sound (SFX_F3_TRUNKER_TALKING, as gczoombox plays it). Once Gobi is freed from his
// rock he comes to stand beside Trunker; the player's slam on Gobi's hump makes him spit his
// water over Trunker (jiggy_trunker_watered), and Trunker grows to his full height (his
// ASSET_FF_ANIM_TRUCKER_GROW, 2.5 seconds, here as a stretch from the stunted height) with
// Banjo-Kazooie's rumble, thanks the player, and a Golden Banana (drawn as a Jiggy) appears in
// front of him (jiggy_rewards.c, reward 0x21) with Banjo-Kazooie's Jiggy jingle. The player
// is held in the game's cutscene lock from the spit until the Golden Banana appears.
//
// Gobi's Valley's models move their vertices themselves (jiggy_aztec.h), so Trunker is drawn
// at rest, squashed down while he's stunted and swaying a little.
//
// His talk and his growth are kept with the save file (AZTEC_SAVE_KEY, character 1). Drawn
// right after the player's own model (func_global_asm_80630DCC in patches_draw.c); the text box
// is jiggy_aztec.c's. Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "jiggy_aztec.h"

#define TRUNKER_MODEL 0x3DF    // ASSET_3DF_MODEL_TRUNKER
#define TRUNKER_PORTRAIT 0x801 // ASSET_801_SPRITE_TRUNKER (64 pixels across; gczoombox: 6 mouth frames)
#define TRUNKER_MOUTH_FRAMES 6
// Trunker converts to about 20 KiB with 14 skeleton entries, posed in under 16 commands.
#define TRUNKER_BLOCK_MAX 0x5000
#define TRUNKER_MTX_MAX 16
#define TRUNKER_GFX_MAX 24
#define TRUNKER_SFX_TALKING 0xF3 // SFX_F3_TRUNKER_TALKING (gczoombox: 20000/32767, 0.9)
static const u16 trunker_voice[] = {TRUNKER_SFX_TALKING};

// How tall he stands: stunted, and grown (his model's own height, 371 units).
#define TRUNKER_SHORT 0.35f
#define TRUNKER_GROW_SECONDS 2.5f
#define TRUNKER_TALK_RADIUS 85.0f
#define TRUNKER_REARM_RADIUS 250.0f
#define TRUNKER_TALK_HEIGHT 40.0f
#define TRUNKER_REMIND_FRAMES (60 * 30)
#define REWARD_HEIGHT 12.5f
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f
// His Golden Banana appears this far in front of him, toward the middle of the area.
static const f32 reward_spot[3] = {2560.0f, 110.0f, 1110.0f};

static const char *const trunker_meet[] = {
    "HELLO DOWN THERE! WELL, UP THERE. I'M TRUNKER, AND I'M MUCH TOO SHORT FOR A TREE.",
    "IT'S ALL THIS SAND. NOT A DROP TO DRINK! IF ONLY SOMEONE WITH A HUMP FULL OF WATER CAME BY...",
};
static const char *const trunker_reminder[] = {
    "SO... THIRSTY... A CAMEL'S HUMP HOLDS PLENTY OF WATER, YOU KNOW.",
};
static const char *const trunker_thanks[] = {
    "AAH, LOVELY! LOOK AT ME, I'M A PROPER TREE NOW! HERE, THIS FELL OUT OF MY BRANCHES.",
};
static const char *const trunker_grown[] = {
    "I CAN SEE ALL THE WAY TO THE TEMPLES FROM UP HERE!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

#define TRUNKER_SAVE_TALKED 0x1
#define TRUNKER_SAVE_GROWN 0x2
#define TRUNKER_OWNER AZTEC_CHARACTER_TRUNKER

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z);

typedef enum {
    TRUNKER_UNLOADED,
    TRUNKER_READY,
    TRUNKER_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} TrunkerLoadState;

typedef enum {
    TRUNKER_STUNTED,
    TRUNKER_WAITING_TO_GROW, // Gobi's spit is on its way (held until it lands).
    TRUNKER_GROWING,
    TRUNKER_THANKING,
    TRUNKER_TALL,
} TrunkerAction;

static u8 trunker_block[TRUNKER_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx trunker_gfx[2][TRUNKER_GFX_MAX];
static Mtx trunker_mtx[2][TRUNKER_MTX_MAX];
static JiggyBkPose trunker_pose;
static TrunkerLoadState trunker_state = TRUNKER_UNLOADED;

static s32 trunker_last_map = -1;
static u32 trunker_last_timer;
static s32 trunker_placed;
static u32 trunker_saved;
static TrunkerAction trunker_action;
static f32 trunker_pos[3];
static f32 trunker_yaw;
static f32 trunker_time;
static f32 trunker_height = TRUNKER_SHORT;
static s32 trunker_armed;
static u32 trunker_last_talk;
static s32 trunker_rumble;
static s32 trunker_water_pending;
static f32 reward_pos[3];

static void trunker_load(void) {
    trunker_state = TRUNKER_UNAVAILABLE;
    if (!jiggy_bk_available() || !aztec_model_load(TRUNKER_MODEL, trunker_block, sizeof(trunker_block))) {
        return;
    }
    aztec_pose_init(&trunker_pose);
    trunker_state = TRUNKER_READY;
}

static void trunker_save(u32 bits) {
    trunker_saved |= bits;
    jiggy_save_set(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_TRUNKER), trunker_saved);
}

static void trunker_talk(const char *const *pages, s32 count) {
    aztec_talk_start(TRUNKER_OWNER, TRUNKER_PORTRAIT, TRUNKER_MOUTH_FRAMES, "TRUNKER", pages, count, trunker_voice,
                     ARRAY_COUNT_OF(trunker_voice), 20000.0f / 32767.0f, 0.9f);
}

// Gobi's spit has landed on Trunker (jiggy_gobi.c): he grows.
void jiggy_trunker_watered(void) {
    trunker_water_pending = TRUE;
}

static void trunker_place(Actor *player) {
    u32 gobi = jiggy_save_get(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_GOBI));

    trunker_pos[0] = TRUNKER_X;
    trunker_pos[1] = aztec_floor(TRUNKER_X, TRUNKER_Y, TRUNKER_Z, FLOOR_PROBE, FLOOR_MAX_STEP);
    trunker_pos[2] = TRUNKER_Z;
    reward_pos[0] = reward_spot[0];
    reward_pos[1] = aztec_floor(reward_spot[0], reward_spot[1], reward_spot[2], FLOOR_PROBE, FLOOR_MAX_STEP) + REWARD_HEIGHT;
    reward_pos[2] = reward_spot[2];
    trunker_yaw = func_global_asm_80665DE0(reward_spot[0], reward_spot[2], trunker_pos[0], trunker_pos[2]) & 0xFFF;
    trunker_time = 0.0f;
    trunker_water_pending = FALSE;
    // Grown if Gobi's water reached him (a save from before his growth was kept counts too).
    if ((trunker_saved & TRUNKER_SAVE_GROWN) || (gobi & GOBI_SAVE_WATERED)) {
        trunker_action = TRUNKER_TALL;
        trunker_height = 1.0f;
        if (!(trunker_saved & TRUNKER_SAVE_GROWN)) {
            trunker_save(TRUNKER_SAVE_GROWN);
        }
    } else {
        trunker_action = TRUNKER_STUNTED;
        trunker_height = TRUNKER_SHORT;
    }
    trunker_armed = aztec_distance_xz(trunker_pos[0], trunker_pos[2], player->x_position, player->z_position) > TRUNKER_REARM_RADIUS;
    trunker_placed = TRUE;
}

static s32 trunker_player_near(Actor *player, f32 radius) {
    f32 dy = player->y_position - trunker_pos[1];

    return aztec_distance_xz(trunker_pos[0], trunker_pos[2], player->x_position, player->z_position) <= radius &&
           dy <= TRUNKER_TALK_HEIGHT && dy >= -TRUNKER_TALK_HEIGHT;
}

// His first talk when the player comes near, then reminders by Bottles' rule (jiggy_bottles.c).
static void trunker_maybe_talk(Actor *player) {
    s32 first = !(trunker_saved & TRUNKER_SAVE_TALKED) && trunker_action == TRUNKER_STUNTED;

    if (aztec_talk_owner() >= 0 || aztec_player_locked()) {
        return;
    }
    if (!trunker_armed) {
        trunker_armed = !trunker_player_near(player, TRUNKER_REARM_RADIUS) &&
                        object_timer - trunker_last_talk >= TRUNKER_REMIND_FRAMES;
        return;
    }
    if (player->y_position - player->floor > AZTEC_AIRBORNE_HEIGHT || !trunker_player_near(player, TRUNKER_TALK_RADIUS)) {
        return;
    }
    if (!first && player->control_state != 0x0C) {
        return;
    }
    trunker_armed = FALSE;
    trunker_last_talk = object_timer;
    if (first) {
        trunker_save(TRUNKER_SAVE_TALKED);
        trunker_talk(trunker_meet, ARRAY_COUNT_OF(trunker_meet));
    } else if (trunker_action == TRUNKER_TALL) {
        trunker_talk(trunker_grown, ARRAY_COUNT_OF(trunker_grown));
    } else {
        trunker_talk(trunker_reminder, ARRAY_COUNT_OF(trunker_reminder));
    }
}

static void trunker_update(Actor *player, u32 frames) {
    f32 seconds = frames / AZTEC_GAME_FPS;
    s32 talking = aztec_talk_owner() == TRUNKER_OWNER;
    s32 finished = FALSE;

    if (talking) {
        finished = aztec_talk_update(TRUNKER_OWNER);
    }
    trunker_time += seconds;
    switch (trunker_action) {
        case TRUNKER_STUNTED:
            if (trunker_water_pending) {
                trunker_water_pending = FALSE;
                trunker_action = TRUNKER_GROWING;
                trunker_time = 0.0f;
                trunker_save(TRUNKER_SAVE_GROWN | TRUNKER_SAVE_TALKED);
                aztec_lock_player(TRUE);
                trunker_rumble = jiggy_bk_sfx_play(AZTEC_SFX_SAND_RUMBLE, 0.6f, 1.0f);
                break;
            }
            if (!talking && frames > 0) {
                trunker_maybe_talk(player);
            }
            break;
        case TRUNKER_GROWING:
            aztec_lock_player(TRUE);
            trunker_height = TRUNKER_SHORT + (1.0f - TRUNKER_SHORT) * (trunker_time / TRUNKER_GROW_SECONDS);
            if (trunker_time >= TRUNKER_GROW_SECONDS) {
                trunker_height = 1.0f;
                if (trunker_rumble > 0) {
                    jiggy_bk_sound_stop(trunker_rumble);
                    trunker_rumble = 0;
                }
                trunker_action = TRUNKER_THANKING;
                trunker_talk(trunker_thanks, ARRAY_COUNT_OF(trunker_thanks));
            }
            break;
        case TRUNKER_THANKING:
            if (finished) {
                jiggy_bk_jingle_play(AZTEC_JINGLE_JIGGY_SPAWN, 1.0f);
                trunker_action = TRUNKER_TALL;
                trunker_armed = FALSE;
                trunker_last_talk = object_timer;
            }
            break;
        case TRUNKER_TALL:
            if (!talking && frames > 0) {
                trunker_maybe_talk(player);
            }
            break;
        default:
            break;
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// Trunker once a frame, with the player.
Gfx *jiggy_trunker_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    f32 sway;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != trunker_last_map || object_timer < trunker_last_timer) {
        if (aztec_talk_owner() == TRUNKER_OWNER) {
            aztec_talk_cancel();
        }
        if (trunker_action == TRUNKER_GROWING) {
            aztec_lock_player(FALSE);
        }
        if (trunker_rumble > 0) {
            jiggy_bk_sound_stop(trunker_rumble);
            trunker_rumble = 0;
        }
        trunker_last_map = current_map;
        trunker_placed = FALSE;
        trunker_saved = current_map == MAP_AZTEC ? jiggy_save_get(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_TRUNKER)) : 0;
    }
    if (current_map != MAP_AZTEC) {
        trunker_last_timer = object_timer;
        return dl;
    }
    if (trunker_state == TRUNKER_UNLOADED) {
        trunker_load();
    }
    if (trunker_state != TRUNKER_READY) {
        return dl;
    }
    if (!trunker_placed) {
        trunker_last_timer = object_timer;
        if (actor->y_position - actor->floor > AZTEC_AIRBORNE_HEIGHT) {
            return dl;
        }
        trunker_place(actor);
    }
    frames = aztec_frames(&trunker_last_timer);
    if (aztec_player_locked() && aztec_talk_owner() != TRUNKER_OWNER && trunker_action != TRUNKER_GROWING &&
        !trunker_water_pending) {
        frames = 0;
    }
    trunker_update(actor, frames);

    // His Golden Banana, once he's thanked the player, until it's collected.
    if (trunker_action == TRUNKER_TALL) {
        jiggy_reward_update(AZTEC_REWARD_TRUNKER, reward_pos[0], reward_pos[1], reward_pos[2], actor, frames);
    }

    // A little sway, quick while he grows.
    sway = func_global_asm_80612794((s16)(trunker_time * 4096.0f / (trunker_action == TRUNKER_GROWING ? 0.3f : 4.0f)));
    dl = aztec_model_draw(dl, &trunker_pose, TRUNKER_MODEL, trunker_block, trunker_pos, trunker_yaw, 1.0f, trunker_height,
                          sway * (trunker_action == TRUNKER_GROWING ? 4.0f : 2.0f), NULL, trunker_gfx[buffer],
                          TRUNKER_GFX_MAX, trunker_mtx[buffer], TRUNKER_MTX_MAX);
    return dl;
}
