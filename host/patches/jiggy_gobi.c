// Gobi: Gobi's Valley's camel, read from the player's Banjo-Kazooie ROM, in Angry Aztec's first
// sand area, where the two worlds have collided; with his two quests.
//
// As in Gobi's Valley (chGobi1, chGobiRock, chGobi2 in src/GV/ch), Gobi (ASSET_3E0_MODEL_GOBI)
// is tied to a rock (ASSET_3E4_MODEL_GOBI_ROCK) by a rope (ASSET_3E3_MODEL_GOBI_ROPE), tugging
// at it and crying (SFX_84_GOBI_CRYING) now and then. The first time the player comes near he
// begs for help in a text box. Three hits break the rock: any of the player's attacks (as the
// Grublins are hit), the Simian Slam (Banjo-Kazooie's Beak Buster breaks it) or a shot. Each
// hit shakes it; the third breaks it with Banjo-Kazooie's boulder sounds (SFX_9B, four times as
// chGobiRock plays them) and puzzle solved fanfare. Gobi thanks the player, a Golden Banana
// (drawn as a Jiggy) appears where the rock stood (jiggy_rewards.c, reward 0x20), and he runs
// off to Trunker (jiggy_trunker.c) with the running sounds BK plays (SFX_2E).
//
// By Trunker he waits with his hump full of water. The player's Simian Slam (or Banjo's Beak
// Buster) on his back makes him gulp (SFX_4B_GULPING, three times as chGobi2 plays them) and
// spit the water over Trunker, who grows (jiggy_trunker_watered). That Jiggy is Trunker's.
//
// Gobi's Valley's models move their vertices themselves (jiggy_aztec.h), so Gobi is drawn at
// rest and moved as a whole: leaning back as he tugs, bobbing as he runs, rearing to spit.
//
// His talks, the rock and the water are kept with the save file (AZTEC_SAVE_KEY, character 0).
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c); the
// text box is jiggy_aztec.c's. Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "jiggy_aztec.h"

#define GOBI_MODEL 0x3E0 // ASSET_3E0_MODEL_GOBI
#define ROCK_MODEL 0x3E4 // ASSET_3E4_MODEL_GOBI_ROCK
#define ROPE_MODEL 0x3E3 // ASSET_3E3_MODEL_GOBI_ROPE
#define GOBI_PORTRAIT 0x7F7 // ASSET_7F7_SPRITE_GOBI (gczoombox: 6 mouth frames)
#define GOBI_MOUTH_FRAMES 6
// Gobi converts to about 19 KiB with 48 skeleton entries (a matrix each, though his geo list
// draws them all with the root's); the rock to about 4.8 KiB with none; the rope to about
// 1.8 KiB with 11. Each poses in under 16 commands.
#define GOBI_BLOCK_MAX 0x5000
#define GOBI_MTX_MAX 50
#define ROCK_BLOCK_MAX 0x1400
#define ROCK_MTX_MAX 2
#define ROPE_BLOCK_MAX 0x800
#define ROPE_MTX_MAX 12
#define POSE_GFX_MAX 24
// The rock's model is built off its origin: its middle, in model units.
static const f32 rock_offset[3] = {-83.0f, 0.0f, 251.0f};
// The rope runs 182 model units along +z, 62 up.
#define ROPE_LENGTH (182.0f * AZTEC_BK_TO_DK64)

// Banjo-Kazooie sounds (sfx_e), as chGobi1, chGobiRock and chGobi2 play them.
#define GOBI_SFX_CRYING 0x84    // SFX_84_GOBI_CRYING: now and then, 32000/32767, 0.9
#define GOBI_SFX_PULLING 0x2C   // SFX_2C_PULLING_NOISE: tugging at the rope, 32000/32767, 0.9-1.1
#define GOBI_SFX_RUNNING 0x2E   // SFX_2E_BIGBUTT_RUNNING: his footfalls running off, 20000/32767
#define GOBI_SFX_GULPING 0x4B   // SFX_4B_GULPING: gulping before spitting, 28000/32767, 0.8
#define GOBI_SFX_SPLASH 0x12    // SFX_12_WATER_PADDLING_1: the water landing on Trunker
#define ROCK_SFX_BREAKING 0x9B  // SFX_9B_BOULDER_BREAKING_1: broken, at 0.3, 0.5, 0.7 and 0.9, 9000/32767
#define ROCK_SFX_HIT 0x1F       // SFX_1F_HITTING_AN_ENEMY_3: a hit that doesn't break it yet
static const u16 gobi_voice[] = {GOBI_SFX_CRYING};

