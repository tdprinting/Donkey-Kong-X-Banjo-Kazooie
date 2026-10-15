// Brentilda: Gruntilda's kinder sister, read from the player's Banjo-Kazooie ROM, in DK Isles,
// where Gruntilda's Lair has moved in. As in Banjo-Kazooie's lair (chBrentilda,
// src/lair/ch/brentilda.c), she tells Banjo and the Kong her sister's secrets, tops up their
// health, and the secrets are what Grunty's quiz asks about; here she runs the quiz herself.
//
// Her quest, kept with the save file (key 0x57000000 | map, BRENT_SAVE_* below):
// 1. On the main island's west beach she introduces herself, tells the first secret and says
//    where to find her next; then she twirls and vanishes (her talking animation spun and
//    shrunk away, with Banjo-Kazooie's pop and her vanishing jingle, COMUSIC_8B).
// 2. On Krem Isle's north shore she tells the second secret and vanishes again.
// 3. On K. Lumsy's island she tells the third, then asks three questions about them (picked with
//    the stick or the D-pad and A, in jiggy_talk.c's question box). A wrong answer gets the
//    buzzer and a try again later (she reminds the player of her secrets first); three right
//    answers get a Jiggy (reward 0x01, jiggy_rewards.c) in front of her, with Banjo-Kazooie's
//    Jiggy jingle.
// Whenever she's talked to with the player hurt, she heals them to full, as chBrentilda's
// healDuoToFull does (DK64's own refill, func_global_asm_80709464). She appears, with her
// "activating" jingle (COMUSIC_81), when the player first comes near her on a visit.
//
// The model (ASSET_539_MODEL_BRENTILDA) idles with ASSET_26B and talks with ASSET_26C, as
// chBrentildaAnimations has them, turning to face the player. Drawn right after the player's
// own model (func_global_asm_80630DCC in patches_draw.c); her text box is jiggy_talk.c's.
// Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "jiggy.h"
#include "jiggy_talk.h"

#define BRENT_MODEL 0x539    // ASSET_539_MODEL_BRENTILDA
#define BRENT_PORTRAIT 0x82E // ASSET_82E_SPRITE_BRENTILDA
// She converts to 39352 bytes, with 34 bones and 47 pose commands.
#define BRENT_BLOCK_MAX 0x9A00
#define BRENT_GFX_MAX 80
#define BRENT_MTX_MAX 36

// Banjo-Kazooie animations (chBrentildaAnimations), 4 seconds each.
#define BRENT_ANIM_IDLE 0x26B
#define BRENT_ANIM_TALK 0x26C
#define BRENT_ANIM_SECONDS 4.0f
// Vanishing and appearing, in seconds.
#define BRENT_VANISH_SECONDS 1.2f
#define BRENT_APPEAR_SECONDS 0.6f
// Turns while vanishing (12-bit angle units a second).
#define BRENT_VANISH_SPIN 8192.0f

// Banjo-Kazooie sounds and music.
#define BRENT_SFX_TALK 0x14C   // SFX_14C_BRENTILDA_TALKING
#define BRENT_SFX_POP 0xC5     // SFX_C5_TWINKLY_POP, as she vanishes
#define JINGLE_ACTIVATE 0x81   // COMUSIC_81_ACTIVATING_BRENTILDA
#define JINGLE_DEACTIVATE 0x8B // COMUSIC_8B_DEACTIVATE_BRENTILDA
#define JINGLE_RIGHT 0x2B      // COMUSIC_2B_DING_B
#define JINGLE_WRONG 0x2C      // COMUSIC_2C_BUZZER
#define JINGLE_JIGGY_SPAWN 0x3D // COMUSIC_3D_JIGGY_SPAWN

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
#define GAME_FPS 30.0f
#define BRENT_TURN_RATE 0.2f
#define BRENT_LOOK_RADIUS 250.0f
// She appears when the player comes this close, across and up or down (chBrentilda fades in
// within 500 units; a little further here, so she's seen appearing).
#define BRENT_APPEAR_RADIUS 250.0f
#define BRENT_APPEAR_HEIGHT 150.0f
// Talking (as Bottles): this close, on the ground; not again until the player has walked this
// far off; a reminder at most once a minute.
#define BRENT_REMIND_FRAMES (60 * 30)
#define BRENT_TALK_RADIUS 70.0f
#define BRENT_REARM_RADIUS 220.0f
#define BRENT_TALK_FEET 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
#define BRENT_FLOOR_PROBE 40.0f
#define BRENT_FLOOR_MAX_STEP 60.0f
#define REWARD_HEIGHT 12.5f
#define REWARD_ID 0x01

