// The Ancient Ones: Gobi's Valley's five buried stone guardians, read from the player's
// Banjo-Kazooie ROM, in Angry Aztec's main area, west of the quicksand pit and east of the
// Llama Temple, where the two worlds have collided; with their puzzle.
//
// As in Gobi's Valley (chAncientOne, src/GV/ch/ancientone.c), five Ancient Ones
// (ASSET_3E8_MODEL_ANCIENT_ONE) are buried in a ring; one stands up out of the sand to begin
// with. The first time the player comes near it, it explains in a text box with its portrait
// (ASSET_817_SPRITE_ANCIENT_ONE) and voice (SFX_10D_ANCIENT_ONE_TALKING, as gczoombox plays
// it). Touching an Ancient One that's up (walking into it) rings Banjo-Kazooie's ding
// (COMUSIC_2B_DING_B) and the next one rises somewhere else in the ring, shaking as it comes
// up (18 units a frame, jittering, as chAncientOne rises) with a rumble. When the fifth is
// touched they thank the player, Banjo-Kazooie's puzzle solved fanfare plays, they all sink
// back (10 units a frame) and a Golden Banana (drawn as a Jiggy) appears in the middle of the
// ring (jiggy_rewards.c, reward 0x23), with Banjo-Kazooie's Jiggy jingle.
//
// Gobi's Valley's models move their vertices themselves (jiggy_aztec.h); the Ancient Ones only
// ever rise and sink, so they're drawn at rest.
//
// The first talk and the solved puzzle are kept with the save file (AZTEC_SAVE_KEY, character
// 3); a puzzle left half way starts again on the next visit. Drawn right after the player's own
// model (func_global_asm_80630DCC in patches_draw.c); the text box is jiggy_aztec.c's. Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "jiggy_aztec.h"

#define ANCIENT_MODEL 0x3E8    // ASSET_3E8_MODEL_ANCIENT_ONE
#define ANCIENT_PORTRAIT 0x817 // ASSET_817_SPRITE_ANCIENT_ONE (gczoombox: 6 mouth frames)
#define ANCIENT_MOUTH_FRAMES 6
// The Ancient One converts to about 18 KiB with 4 skeleton entries, posed in under 16 commands.
#define ANCIENT_BLOCK_MAX 0x4C00
#define ANCIENT_MTX_MAX 6
#define ANCIENT_GFX_MAX 24
#define ANCIENT_COUNT 5
#define ANCIENT_SFX_TALKING 0x10D // SFX_10D_ANCIENT_ONE_TALKING (gczoombox: 28000/32767, 1.0)
static const u16 ancient_voice[] = {ANCIENT_SFX_TALKING};

// The ring: flat sand (floor 112 to 120) west of the quicksand pit, clear of the Llama Temple's
// doors (x 2125-2231) and the bananas along its east wall (x 2379-2545).
static const f32 ring_middle[3] = {2620.0f, 114.0f, 3900.0f};
static const f32 ancient_spots[ANCIENT_COUNT][3] = {
    {2620.0f, 120.0f, 4040.0f},
    {2753.0f, 120.0f, 3943.0f},
    {2702.0f, 115.0f, 3787.0f},
    {2538.0f, 117.0f, 3787.0f},
    {2487.0f, 113.0f, 3943.0f},
};
// The order they rise in: back and forth across the ring.
static const u8 ancient_order[ANCIENT_COUNT] = {0, 2, 4, 1, 3};

// Sizes and speeds, in DK64 units. They're drawn smaller than in Banjo-Kazooie (their model is
// 1072 units tall) so they stand about as tall as Candy's shop.
#define ANCIENT_SCALE 0.5f
#define ANCIENT_HEIGHT (1072.0f * AZTEC_BK_TO_DK64 * ANCIENT_SCALE)
#define ANCIENT_RISE_SPEED (18.0f * AZTEC_BK_TO_DK64 * ANCIENT_SCALE * 30.0f)
#define ANCIENT_SINK_SPEED (10.0f * AZTEC_BK_TO_DK64 * ANCIENT_SCALE * 30.0f)
#define ANCIENT_JITTER 2.5f
#define ANCIENT_TOUCH_RADIUS 45.0f // Its own half width (about 35) and a little more.
#define ANCIENT_TALK_RADIUS 110.0f
#define ANCIENT_TALK_HEIGHT 50.0f
#define REWARD_HEIGHT 12.5f
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f