// Where things are: Angry Aztec's first sand area (all of it flat sand at 110 to 120), clear of
// the Tiny Temple's bananas (2300-2550, 870-1075) and the alcove to the north (z 420-550). Gobi
// is tied up near the west side, the way in from the entrance tunnel; by Trunker he stands on
// the east side of the area, beside him (TRUNKER_X, TRUNKER_Z).
static const f32 gobi_stuck_spot[3] = {2290.0f, 115.0f, 690.0f};
static const f32 rock_spot[3] = {2215.0f, 120.0f, 750.0f};
static const f32 gobi_trunker_spot[3] = {2705.0f, 120.0f, 1115.0f};

// What he says (in his own sorry voice). Each page is one press of A.
static const char *const gobi_meet[] = {
    "BOO HOO! I'M GOBI, AND I'VE BEEN TIED TO THIS ROCK SINCE OUR TWO WORLDS GOT MUDDLED UP!",
    "I'VE TUGGED AND TUGGED BUT IT WON'T BUDGE. COULD YOU GIVE IT A GOOD BASH FOR ME?",
};
static const char *const gobi_reminder[] = {
    "SNIFF... THE ROCK, PLEASE! A FEW GOOD WHACKS SHOULD DO IT!",
};
static const char *const gobi_freed[] = {
    "I'M FREE! OH THANK YOU, THANK YOU! HAVE THIS, IT WAS STUCK UNDER THE ROCK.",
    "NOW I MUST DASH. MY FRIEND TRUNKER LOOKS SO THIRSTY, AND MY HUMP IS FULL OF WATER!",
};
static const char *const gobi_at_trunker[] = {
    "I'D GIVE TRUNKER A DRINK, BUT MY HUMP'S STUCK. GIVE IT A HARD SLAM FROM ABOVE AND OUT IT COMES!",
};
static const char *const gobi_watered[] = {
    "AH, THAT'S BETTER. LOOK HOW TALL TRUNKER IS NOW! THANKS, YOU TWO.",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

#define GOBI_SAVE_AT_TRUNKER_TALKED 0x8
#define GOBI_OWNER AZTEC_CHARACTER_GOBI

// Talking and hitting, in DK64 units: he talks when the player is this close, on the ground,
// and reminds them (standing by him, at most once a minute) after they've been this far off.
#define GOBI_TALK_RADIUS 85.0f
#define GOBI_REARM_RADIUS 250.0f
#define GOBI_TALK_HEIGHT 40.0f
#define GOBI_REMIND_FRAMES (60 * 30)
#define ROCK_RADIUS 26.0f
#define ROCK_HEIGHT 30.0f
#define ROCK_HITS 3
#define ROCK_HIT_COOLDOWN 0.6f // Seconds before another hit counts.
#define ROCK_SHAKE_SECONDS 0.4f
// Gobi's hump, for the slam.
#define HUMP_RADIUS 30.0f
// Running to Trunker, units per second, and his bob.
#define GOBI_RUN_SPEED 150.0f
#define GOBI_TURN_RATE 0.2f
#define REWARD_HEIGHT 12.5f
// The spit: seconds to each moment.
#define SPIT_GULP_1 0.8f
#define SPIT_GULP_2 1.4f
#define SPIT_GULP_3 2.0f
#define SPIT_REAR 2.4f
#define SPIT_SPLASH 2.8f
#define SPIT_END 3.4f
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
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z);
void jiggy_trunker_watered(void);

typedef enum {
    GOBI_UNLOADED,
    GOBI_READY,
    GOBI_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model didn't convert.
} GobiLoadState;

typedef enum {
    GOBI_STUCK,      // Tied to the rock.
    GOBI_THANKING,   // The rock just broke: his thanks.
    GOBI_RUNNING,    // Off to Trunker.
    GOBI_WAITING,    // By Trunker, his hump full.
    GOBI_SPITTING,   // Gulping, then spitting on Trunker.
    GOBI_RELAXING,   // Done.
} GobiAction;