// DK64 player states (Actor control_state) she talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
#define CUTSCENE_LOCK 0x10

// Save key and bits: her stage (which spot she's at, 3 once the quiz is won) and whether the
// quiz has been asked.
#define BRENT_SAVE_MAP_KEY(map) (0x57000000 | (u32)(map))
#define BRENT_SAVE_STAGE 0x3
#define BRENT_SAVE_QUIZ_ASKED 0x4
#define BRENT_STAGE_DONE 3

// Where she stands at each stage, map by map (DK Isles' floor is about y 500 at all three, on
// sand clear of DK64's own objects, the note trails and the other collectibles), and where
// her Jiggy appears in front of the last spot.
typedef struct {
    f32 x, y, z;
} BrentSpot;
typedef struct {
    Maps map;
    BrentSpot spots[3];
    BrentSpot reward;
} BrentPlacement;
static const BrentPlacement brent_maps[] = {
    {MAP_DK_ISLES_OVERWORLD,
     {
         {2146.0f, 500.0f, 1695.0f}, // The main island's west beach, south of the Training Grounds' tunnel.
         {2380.0f, 500.0f, 3180.0f}, // Krem Isle's north shore, facing the main island.
         {3420.0f, 500.0f, 3200.0f}, // K. Lumsy's island, on the rim north of his cage.
     },
     {3380.0f, 500.0f, 3150.0f}},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// What she says.
static const char *const brent_meet[] = {
    "OH, HELLO, YOU TWO! I'M BRENTILDA, GRUNTY'S MUCH NICER SISTER. DON'T WORRY, I'M ON YOUR SIDE!",
    "MY SISTER HAS SQUASHED HER LAIR INTO THESE ISLES. I'LL TELL YOU HER SECRETS, ONE AT A TIME.",
    "SECRET NUMBER ONE: GRUNTY'S FAVOURITE SNACK IS A BOWL OF SLUGS AND SNAILS!",
    "NOW FIND ME AGAIN ON THE SHORE OF KREM ISLE. TOODLE-OO!",
};
static const char *const brent_second[] = {
    "YOU FOUND ME! SECRET NUMBER TWO: GRUNTY HASN'T WASHED HER FEET IN 300 YEARS. PHEW!",
    "MY LAST SECRET WAITS ON THE LITTLE ISLAND WITH THE BIG KREMLING IN A CAGE. SEE YOU THERE!",
};
static const char *const brent_third[] = {
    "SECRET NUMBER THREE: GRUNTY'S CAULDRON IS CALLED DINGPOT, AND HE SNORES ALL NIGHT LONG!",
    "NOW, LET'S SEE IF YOU WERE LISTENING. GET ALL THREE RIGHT AND THERE'S A JIGGY IN IT!",
};
static const char *const brent_retry[] = {
    "BACK FOR ANOTHER GO? MY SECRETS: SLUGS AND SNAILS, 300 YEARS OF SMELLY FEET, AND DINGPOT!",
    "RIGHT, DEARS. HERE COME THE QUESTIONS!",
};
static const char *const brent_wrong[] = {
    "OH DEAR, THAT'S NOT IT! HAVE A THINK AND COME BACK TO TRY AGAIN.",
};
static const char *const brent_won[] = {
    "ALL THREE RIGHT! MY SISTER WOULD BE FURIOUS. HERE, THIS JIGGY IS FOR YOU!",
};
static const char *const brent_after[] = {
    "GO ON, YOU TWO! GIVE MY SISTER A GOOD WALLOP FROM ME.",
};
static const char *const brent_heal[] = {
    "YOU LOOK WORN OUT, DEARS. THERE, ALL BETTER!",
};
typedef struct {
    const char *question;
    const char *options[3];
    s32 right;
} BrentQuestion;
static const BrentQuestion brent_quiz[] = {
    {"WHAT IS GRUNTY'S FAVOURITE SNACK?", {"SLUGS AND SNAILS", "BANANA SPLITS", "HONEY CAKES"}, 0},
    {"HOW LONG SINCE GRUNTY LAST WASHED HER FEET?", {"THREE DAYS", "THIRTY YEARS", "300 YEARS"}, 2},
    {"WHAT IS GRUNTY'S CAULDRON CALLED?", {"POTTY", "DINGPOT", "BUBBLES"}, 1},
};
static const u16 brent_talk_sfx[] = {BRENT_SFX_TALK};
static const JiggyTalker brent_talker = {
    BRENT_PORTRAIT, 6, "BRENTILDA", brent_talk_sfx, 1, 20000.0f / 32767.0f, 1.0f,
};

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_8076A0B3;
// Player 1's PlayerProgress (dk64_decomp's structs.h): health (s8) at 0x2FB, melons at 0x2FC.
extern u8 D_global_asm_807FC950[];
#define PROGRESS_HEALTH 0x2FB
#define PROGRESS_MELONS 0x2FC
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
s32 func_global_asm_8070943C(void);          // Health segments per melon.
void func_global_asm_80709464(u8 playerIndex); // Fills the player's health.

typedef enum {
    BRENT_UNLOADED,
    BRENT_READY,
    BRENT_UNAVAILABLE,
} BrentLoadState;

typedef enum {
    BRENT_HIDDEN,    // Not yet seen on this visit: appears when the player comes near.
    BRENT_APPEARING,
    BRENT_IDLE,
    BRENT_TALKING,   // A text box (a secret, the result, a reminder).
    BRENT_QUIZZING,  // Asking the questions.
    BRENT_VANISHING, // Off to the next spot.
} BrentAction;

typedef enum {
    TALK_NONE,
    TALK_SECRET,    // A secret; after it she vanishes (first two) or asks the quiz (third).
    TALK_QUIZ_LEAD, // The reminder before a retry.
    TALK_WRONG,
    TALK_WON,       // After it, her Jiggy appears.
    TALK_AFTER,
    TALK_HEAL,
} BrentTalk;

static u8 brent_block[BRENT_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx brent_gfx[2][BRENT_GFX_MAX];
static Mtx brent_mtx[2][BRENT_MTX_MAX];
static JiggyBkPose brent_pose;
static BrentLoadState brent_state = BRENT_UNLOADED;

static s32 brent_last_map = -1;
static u32 brent_last_timer;
static const BrentPlacement *brent_place_map; // This map's, or NULL.
static s32 brent_placed;
static u32 brent_saved;
static f32 brent_pos[3];
static f32 brent_reward[3];
static f32 brent_yaw;
static BrentAction brent_action;
static s32 brent_animation;
static f32 brent_progress; // 0 to 1 through the animation.
static f32 brent_timer;    // Seconds into appearing or vanishing.
static BrentTalk brent_talk;
static BrentTalk brent_after_heal; // What a heal box interrupted.
static s32 brent_armed;
static u32 brent_last_talk;
static s32 brent_question;
static s32 brent_reward_due;
static s32 brent_reward_jingle;
static u32 brent_random_state = 0x4252454E;

static f32 brent_random(void) {
    brent_random_state = brent_random_state * 1664525 + 1013904223;
    return (brent_random_state >> 8) * (1.0f / 16777216.0f);
}

static void brent_load(void) {
    s32 size;
    s32 i;

    brent_state = BRENT_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(BRENT_MODEL, brent_block, sizeof(brent_block));
    if (size <= 0 || size > (s32)sizeof(brent_block)) {
        return;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        brent_pose.selectors[i] = 0;
    }
    brent_pose.selectors[1] = 1;
    brent_state = BRENT_READY;
}

static f32 brent_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + BRENT_FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < BRENT_FLOOR_MAX_STEP &&
        y - floor < BRENT_FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

static s32 brent_stage(void) {
    return brent_saved & BRENT_SAVE_STAGE;
}

static void brent_save(u32 saved) {
    brent_saved = saved;
    jiggy_save_set(current_file, BRENT_SAVE_MAP_KEY(current_map), brent_saved);
}

// Stands her at her stage's spot (the last one once the quiz is won), hidden until the player
// comes near.
static void brent_place(Actor *player) {
    s32 stage = brent_stage();
    const BrentSpot *spot = &brent_place_map->spots[stage < 3 ? stage : 2];

    brent_pos[0] = spot->x;
    brent_pos[1] = brent_floor(spot->x, spot->y, spot->z);
    brent_pos[2] = spot->z;
    brent_reward[0] = brent_place_map->reward.x;
    brent_reward[1] = brent_floor(brent_place_map->reward.x, brent_place_map->reward.y, brent_place_map->reward.z);
    brent_reward[2] = brent_place_map->reward.z;
    brent_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, brent_pos[0], brent_pos[2]) & 0xFFF;
    brent_action = BRENT_HIDDEN;
    brent_animation = BRENT_ANIM_IDLE;
    brent_progress = brent_random();
    brent_talk = TALK_NONE;
    brent_reward_due = stage == BRENT_STAGE_DONE;
    brent_reward_jingle = FALSE;
    brent_placed = TRUE;
}

static s32 brent_player_within(Actor *player, f32 radius, f32 height) {
    f32 dx = brent_pos[0] - player->x_position;
    f32 dy = brent_pos[1] - player->y_position;
    f32 dz = brent_pos[2] - player->z_position;

    return dx * dx + dz * dz <= radius * radius && dy >= -height && dy <= height;
}

static s32 brent_player_near(Actor *player, f32 radius) {
    return brent_player_within(player, radius, BRENT_TALK_FEET);
}

// The player can't move while she vanishes (as jiggy_bottles.c holds them).
static void brent_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void brent_turn_to(s16 target, u32 frames) {
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = (target & 0xFFF) - brent_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        brent_yaw += turn * BRENT_TURN_RATE;
        if (brent_yaw < 0.0f) {
            brent_yaw += 4096.0f;
        } else if (brent_yaw >= 4096.0f) {
            brent_yaw -= 4096.0f;
        }
    }
}