static const char *const ancient_meet[] = {
    "WHO DISTURBS THE ANCIENT ONES? ...AH. TRAVELLERS FROM TWO WORLDS. WE HAVE SLEPT UNDER THIS SAND FOR AGES.",
    "TOUCH EACH OF US AS WE RISE, AND THE ANCIENT ONES SHALL REWARD YOU.",
};
static const char *const ancient_done[] = {
    "ALL FIVE OF US AWAKE AT LAST! TAKE THIS TREASURE, WITH THE THANKS OF THE ANCIENT ONES.",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

#define ANCIENT_SAVE_TALKED 0x1
#define ANCIENT_SAVE_DONE 0x2
#define ANCIENT_OWNER AZTEC_CHARACTER_ANCIENT_ONES

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.

typedef enum {
    ANCIENT_UNLOADED,
    ANCIENT_READY,
    ANCIENT_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} AncientLoadState;

typedef enum {
    PUZZLE_WAITING,  // The first is up, the rest buried.
    PUZZLE_THANKING, // All five touched: their thanks.
    PUZZLE_SINKING,
    PUZZLE_DONE,
} PuzzleAction;

typedef struct {
    f32 pos[3];
    f32 depth;  // How far under the sand: 0 up, ANCIENT_HEIGHT buried.
    s32 rising;
    s32 touched;
} AncientOne;

static u8 ancient_block[ANCIENT_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx ancient_gfx[2][ANCIENT_COUNT][ANCIENT_GFX_MAX];
static Mtx ancient_mtx[2][ANCIENT_COUNT][ANCIENT_MTX_MAX];
static JiggyBkPose ancient_pose;
static AncientLoadState ancient_state = ANCIENT_UNLOADED;

static s32 ancient_last_map = -1;
static u32 ancient_last_timer;
static s32 ancient_placed;
static u32 ancient_saved;
static PuzzleAction puzzle_action;
static s32 puzzle_next; // How many have been touched.
static s32 puzzle_rumble;
static AncientOne ancients[ANCIENT_COUNT];
static f32 reward_pos[3];

static void ancient_load(void) {
    ancient_state = ANCIENT_UNAVAILABLE;
    if (!jiggy_bk_available() || !aztec_model_load(ANCIENT_MODEL, ancient_block, sizeof(ancient_block))) {
        return;
    }
    aztec_pose_init(&ancient_pose);
    ancient_state = ANCIENT_READY;
}

static void ancient_save(u32 bits) {
    ancient_saved |= bits;
    jiggy_save_set(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_ANCIENT_ONES), ancient_saved);
}

static void ancient_talk(const char *const *pages, s32 count) {
    aztec_talk_start(ANCIENT_OWNER, ANCIENT_PORTRAIT, ANCIENT_MOUTH_FRAMES, "ANCIENT ONE", pages, count, ancient_voice,
                     ARRAY_COUNT_OF(ancient_voice), 28000.0f / 32767.0f, 1.0f);
}

static void ancient_stop_rumble(void) {
    if (puzzle_rumble > 0) {
        jiggy_bk_sound_stop(puzzle_rumble);
        puzzle_rumble = 0;
    }
}

static void ancient_place(void) {
    s32 i;

    for (i = 0; i < ANCIENT_COUNT; i++) {
        AncientOne *a = &ancients[i];

        a->pos[0] = ancient_spots[i][0];
        a->pos[1] = aztec_floor(ancient_spots[i][0], ancient_spots[i][1], ancient_spots[i][2], FLOOR_PROBE, FLOOR_MAX_STEP);
        a->pos[2] = ancient_spots[i][2];
        a->depth = ANCIENT_HEIGHT;
        a->rising = FALSE;
        a->touched = FALSE;
    }
    reward_pos[0] = ring_middle[0];
    reward_pos[1] = aztec_floor(ring_middle[0], ring_middle[1], ring_middle[2], FLOOR_PROBE, FLOOR_MAX_STEP) + REWARD_HEIGHT;
    reward_pos[2] = ring_middle[2];
    puzzle_next = 0;
    if (ancient_saved & ANCIENT_SAVE_DONE) {
        puzzle_action = PUZZLE_DONE;
    } else {
        puzzle_action = PUZZLE_WAITING;
        // The first one is up to begin with.
        ancients[ancient_order[0]].depth = 0.0f;
    }
    ancient_placed = TRUE;
}

static s32 ancient_player_near(AncientOne *a, Actor *player, f32 radius) {
    f32 dy = player->y_position - a->pos[1];

    return aztec_distance_xz(a->pos[0], a->pos[2], player->x_position, player->z_position) <= radius &&
           dy <= ANCIENT_TALK_HEIGHT && dy >= -ANCIENT_TALK_HEIGHT;
}

static void ancient_update(Actor *player, u32 frames) {
    f32 seconds = frames / AZTEC_GAME_FPS;
    s32 talking = aztec_talk_owner() == ANCIENT_OWNER;
    s32 finished = FALSE;
    s32 i;

    if (talking) {
        finished = aztec_talk_update(ANCIENT_OWNER);
    }
    if (frames == 0) {
        return;
    }
    // Rising (chAncientOne's state 1 before it's up).
    for (i = 0; i < ANCIENT_COUNT; i++) {
        AncientOne *a = &ancients[i];

        if (a->rising) {
            a->depth -= ANCIENT_RISE_SPEED * seconds;
            if (a->depth <= 0.0f) {
                a->depth = 0.0f;
                a->rising = FALSE;
                ancient_stop_rumble();
            }
        }
    }
    switch (puzzle_action) {
        case PUZZLE_WAITING: {
            AncientOne *current = &ancients[ancient_order[puzzle_next]];

            if (talking || current->rising || aztec_player_locked()) {
                break;
            }
            // The first time near the first one: it explains.
            if (!(ancient_saved & ANCIENT_SAVE_TALKED) && ancient_player_near(current, player, ANCIENT_TALK_RADIUS) &&
                player->y_position - player->floor <= AZTEC_AIRBORNE_HEIGHT) {
                ancient_save(ANCIENT_SAVE_TALKED);
                ancient_talk(ancient_meet, ARRAY_COUNT_OF(ancient_meet));
                break;
            }
            if (!ancient_player_near(current, player, ANCIENT_TOUCH_RADIUS)) {
                break;
            }
            current->touched = TRUE;
            puzzle_next++;
            if (puzzle_next >= ANCIENT_COUNT) {
                jiggy_bk_jingle_play(AZTEC_JINGLE_PUZZLE_SOLVED, 1.0f);
                ancient_save(ANCIENT_SAVE_DONE | ANCIENT_SAVE_TALKED);
                puzzle_action = PUZZLE_THANKING;
                ancient_talk(ancient_done, ARRAY_COUNT_OF(ancient_done));
                break;
            }
            jiggy_bk_jingle_play(AZTEC_JINGLE_DING, 28000.0f / 32767.0f);
            ancients[ancient_order[puzzle_next]].rising = TRUE;
            ancient_stop_rumble();
            puzzle_rumble = jiggy_bk_sfx_play(AZTEC_SFX_SAND_RUMBLE, 0.6f, 0.9f);
            break;
        }
        case PUZZLE_THANKING:
            if (finished) {
                puzzle_action = PUZZLE_SINKING;
                puzzle_rumble = jiggy_bk_sfx_play(AZTEC_SFX_SAND_RUMBLE, 0.6f, 0.7f);
            }
            break;
        case PUZZLE_SINKING: {
            s32 all_down = TRUE;

            for (i = 0; i < ANCIENT_COUNT; i++) {
                ancients[i].depth += ANCIENT_SINK_SPEED * seconds;
                if (ancients[i].depth < ANCIENT_HEIGHT) {
                    all_down = FALSE;
                } else {
                    ancients[i].depth = ANCIENT_HEIGHT;
                }
            }
            if (all_down) {
                ancient_stop_rumble();
                jiggy_bk_jingle_play(AZTEC_JINGLE_JIGGY_SPAWN, 1.0f);
                puzzle_action = PUZZLE_DONE;
            }
            break;
        }
        default:
            break;
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// the Ancient Ones once a frame, with the player.
Gfx *jiggy_ancient_ones_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    s32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != ancient_last_map || object_timer < ancient_last_timer) {
        if (aztec_talk_owner() == ANCIENT_OWNER) {
            aztec_talk_cancel();
        }
        ancient_stop_rumble();
        ancient_last_map = current_map;
        ancient_placed = FALSE;
        ancient_saved = current_map == MAP_AZTEC ? jiggy_save_get(current_file, AZTEC_SAVE_KEY(AZTEC_CHARACTER_ANCIENT_ONES)) : 0;
    }
    if (current_map != MAP_AZTEC) {
        ancient_last_timer = object_timer;
        return dl;
    }
    if (ancient_state == ANCIENT_UNLOADED) {
        ancient_load();
    }
    if (ancient_state != ANCIENT_READY) {
        return dl;
    }
    if (!ancient_placed) {
        ancient_last_timer = object_timer;
        if (actor->y_position - actor->floor > AZTEC_AIRBORNE_HEIGHT) {
            return dl;
        }
        ancient_place();
    }
    frames = aztec_frames(&ancient_last_timer);
    if (aztec_player_locked() && aztec_talk_owner() != ANCIENT_OWNER && puzzle_action != PUZZLE_SINKING) {
        frames = 0;
    }
    ancient_update(actor, frames);

    if (puzzle_action == PUZZLE_DONE) {
        jiggy_reward_update(AZTEC_REWARD_ANCIENT_ONES, reward_pos[0], reward_pos[1], reward_pos[2], actor, frames);
        return dl;
    }
    for (i = 0; i < ANCIENT_COUNT; i++) {
        AncientOne *a = &ancients[i];
        f32 pos[3];

        if (a->depth >= ANCIENT_HEIGHT) {
            continue;
        }
        pos[0] = a->pos[0];
        pos[1] = a->pos[1] - a->depth;
        pos[2] = a->pos[2];
        // Shaking as it comes up (chAncientOne's jitter).
        if (a->rising) {
            pos[0] += (object_timer & 1) ? ANCIENT_JITTER : -ANCIENT_JITTER;
            pos[2] += (object_timer & 2) ? ANCIENT_JITTER * 0.5f : -ANCIENT_JITTER * 0.5f;
        }
        // Facing the middle of the ring.
        dl = aztec_model_draw(dl, &ancient_pose, ANCIENT_MODEL, ancient_block, pos,
                              func_global_asm_80665DE0(ring_middle[0], ring_middle[2], a->pos[0], a->pos[2]) & 0xFFF,
                              ANCIENT_SCALE, 1.0f, 0.0f, NULL, ancient_gfx[buffer][i], ANCIENT_GFX_MAX,
                              ancient_mtx[buffer][i], ANCIENT_MTX_MAX);
    }
    return dl;
}