static u8 gobi_block[GOBI_BLOCK_MAX] __attribute__((aligned(16)));
static u8 rock_block[ROCK_BLOCK_MAX] __attribute__((aligned(16)));
static u8 rope_block[ROPE_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx gobi_gfx[2][POSE_GFX_MAX];
static Mtx gobi_mtx[2][GOBI_MTX_MAX];
static Gfx rock_gfx[2][POSE_GFX_MAX];
static Mtx rock_mtx[2][ROCK_MTX_MAX];
static Gfx rope_gfx[2][POSE_GFX_MAX];
static Mtx rope_mtx[2][ROPE_MTX_MAX];
static JiggyBkPose gobi_pose;
static GobiLoadState gobi_state = GOBI_UNLOADED;

static s32 gobi_last_map = -1;
static u32 gobi_last_timer;
static s32 gobi_placed;
static u32 gobi_saved;
static GobiAction gobi_action;
static f32 gobi_pos[3];
static f32 gobi_yaw;
static f32 gobi_time;  // Seconds into the action.
static f32 gobi_cry_timer;
static s32 gobi_thanks_started;
static s32 gobi_armed;
static u32 gobi_last_talk;
static f32 rock_pos[3];
static s32 rock_hits;
static f32 rock_cooldown;
static f32 rock_shake;
static f32 reward_pos[3];
static u32 gobi_random_state = 0x474F4249;

static f32 gobi_random(void) {
    gobi_random_state = gobi_random_state * 1664525 + 1013904223;
    return (gobi_random_state >> 8) * (1.0f / 16777216.0f);
}

static void gobi_load(void) {
    gobi_state = GOBI_UNAVAILABLE;
    if (!jiggy_bk_available() || !aztec_model_load(GOBI_MODEL, gobi_block, sizeof(gobi_block)) ||
        !aztec_model_load(ROCK_MODEL, rock_block, sizeof(rock_block)) ||
        !aztec_model_load(ROPE_MODEL, rope_block, sizeof(rope_block))) {
        return;
    }
    aztec_pose_init(&gobi_pose);
    gobi_state = GOBI_READY;
}

static void gobi_save(u32 bits) {
    gobi_saved |= bits;
    jiggy_save_set(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_GOBI), gobi_saved);
}

static void gobi_talk(const char *const *pages, s32 count) {
    aztec_talk_start(GOBI_OWNER, GOBI_PORTRAIT, GOBI_MOUTH_FRAMES, "GOBI", pages, count, gobi_voice,
                     ARRAY_COUNT_OF(gobi_voice), 20000.0f / 32767.0f, 1.0f);
}

// Stands him where his save says he is.
static void gobi_place(Actor *player) {
    rock_pos[0] = rock_spot[0];
    rock_pos[1] = aztec_floor(rock_spot[0], rock_spot[1], rock_spot[2], FLOOR_PROBE, FLOOR_MAX_STEP);
    rock_pos[2] = rock_spot[2];
    reward_pos[0] = rock_pos[0];
    reward_pos[1] = rock_pos[1] + REWARD_HEIGHT;
    reward_pos[2] = rock_pos[2];
    rock_hits = 0;
    rock_cooldown = 0.0f;
    rock_shake = 0.0f;
    gobi_time = 0.0f;
    gobi_cry_timer = 2.0f + gobi_random() * 6.0f;
    if (!(gobi_saved & GOBI_SAVE_FREED)) {
        gobi_action = GOBI_STUCK;
        gobi_pos[0] = gobi_stuck_spot[0];
        gobi_pos[1] = aztec_floor(gobi_stuck_spot[0], gobi_stuck_spot[1], gobi_stuck_spot[2], FLOOR_PROBE, FLOOR_MAX_STEP);
        gobi_pos[2] = gobi_stuck_spot[2];
        // Facing away from the rock, pulling.
        gobi_yaw = func_global_asm_80665DE0(gobi_pos[0] * 2.0f - rock_pos[0], gobi_pos[2] * 2.0f - rock_pos[2], gobi_pos[0],
                                            gobi_pos[2]) & 0xFFF;
    } else {
        gobi_action = (gobi_saved & GOBI_SAVE_WATERED) ? GOBI_RELAXING : GOBI_WAITING;
        gobi_pos[0] = gobi_trunker_spot[0];
        gobi_pos[1] = aztec_floor(gobi_trunker_spot[0], gobi_trunker_spot[1], gobi_trunker_spot[2], FLOOR_PROBE, FLOOR_MAX_STEP);
        gobi_pos[2] = gobi_trunker_spot[2];
        gobi_yaw = func_global_asm_80665DE0(TRUNKER_X, TRUNKER_Z, gobi_pos[0], gobi_pos[2]) & 0xFFF;
    }
    gobi_armed = aztec_distance_xz(gobi_pos[0], gobi_pos[2], player->x_position, player->z_position) > GOBI_REARM_RADIUS;
    gobi_placed = TRUE;
}