static s32 brent_say(BrentTalk kind, const char *const *pages, s32 count) {
    if (!jiggy_talk_say(JIGGY_TALK_OWNER_BRENTILDA, &brent_talker, pages, count, JIGGY_TALK_LOCK)) {
        return FALSE;
    }
    brent_talk = kind;
    brent_action = BRENT_TALKING;
    brent_animation = BRENT_ANIM_TALK;
    return TRUE;
}

static void brent_ask(void) {
    const BrentQuestion *q = &brent_quiz[brent_question];

    if (jiggy_talk_ask(JIGGY_TALK_OWNER_BRENTILDA, &brent_talker, q->question, q->options, 3)) {
        brent_action = BRENT_QUIZZING;
        brent_animation = BRENT_ANIM_TALK;
    }
}

// Whether the player has lost any health.
static s32 brent_player_hurt(void) {
    s32 health = (s8)D_global_asm_807FC950[PROGRESS_HEALTH];

    return health < D_global_asm_807FC950[PROGRESS_MELONS] * func_global_asm_8070943C();
}

// What she says when the player stops by, for her stage.
static void brent_start_talk(void) {
    switch (brent_stage()) {
        case 0:
            brent_say(TALK_SECRET, brent_meet, ARRAY_COUNT_OF(brent_meet));
            break;
        case 1:
            brent_say(TALK_SECRET, brent_second, ARRAY_COUNT_OF(brent_second));
            break;
        case 2:
            if (brent_saved & BRENT_SAVE_QUIZ_ASKED) {
                brent_say(TALK_QUIZ_LEAD, brent_retry, ARRAY_COUNT_OF(brent_retry));
            } else {
                brent_say(TALK_SECRET, brent_third, ARRAY_COUNT_OF(brent_third));
            }
            break;
        default:
            if (brent_player_hurt()) {
                func_global_asm_80709464(0);
                brent_after_heal = TALK_NONE;
                brent_say(TALK_HEAL, brent_heal, ARRAY_COUNT_OF(brent_heal));
            } else {
                brent_say(TALK_AFTER, brent_after, ARRAY_COUNT_OF(brent_after));
            }
            break;
    }
}

// Whether what she'd say now is new (then walking up is enough, and it needn't wait the minute).
static s32 brent_talk_is_new(void) {
    return brent_stage() < BRENT_STAGE_DONE;
}

// What follows a box of hers: a vanish or the quiz after a secret, the Jiggy after the quiz.
static void brent_follow_up(BrentTalk kind) {
    switch (kind) {
        case TALK_SECRET:
            if (brent_stage() < 2) {
                // Off to the next spot, saved now so it stands whatever happens.
                brent_save((brent_saved & ~BRENT_SAVE_STAGE) | (brent_stage() + 1));
                brent_action = BRENT_VANISHING;
                brent_animation = BRENT_ANIM_TALK;
                brent_timer = 0.0f;
                brent_lock_player(TRUE);
                jiggy_bk_jingle_play(JINGLE_DEACTIVATE, 1.0f);
                jiggy_bk_sfx_play(BRENT_SFX_POP, 32000.0f / 32767.0f, 1.0f);
                return;
            }
            brent_save(brent_saved | BRENT_SAVE_QUIZ_ASKED);
            brent_question = 0;
            brent_ask();
            return;
        case TALK_QUIZ_LEAD:
            brent_question = 0;
            brent_ask();
            return;
        case TALK_WON:
            brent_reward_due = TRUE;
            brent_reward_jingle = TRUE;
            return;
        default:
            return;
    }
}