static s32 gobi_player_near(Actor *player, f32 radius) {
    f32 dy = player->y_position - gobi_pos[1];

    return aztec_distance_xz(gobi_pos[0], gobi_pos[2], player->x_position, player->z_position) <= radius &&
           dy <= GOBI_TALK_HEIGHT && dy >= -GOBI_TALK_HEIGHT;
}

// His talks: the first one when the player comes near, then a reminder when they stand by him
// (Bottles' rule: after they've been away, and at most once a minute).
static void gobi_maybe_talk(Actor *player) {
    s32 grounded = player->y_position - player->floor <= AZTEC_AIRBORNE_HEIGHT;
    s32 first;
    const char *const *pages;
    s32 count;

    if (aztec_talk_owner() >= 0 || aztec_player_locked()) {
        return;
    }
    if (!gobi_armed) {
        gobi_armed = !gobi_player_near(player, GOBI_REARM_RADIUS) && object_timer - gobi_last_talk >= GOBI_REMIND_FRAMES;
        return;
    }
    if (!grounded || !gobi_player_near(player, GOBI_TALK_RADIUS)) {
        return;
    }
    switch (gobi_action) {
        case GOBI_STUCK:
            first = !(gobi_saved & GOBI_SAVE_TALKED);
            pages = first ? gobi_meet : gobi_reminder;
            count = first ? (s32)ARRAY_COUNT_OF(gobi_meet) : (s32)ARRAY_COUNT_OF(gobi_reminder);
            break;
        case GOBI_WAITING:
            first = !(gobi_saved & GOBI_SAVE_AT_TRUNKER_TALKED);
            pages = gobi_at_trunker;
            count = ARRAY_COUNT_OF(gobi_at_trunker);
            break;
        case GOBI_RELAXING:
            first = FALSE;
            pages = gobi_watered;
            count = ARRAY_COUNT_OF(gobi_watered);
            break;
        default:
            return;
    }
    // A repeat needs the player to stop by him (standing, not walking or fighting past).
    if (!first && player->control_state != 0x0C) {
        return;
    }
    gobi_armed = FALSE;
    gobi_last_talk = object_timer;
    if (gobi_action == GOBI_STUCK) {
        gobi_save(GOBI_SAVE_TALKED);
    } else if (gobi_action == GOBI_WAITING) {
        gobi_save(GOBI_SAVE_AT_TRUNKER_TALKED);
    }
    gobi_talk(pages, count);
}

// The rock: hit by the player's attacks, Simian Slam or shots; broken on the third.
static void rock_update(Actor *player, f32 seconds) {
    f32 middle[3];

    if (rock_cooldown > 0.0f) {
        rock_cooldown -= seconds;
    }
    if (rock_shake > 0.0f) {
        rock_shake -= seconds;
    }
    middle[0] = rock_pos[0];
    middle[1] = rock_pos[1];
    middle[2] = rock_pos[2];
    if (rock_cooldown > 0.0f || !(aztec_player_attacks(player, middle, ROCK_RADIUS) ||
                                  aztec_player_shot(player, middle, ROCK_RADIUS, ROCK_HEIGHT))) {
        return;
    }
    rock_hits++;
    rock_cooldown = ROCK_HIT_COOLDOWN;
    rock_shake = ROCK_SHAKE_SECONDS;
    if (rock_hits < ROCK_HITS) {
        jiggy_bk_sfx_play(ROCK_SFX_HIT, 24000.0f / 32767.0f, 0.8f + 0.1f * rock_hits);
        jiggy_bk_sfx_play(GOBI_SFX_CRYING, 20000.0f / 32767.0f, 1.2f);
        return;
    }
    // Broken (chGobiRock's state 2).
    jiggy_bk_sfx_play(ROCK_SFX_BREAKING, 9000.0f / 32767.0f * 3.0f, 0.3f);
    jiggy_bk_sfx_play(ROCK_SFX_BREAKING, 9000.0f / 32767.0f * 3.0f, 0.5f);
    jiggy_bk_sfx_play(ROCK_SFX_BREAKING, 9000.0f / 32767.0f * 3.0f, 0.7f);
    jiggy_bk_sfx_play(ROCK_SFX_BREAKING, 9000.0f / 32767.0f * 3.0f, 0.9f);
    jiggy_bk_jingle_play(AZTEC_JINGLE_PUZZLE_SOLVED, 28000.0f / 32767.0f);
    gobi_save(GOBI_SAVE_FREED | GOBI_SAVE_TALKED);
    gobi_action = GOBI_THANKING;
    gobi_time = 0.0f;
    gobi_thanks_started = FALSE;
    aztec_lock_player(TRUE);
}