// A box of hers has closed. If the player is hurt she heals them first (chBrentilda heals after
// her talk), then carries on.
static void brent_talk_done(void) {
    BrentTalk kind = brent_talk;

    brent_talk = TALK_NONE;
    brent_action = BRENT_IDLE;
    brent_animation = BRENT_ANIM_IDLE;
    if (kind == TALK_HEAL) {
        kind = brent_after_heal;
        brent_after_heal = TALK_NONE;
        brent_follow_up(kind);
        return;
    }
    if (kind != TALK_WON && brent_player_hurt()) {
        func_global_asm_80709464(0);
        brent_after_heal = kind;
        if (brent_say(TALK_HEAL, brent_heal, ARRAY_COUNT_OF(brent_heal))) {
            return;
        }
        brent_after_heal = TALK_NONE;
    }
    brent_follow_up(kind);
}

// The question box has closed with an answer.
static void brent_answered(s32 pick) {
    if (pick != brent_quiz[brent_question].right) {
        jiggy_bk_jingle_play(JINGLE_WRONG, 22000.0f / 32767.0f);
        brent_say(TALK_WRONG, brent_wrong, ARRAY_COUNT_OF(brent_wrong));
        return;
    }
    jiggy_bk_jingle_play(JINGLE_RIGHT, 28000.0f / 32767.0f);
    brent_question++;
    if (brent_question < (s32)ARRAY_COUNT_OF(brent_quiz)) {
        brent_ask();
        return;
    }
    // Won: saved now, so the Jiggy is hers to give whatever happens.
    brent_save((brent_saved & ~BRENT_SAVE_STAGE) | BRENT_STAGE_DONE);
    brent_say(TALK_WON, brent_won, ARRAY_COUNT_OF(brent_won));
}

static void brent_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    s16 to_player = func_global_asm_80665DE0(player->x_position, player->z_position, brent_pos[0], brent_pos[2]);

    if (frames == 0) {
        return;
    }
    brent_progress += seconds / BRENT_ANIM_SECONDS;
    while (brent_progress >= 1.0f) {
        brent_progress -= 1.0f;
    }
    switch (brent_action) {
        case BRENT_HIDDEN:
            if (brent_player_within(player, BRENT_APPEAR_RADIUS, BRENT_APPEAR_HEIGHT)) {
                brent_action = BRENT_APPEARING;
                brent_timer = 0.0f;
                brent_yaw = to_player & 0xFFF;
                jiggy_bk_jingle_play(JINGLE_ACTIVATE, 1.0f);
            }
            break;
        case BRENT_APPEARING:
            brent_timer += seconds;
            brent_turn_to(to_player, frames);
            if (brent_timer >= BRENT_APPEAR_SECONDS) {
                brent_action = BRENT_IDLE;
                brent_armed = !brent_player_near(player, BRENT_TALK_RADIUS);
            }
            break;
        case BRENT_IDLE:
            if (brent_player_near(player, BRENT_LOOK_RADIUS)) {
                brent_turn_to(to_player, frames);
            }
            if (!brent_armed) {
                brent_armed = !brent_player_near(player, BRENT_REARM_RADIUS) &&
                              (brent_talk_is_new() || object_timer - brent_last_talk >= BRENT_REMIND_FRAMES);
                break;
            }
            if (brent_player_near(player, BRENT_TALK_RADIUS) &&
                player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT &&
                (player->control_state == STATE_IDLE || (brent_talk_is_new() && player->control_state == STATE_WALK)) &&
                !jiggy_talk_open(JIGGY_TALK_OWNER_CHEATO)) {
                s16 to_brent = func_global_asm_80665DE0(brent_pos[0], brent_pos[2], player->x_position, player->z_position);

                brent_armed = FALSE;
                brent_last_talk = object_timer;
                player->y_rotation = to_brent & 0xFFF;
                player->unkEE = to_brent & 0xFFF;
                brent_start_talk();
            }
            break;
        case BRENT_TALKING:
            brent_turn_to(to_player, frames);
            jiggy_talk_update();
            if (!jiggy_talk_open(JIGGY_TALK_OWNER_BRENTILDA)) {
                brent_talk_done();
            }
            break;
        case BRENT_QUIZZING:
            brent_turn_to(to_player, frames);
            jiggy_talk_update();
            if (!jiggy_talk_open(JIGGY_TALK_OWNER_BRENTILDA)) {
                s32 pick = jiggy_talk_answer(JIGGY_TALK_OWNER_BRENTILDA);

                brent_action = BRENT_IDLE;
                brent_animation = BRENT_ANIM_IDLE;
                if (pick >= 0) {
                    brent_answered(pick);
                }
            }
            break;
        case BRENT_VANISHING:
            brent_lock_player(TRUE);
            brent_timer += seconds;
            brent_yaw += BRENT_VANISH_SPIN * seconds;
            while (brent_yaw >= 4096.0f) {
                brent_yaw -= 4096.0f;
            }
            if (brent_timer >= BRENT_VANISH_SECONDS) {
                brent_lock_player(FALSE);
                brent_place(player); // At the next spot, hidden until the player comes.
            }
            break;
    }
    if (brent_reward_due) {
        if (brent_reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
            brent_reward_jingle = FALSE;
        }
        if (jiggy_reward_update(REWARD_ID, brent_reward[0], brent_reward[1] + REWARD_HEIGHT, brent_reward[2], player, frames)) {
            brent_reward_due = FALSE;
        }
    }
}

static Gfx *brent_draw(Gfx *dl, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 size = BK_TO_DK64;
    f32 progress = brent_progress;

    if (brent_action == BRENT_HIDDEN) {
        return dl;
    }
    if (brent_action == BRENT_APPEARING) {
        size *= brent_timer / BRENT_APPEAR_SECONDS;
    } else if (brent_action == BRENT_VANISHING) {
        size *= 1.0f - brent_timer / BRENT_VANISH_SECONDS;
    }
    if (size < 0.005f) {
        return dl;
    }
    guScaleF(scale, size, size, size);
    guRotateF(rotate, brent_yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, brent_pose.world);
    brent_pose.world[3][0] = brent_pos[0];
    brent_pose.world[3][1] = brent_pos[1];
    brent_pose.world[3][2] = brent_pos[2];
    if (progress > 0.9999f) {
        progress = 0.9999f;
    }
    brent_pose.model = BRENT_MODEL;
    brent_pose.block = brent_block;
    brent_pose.animation = brent_animation;
    brent_pose.progress = progress;
    brent_pose.gfx = gfx;
    brent_pose.gfx_capacity = BRENT_GFX_MAX;
    brent_pose.mtx = mtx;
    brent_pose.mtx_capacity = BRENT_MTX_MAX;
    if (jiggy_bk_model_pose(&brent_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Brentilda once a frame, with the player.
Gfx *jiggy_brentilda_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over).
    if ((s32)current_map != brent_last_map || object_timer < brent_last_timer) {
        if (brent_action == BRENT_TALKING || brent_action == BRENT_QUIZZING || brent_action == BRENT_VANISHING) {
            jiggy_talk_close(JIGGY_TALK_OWNER_BRENTILDA);
            if (brent_action == BRENT_VANISHING) {
                brent_lock_player(FALSE);
            }
        }
        brent_last_map = current_map;
        brent_placed = FALSE;
        brent_place_map = NULL;
        brent_action = BRENT_HIDDEN;
        for (i = 0; i < ARRAY_COUNT_OF(brent_maps); i++) {
            if (brent_maps[i].map == current_map) {
                brent_place_map = &brent_maps[i];
            }
        }
        brent_saved = brent_place_map != NULL ? jiggy_save_get(current_file, BRENT_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (brent_place_map == NULL) {
        brent_last_timer = object_timer;
        return dl;
    }
    if (brent_state == BRENT_UNLOADED) {
        brent_load();
    }
    if (brent_state != BRENT_READY) {
        return dl;
    }
    if (!brent_placed) {
        brent_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        brent_place(actor);
    }
    frames = object_timer - brent_last_timer;
    brent_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    // She waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && brent_action != BRENT_TALKING && brent_action != BRENT_QUIZZING &&
        brent_action != BRENT_VANISHING && !jiggy_talk_open(JIGGY_TALK_OWNER_BRENTILDA)) {
        frames = 0;
    }
    brent_update(actor, frames);
    return brent_draw(dl, brent_gfx[buffer], brent_mtx[buffer]);
}