// A frame of Gobi, `frames` game frames on.
static void gobi_update(Actor *player, u32 frames) {
    f32 seconds = frames / AZTEC_GAME_FPS;
    f32 before = gobi_time;
    s32 talking = aztec_talk_owner() == GOBI_OWNER;
    s32 finished = FALSE;

    if (talking) {
        finished = aztec_talk_update(GOBI_OWNER);
    }
    if (frames == 0) {
        return;
    }
    gobi_time += seconds;
    // Crying now and then while he's stuck or waiting (chGobi1's timer), when the player's near.
    if (gobi_action == GOBI_STUCK || gobi_action == GOBI_WAITING) {
        gobi_cry_timer -= seconds;
        if (gobi_cry_timer <= 0.0f) {
            gobi_cry_timer = 4.0f + gobi_random() * 8.0f;
            if (!talking) {
                aztec_sfx_at(player, gobi_pos[0], gobi_pos[2], GOBI_SFX_CRYING, 0.9f, 0.9f + gobi_random() * 0.2f);
            }
        }
    }
    switch (gobi_action) {
        case GOBI_STUCK: {
            // Tugging: a pull every couple of seconds.
            f32 cycle = gobi_time - (s32)(gobi_time / 2.2f) * 2.2f;
            f32 cycle_before = before - (s32)(before / 2.2f) * 2.2f;

            if (cycle < cycle_before) {
                aztec_sfx_at(player, gobi_pos[0], gobi_pos[2], GOBI_SFX_PULLING, 0.6f, 0.9f + gobi_random() * 0.2f);
            }
            if (!talking) {
                rock_update(player, seconds);
                gobi_maybe_talk(player);
            }
            break;
        }
        case GOBI_THANKING:
            aztec_turn_toward(&gobi_yaw, player->x_position, player->z_position, gobi_pos[0], gobi_pos[2], GOBI_TURN_RATE, frames);
            // A moment for the rock to go, then his thanks.
            if (!gobi_thanks_started && gobi_time > 1.0f && aztec_talk_owner() < 0) {
                gobi_thanks_started = TRUE;
                gobi_talk(gobi_freed, ARRAY_COUNT_OF(gobi_freed));
            }
            if (finished) {
                jiggy_bk_jingle_play(AZTEC_JINGLE_JIGGY_SPAWN, 1.0f);
                gobi_action = GOBI_RUNNING;
                gobi_time = 0.0f;
            }
            break;
        case GOBI_RUNNING: {
            f32 step = GOBI_RUN_SPEED * seconds;
            f32 distance = aztec_distance_xz(gobi_trunker_spot[0], gobi_trunker_spot[2], gobi_pos[0], gobi_pos[2]);
            f32 stride = gobi_time - (s32)(gobi_time / 0.35f) * 0.35f;
            f32 stride_before = before - (s32)(before / 0.35f) * 0.35f;

            aztec_turn_toward(&gobi_yaw, gobi_trunker_spot[0], gobi_trunker_spot[2], gobi_pos[0], gobi_pos[2], GOBI_TURN_RATE,
                              frames);
            if (stride < stride_before) {
                aztec_sfx_at(player, gobi_pos[0], gobi_pos[2], GOBI_SFX_RUNNING, 20000.0f / 32767.0f, 1.0f);
            }
            if (distance <= step) {
                gobi_pos[0] = gobi_trunker_spot[0];
                gobi_pos[2] = gobi_trunker_spot[2];
                gobi_action = GOBI_WAITING;
                gobi_time = 0.0f;
                gobi_armed = TRUE;
            } else {
                gobi_pos[0] += (gobi_trunker_spot[0] - gobi_pos[0]) * step / distance;
                gobi_pos[2] += (gobi_trunker_spot[2] - gobi_pos[2]) * step / distance;
            }
            gobi_pos[1] = aztec_floor(gobi_pos[0], gobi_pos[1], gobi_pos[2], FLOOR_PROBE, FLOOR_MAX_STEP);
            break;
        }
        case GOBI_WAITING: {
            f32 hump[3];

            aztec_turn_toward(&gobi_yaw, TRUNKER_X, TRUNKER_Z, gobi_pos[0], gobi_pos[2], GOBI_TURN_RATE, frames);
            hump[0] = gobi_pos[0];
            hump[1] = gobi_pos[1] + 20.0f;
            hump[2] = gobi_pos[2];
            if (!talking && aztec_player_slams(player, hump, HUMP_RADIUS)) {
                // chGobi2's state 3: a cry, three gulps, then the spit.
                aztec_lock_player(TRUE);
                jiggy_bk_sfx_play(GOBI_SFX_CRYING, 1.0f, 1.1f);
                gobi_action = GOBI_SPITTING;
                gobi_time = 0.0f;
                break;
            }
            if (!talking) {
                gobi_maybe_talk(player);
            }
            break;
        }
        case GOBI_SPITTING:
            aztec_lock_player(TRUE);
            aztec_turn_toward(&gobi_yaw, TRUNKER_X, TRUNKER_Z, gobi_pos[0], gobi_pos[2], GOBI_TURN_RATE, frames);
            if ((before < SPIT_GULP_1 && gobi_time >= SPIT_GULP_1) || (before < SPIT_GULP_2 && gobi_time >= SPIT_GULP_2) ||
                (before < SPIT_GULP_3 && gobi_time >= SPIT_GULP_3)) {
                jiggy_bk_sfx_play(GOBI_SFX_GULPING, 28000.0f / 32767.0f, 0.8f);
            }
            if (before < SPIT_SPLASH && gobi_time >= SPIT_SPLASH) {
                jiggy_bk_sfx_play(GOBI_SFX_SPLASH, 1.0f, 0.7f);
                jiggy_bk_sfx_play(GOBI_SFX_SPLASH, 1.0f, 0.9f);
            }
            if (gobi_time >= SPIT_END) {
                gobi_save(GOBI_SAVE_WATERED);
                gobi_action = GOBI_RELAXING;
                gobi_time = 0.0f;
                gobi_armed = FALSE;
                gobi_last_talk = object_timer;
                // Trunker takes the player from here: he grows, then thanks them.
                jiggy_trunker_watered();
            }
            break;
        case GOBI_RELAXING:
            if (!talking) {
                gobi_maybe_talk(player);
            }
            break;
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// Gobi, his rock and rope once a frame, with the player.
Gfx *jiggy_gobi_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    f32 pitch = 0.0f;
    f32 lift = 0.0f;
    f32 pos[3];

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over).
    if ((s32)current_map != gobi_last_map || object_timer < gobi_last_timer) {
        if (aztec_talk_owner() == GOBI_OWNER) {
            aztec_talk_cancel();
        }
        if (gobi_action == GOBI_THANKING || gobi_action == GOBI_SPITTING) {
            aztec_lock_player(FALSE);
        }
        gobi_last_map = current_map;
        gobi_placed = FALSE;
        // Read back on every map, which also catches a different save file being loaded.
        gobi_saved = current_map == MAP_AZTEC ? jiggy_save_get(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_GOBI)) : 0;
    }
    if (current_map != MAP_AZTEC) {
        gobi_last_timer = object_timer;
        return dl;
    }
    if (gobi_state == GOBI_UNLOADED) {
        gobi_load();
    }
    if (gobi_state != GOBI_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!gobi_placed) {
        gobi_last_timer = object_timer;
        if (actor->y_position - actor->floor > AZTEC_AIRBORNE_HEIGHT) {
            return dl;
        }
        gobi_place(actor);
    }
    frames = aztec_frames(&gobi_last_timer);
    // He waits while someone else's cutscene holds the player.
    if (aztec_player_locked() && aztec_talk_owner() != GOBI_OWNER && gobi_action != GOBI_THANKING &&
        gobi_action != GOBI_SPITTING) {
        frames = 0;
    }
    gobi_update(actor, frames);

    // His Golden Banana, from the moment the rock breaks until it's collected.
    if ((gobi_saved & GOBI_SAVE_FREED) && gobi_action != GOBI_THANKING) {
        jiggy_reward_update(AZTEC_REWARD_GOBI, reward_pos[0], reward_pos[1], reward_pos[2], actor, frames);
    }

    // How he moves as a whole (see the top of this file).
    switch (gobi_action) {
        case GOBI_STUCK: {
            // Leaning back on the rope, harder at each pull.
            f32 cycle = gobi_time - (s32)(gobi_time / 2.2f) * 2.2f;

            pitch = cycle < 0.6f ? -9.0f * (cycle / 0.6f) : -9.0f * (1.0f - (cycle - 0.6f) / 1.6f);
            break;
        }
        case GOBI_RUNNING:
            lift = 3.0f * __builtin_fabsf(func_global_asm_80612794((s16)(gobi_time * 4096.0f / 0.35f)));
            pitch = 4.0f;
            break;
        case GOBI_SPITTING:
            if (gobi_time < SPIT_REAR) {
                // Gulping: little bobs.
                lift = 1.5f * __builtin_fabsf(func_global_asm_80612794((s16)(gobi_time * 4096.0f / 0.6f)));
            } else if (gobi_time < SPIT_SPLASH) {
                pitch = -12.0f * (gobi_time - SPIT_REAR) / (SPIT_SPLASH - SPIT_REAR);
            } else {
                pitch = 10.0f * (1.0f - (gobi_time - SPIT_SPLASH) / (SPIT_END - SPIT_SPLASH));
            }
            break;
        case GOBI_RELAXING:
        case GOBI_WAITING:
            // Breathing.
            lift = 0.6f * func_global_asm_80612794((s16)(gobi_time * 4096.0f / 3.0f));
            break;
        default:
            break;
    }
    pos[0] = gobi_pos[0];
    pos[1] = gobi_pos[1] + lift;
    pos[2] = gobi_pos[2];
    dl = aztec_model_draw(dl, &gobi_pose, GOBI_MODEL, gobi_block, pos, gobi_yaw, 1.0f, 1.0f, pitch, NULL, gobi_gfx[buffer],
                          POSE_GFX_MAX, gobi_mtx[buffer], GOBI_MTX_MAX);
    if (gobi_action == GOBI_STUCK) {
        f32 shake = rock_shake > 0.0f ? 3.0f * func_global_asm_80612794((s16)(rock_shake * 4096.0f * 8.0f)) : 0.0f;
        f32 rope_yaw = func_global_asm_80665DE0(gobi_pos[0], gobi_pos[2], rock_pos[0], rock_pos[2]) & 0xFFF;
        f32 length = aztec_distance_xz(gobi_pos[0], gobi_pos[2], rock_pos[0], rock_pos[2]);

        pos[0] = rock_pos[0] + shake;
        pos[1] = rock_pos[1];
        pos[2] = rock_pos[2];
        dl = aztec_model_draw(dl, &gobi_pose, ROCK_MODEL, rock_block, pos, 0.0f, 1.0f, 1.0f, 0.0f, rock_offset,
                              rock_gfx[buffer], POSE_GFX_MAX, rock_mtx[buffer], ROCK_MTX_MAX);
        // The rope, from the rock to his neck, stretched to reach.
        pos[0] = rock_pos[0];
        pos[1] = rock_pos[1] + 2.0f;
        pos[2] = rock_pos[2];
        if (length > 1.0f) {
            dl = aztec_model_draw(dl, &gobi_pose, ROPE_MODEL, rope_block, pos, rope_yaw, length / ROPE_LENGTH, 0.25f, 0.0f,
                                  NULL, rope_gfx[buffer], POSE_GFX_MAX, rope_mtx[buffer], ROPE_MTX_MAX);
        }
    }
    return dl;
}
