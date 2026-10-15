// Boggy: Freezeezy Peak's polar bear and his three kids, read from the player's Banjo-Kazooie
// ROM, in Crystal Caves, where the two worlds have collided; with the presents quest.
//
// In Banjo-Kazooie the kids (Moggy, Soggy and Groggy: ASSET_44C/44D/44E_MODEL_POLAR_BEAR_CUB_*)
// sit sad in Boggy's igloo until Banjo brings each their present (the blue, green and red
// ASSET_47F/480/481_MODEL_XMAS_GIFT_*), then cheer, and the last one brings out a Jiggy
// (JIGGY_2E_FP_PRESENTS, bearcub.c and levelcollectible.c). Here Sir Slush (jiggy_sir_slush.c)
// has flung the presents across the caves: they wait on the ground, turning slowly with their
// wobble (ASSET_18A_ANIM_XMAS_GIFT). Touching one takes it, with Banjo-Kazooie's ding
// (COMUSIC_2B_DING_B), and its 2D present (ASSET_822/823/824_SPRITE_XMAS_GIFT_*) shows in the
// HUD until it's handed over; DK64 can't carry it, so it's kept as an item, as Chimpy's orange
// is (jiggy_chimpy.c).
//
// The kids mope with their sad animation and Banjo-Kazooie's crying (SFX_B1_BOGGY_KID_CRYING at
// 0.45 through it) until their present comes, then hop with their happy one (SFX_B2 at 0.4,
// SFX_53 at 0.75, each kid a little higher, as func_8038A274 plays them). Boggy sits behind them
// and turns to face the player. His model is one of Banjo-Kazooie's vertex-animated ones (its
// geo list has no bones), which jiggy_bk_model_pose draws in its rest pose, so he only breathes
// (a slow stretch) and turns.
//
// Talking, in the shared text box (jiggy_caves_talk.c): coming near (standing or walking, the
// first time; standing, for a repeat, by jiggy_bottles.c's reminder rule), Boggy explains, and
// after that reminds the player where the missing presents went. Bringing presents, the kids
// thank the player, one page each (their portrait, ASSET_843_SPRITE_POLAR_BEAR_CUBS); the last
// one plays the puzzle solved fanfare (COMUSIC_2D), Boggy says his thanks and cheers
// (SFX_8C_BOGGY_WAHEY), and a Golden Banana (drawn as a Jiggy) appears in front of him with the
// Jiggy jingle (COMUSIC_3D). The player is held in the cutscene lock from the last present until
// it appears. The reward is jiggy_rewards.c's CAVES_REWARD_BOGGY; where it appears is saved, so a
// Jiggy left behind is there next visit.
//
// Saved (jiggy_save_get/set, CAVES_SAVE_KEY(CAVES_CHARACTER_BOGGY, map)): BOGGY_SAVE_* bits;
// the reward's spot under CAVES_SAVE_KEY(BOGGY_SPOT_CHARACTER, map).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), the
// carried presents in the HUD pass (jiggy_boggy_hud_draw, patches_ui.c). Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"
#include "jiggy_caves.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define BOGGY_MODEL 0x38A // ASSET_38A_MODEL_BOGGY_1
static const u16 cub_models[3] = {0x44C, 0x44D, 0x44E};     // ASSET_44C/44D/44E_MODEL_POLAR_BEAR_CUB_BLUE/GREEN/RED
static const u16 present_models[3] = {0x47F, 0x480, 0x481}; // ASSET_47F/480/481_MODEL_XMAS_GIFT_BLUE/GREEN/RED
static const u16 present_sprites[3] = {0x822, 0x823, 0x824}; // ASSET_822/823/824_SPRITE_XMAS_GIFT_*: 32x32
#define BOGGY_PORTRAIT 0x83C // ASSET_83C_SPRITE_BOGGY
#define CUBS_PORTRAIT 0x843  // ASSET_843_SPRITE_POLAR_BEAR_CUBS
// Boggy converts to about 23 KiB (40 bones, but no geo bones: 13 pose commands at most); a kid
// to 17.5-18 KiB (33-36 bones, about 105 commands); a present to about 11 KiB (4 bones, 8).
#define BOGGY_BLOCK_MAX 0x5D00
#define BOGGY_GFX_MAX 24
#define BOGGY_MTX_MAX 44
#define CUB_BLOCK_MAX 0x4800
#define CUB_GFX_MAX 128
#define CUB_MTX_MAX 40
#define PRESENT_BLOCK_MAX 0x2D00
#define PRESENT_GFX_MAX 16
#define PRESENT_MTX_MAX 8
#define HUD_PRESENT_MAX_TEXELS (32 * 32)

// Banjo-Kazooie animations and how long each plays, in seconds.
#define BOGGY_ANIM_IDLE 0x14C       // ASSET_14C_ANIM_BOGGY_IDLE (kept for when vertex animation is drawn)
#define CUB_ANIM_SAD 0x17E          // ASSET_17E_ANIM_POLAR_BEAR_CUB_SAD
#define CUB_ANIM_HAPPY 0x17D        // ASSET_17D_ANIM_POLAR_BEAR_CUB_HAPPY
#define PRESENT_ANIM 0x18A          // ASSET_18A_ANIM_XMAS_GIFT
#define BOGGY_IDLE_SECONDS 2.0f
#define CUB_SAD_SECONDS 5.5f
#define CUB_HAPPY_SECONDS 2.5f
#define PRESENT_SECONDS 1.5f
// Each kid starts its animation a step further on (bearcub.c's 0.0, 0.4 and 0.7).
static const f32 cub_phase[3] = {0.0f, 0.4f, 0.7f};
// The kids' happy noises (func_8038A274: SFX_B2 at their first pitch, SFX_53 at their second).
static const f32 cub_pitch[3][2] = {{1.0f, 1.2f}, {1.1f, 1.3f}, {1.2f, 1.4f}};

// Banjo-Kazooie sounds and music.
#define SFX_KID_CRYING 0xB1 // SFX_B1_BOGGY_KID_CRYING: at 0.45 through the sad loop, 32000/32767, 0.9-1.1
#define SFX_KID_HAPPY 0xB2  // SFX_B2_BOGGY_KID_HAPPY: at 0.4 through the happy loop, 32000/32767
#define SFX_KID_HUIII 0x53  // SFX_53_BANJO_HUIII: at 0.75 through it, 32000/32767
#define SFX_BOGGY_WAHEY 0x8C // SFX_8C_BOGGY_WAHEY
#define SFX_PRESENT_BOUNCE 0x21 // SFX_21_EGG_BOUNCE_1: a present handed over, 25000/32767, 0.76
#define JINGLE_DING 0x2B          // COMUSIC_2B_DING_B: a present taken, 0x7FFF/32767
#define JINGLE_PUZZLE_SOLVED 0x2D // COMUSIC_2D_PUZZLE_SOLVED_FANFARE, 0x7FFF/32767
#define JINGLE_JIGGY_SPAWN 0x3D   // COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767
// The kids are heard from this close (Banjo-Kazooie's 150 to 700 units, scaled).
#define KID_SFX_NEAR 37.0f
#define KID_SFX_FAR 175.0f
// Their talking sounds (gczoombox: Boggy SFX_10F at 20000/32767, 1.2; the kids SFX_BF/C0 at
// 20000/32767, 1.6).
static const u16 boggy_talk_sfx[] = {0x10F};
static const u16 cubs_talk_sfx[] = {0xBF, 0xC0};
static const CavesSpeaker boggy_speaker = {"BOGGY", BOGGY_PORTRAIT, 6, boggy_talk_sfx, 1, 20000.0f / 32767.0f, 1.2f};
static const CavesSpeaker cubs_speaker = {"BOGGY'S KIDS", CUBS_PORTRAIT, 6, cubs_talk_sfx, 2, 20000.0f / 32767.0f, 1.6f};

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo, and their sizes on top:
// Boggy a little bigger than Banjo-Kazooie's 1.0 (he's a big bear next to the Kongs), the kids
// at 1.0 (smaller than his 1.2 against them), the presents at 1.0.
#define BK_TO_DK64 0.25f
#define BOGGY_SCALE (BK_TO_DK64 * 1.2f)
#define CUB_SCALE (BK_TO_DK64 * 1.0f)
#define PRESENT_SCALE (BK_TO_DK64 * 1.0f)
#define GAME_FPS 30.0f
#define TURN_RATE_BOGGY 0.08f
#define TURN_RATE_CUB 0.12f
// Boggy breathes: his height stretches by this much, this often.
#define BOGGY_BREATH 0.02f
#define BOGGY_BREATH_SECONDS 3.0f

// Talking: Boggy talks when the player comes this close to him (Banjo-Kazooie's 250 units
// plus his size), on the ground; a repeat needs the player to have been this far off since,
// and comes at most this often (jiggy_bottles.c's reminder rule).
#define TALK_RADIUS 85.0f
#define REARM_RADIUS 250.0f
#define REPEAT_FRAMES (60 * 30)
#define TALK_HEIGHT 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing them on the floor under their spots: looked for from this far above each spot, and
// only trusted this close to its height.
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f
// Presents: they turn slowly, and are taken within this distance and height.
#define PRESENT_SPIN_PER_FRAME 24.0f // 12-bit angle units.
#define PRESENT_TAKE_RADIUS 24.0f
#define PRESENT_TAKE_HEIGHT 40.0f
// The Golden Banana: this far in front of Boggy toward the player, this high over the floor;
// it appears after Boggy's cheer.
#define REWARD_FORWARD 38.0f
#define REWARD_HEIGHT 6.0f
#define CHEER_SECONDS 1.2f

// DK64 player states (Actor control_state) he talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D

#define PRESENT_COUNT 3
// Where they are, map by map. Crystal Caves: Boggy and the kids on the round ledge in the south
// (about y 280), east of Chunky's boulder (1926, 2498), clear of the notes' ring round the ledge
// and the DK balloon (1972, 2615). The presents: the blue one on the round ledge in the far
// north-east (y 287, inside its notes' ring), the green one on the round ledge out west (y 180,
// past its door at (300, 2428)), the red one in the Kremlings' camp east of the entrance (y 14).
// All are ground DK64's own bananas and coins stand on.
typedef struct {
    Maps map;
    f32 x, y, z;             // Boggy.
    f32 cub[3][2];           // The kids (x, z), on Boggy's floor.
    f32 present[PRESENT_COUNT][3];
} BoggyPlacement;
static const BoggyPlacement boggy_spots[] = {
    {MAP_CAVES, 2060.0f, 280.0f, 2390.0f,
     {{2005.0f, 2430.0f}, {2040.0f, 2462.0f}, {2085.0f, 2450.0f}},
     {{3520.0f, 287.0f, 700.0f}, {480.0f, 180.0f, 2560.0f}, {2580.0f, 14.0f, 470.0f}}},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// What they say. Each page is one press of A; lines are broken to fit the box when shown.
static const char *const boggy_ask[] = {
    "HUFF... OH, IT'S YOU TWO. NAME'S BOGGY, AND THESE ARE MY KIDS.",
    "THAT ROTTEN SNOWMAN SIR SLUSH PINCHED THEIR PRESENTS AND FLUNG 'EM ALL OVER THESE CAVES!",
    "A BLUE ONE, A GREEN ONE AND A RED ONE. FIND 'EM AND HAND 'EM TO THE KIDS, WILL YA?",
};
static const char *const boggy_remind = "STILL MISSING PRESENTS? THE KIDS WON'T STOP BLUBBING!";
static const char *const boggy_hints[PRESENT_COUNT] = {
    "THE BLUE ONE WENT UP ON THE ROUND LEDGE, WAY OUT NORTH-EAST.",
    "THE GREEN ONE LANDED OUT WEST, ON THE ROUND LEDGE PAST THE SLOPE.",
    "THE RED ONE'S IN THE KREMLINGS' CAMP, BY WHERE YOU CAME IN.",
};
static const char *const boggy_given_back = "HAND 'EM TO THE KIDS, NOT ME! I'M ON MY HOLIDAYS.";
static const char *const cubs_thanks[PRESENT_COUNT] = {
    "MY BLUE PRESENT! MOGGY'S NOT SAD ANY MORE!",
    "THE GREEN ONE'S MINE! SOGGY SAYS THANK YOU!",
    "RED! THAT'S GROGGY'S! YIPPEE!",
};
static const char *const cubs_more_one = "BUT WHERE'S THE LAST PRESENT? WAAAH!";
static const char *const cubs_more_two = "WHERE ARE THE OTHER TWO PRESENTS? SNIFF...";
static const char *const cubs_all = "WE ALL GOT OUR PRESENTS! DAD, GIVE THEM THE SHINY THING!";
static const char *const boggy_reward_pages[] = {
    "WAHEY! THE KIDS ARE HAPPY AT LAST. HERE, YOU'VE EARNED THIS JIGGY!",
};
static const char *const boggy_after[] = {
    "THE KIDS HAVEN'T STOPPED GRINNING. THANKS AGAIN, YOU TWO!",
};

// Save keys: BOGGY_SAVE_* bits; and the reward's spot, x in the top half and z in the bottom.
#define BOGGY_SAVE_KEY(map) CAVES_SAVE_KEY(CAVES_CHARACTER_BOGGY, map)
#define BOGGY_SPOT_CHARACTER 0x10
#define BOGGY_SPOT_KEY(map) CAVES_SAVE_KEY(BOGGY_SPOT_CHARACTER, map)
#define BOGGY_SAVE_MET 0x1
#define BOGGY_SAVE_TAKEN(i) (0x2 << (i))  // 0x2, 0x4, 0x8
#define BOGGY_SAVE_GIVEN(i) (0x10 << (i)) // 0x10, 0x20, 0x40
#define BOGGY_SAVE_DONE 0x80              // The Jiggy's earned.

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern s16 D_global_asm_80744490; // Screen width.
extern u8 D_global_asm_8076A0B1;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y and returns nonzero, or leaves it as it was.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    BOGGY_UNLOADED,
    BOGGY_READY,
    BOGGY_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model didn't convert.
} BoggyLoadState;

typedef enum {
    TALK_NONE,
    TALK_ASK,
    TALK_REMIND,
    TALK_DELIVER, // The kids' thanks.
    TALK_REWARD,  // Boggy's, after the last present.
} BoggyTalk;

static u8 boggy_block[BOGGY_BLOCK_MAX] __attribute__((aligned(16)));
static u8 cub_blocks[3][CUB_BLOCK_MAX] __attribute__((aligned(16)));
static u8 present_blocks[PRESENT_COUNT][PRESENT_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx boggy_gfx[2][BOGGY_GFX_MAX];
static Mtx boggy_mtx[2][BOGGY_MTX_MAX];
static Gfx cub_gfx[2][3][CUB_GFX_MAX];
static Mtx cub_mtx[2][3][CUB_MTX_MAX];
static Gfx present_gfx[2][PRESENT_COUNT][PRESENT_GFX_MAX];
static Mtx present_mtx[2][PRESENT_COUNT][PRESENT_MTX_MAX];
static JiggyBkPose boggy_pose;
static u16 hud_present_texels[PRESENT_COUNT][HUD_PRESENT_MAX_TEXELS] __attribute__((aligned(8)));
static s32 hud_present_width[PRESENT_COUNT]; // 0 when the sprite didn't convert: no icon.
static s32 hud_present_height[PRESENT_COUNT];
static s32 hud_present_alpha[PRESENT_COUNT];
static BoggyLoadState boggy_state = BOGGY_UNLOADED;

static s32 boggy_last_map = -1;
static u32 boggy_last_timer;
static const BoggyPlacement *boggy_spot; // This map's, or NULL.
static s32 boggy_placed;
static f32 boggy_pos[3];
static f32 boggy_yaw;
static f32 boggy_breath;
static f32 cub_pos[3][3];
static f32 cub_yaw[3];
static f32 cub_progress[3];
static f32 present_pos[PRESENT_COUNT][3];
static f32 present_yaw;
static f32 present_progress;
static u32 boggy_saved;
static s32 boggy_armed;
static u32 boggy_last_talk;
static BoggyTalk talk_kind;
static s32 boggy_holding; // The player's held from the last present until the Golden Banana appears.
static f32 cheer_timer;
static f32 reward_spot[3];
static s32 reward_due;
static u32 boggy_random_state = 0x424F4747;
// The pages of a talk put together on the spot (reminders, the kids' thanks).
static const char *talk_pages[PRESENT_COUNT + 2];

static f32 boggy_random(void) {
    boggy_random_state = boggy_random_state * 1664525 + 1013904223;
    return (boggy_random_state >> 8) * (1.0f / 16777216.0f);
}

static s32 boggy_build(u32 model, u8 *block, u32 capacity) {
    s32 size = jiggy_bk_model_build(model, block, capacity);

    return size > 0 && size <= (s32)capacity;
}

static void boggy_load(void) {
    s32 i;

    boggy_state = BOGGY_UNAVAILABLE;
    if (!jiggy_bk_available() || !boggy_build(BOGGY_MODEL, boggy_block, sizeof(boggy_block))) {
        return;
    }
    for (i = 0; i < 3; i++) {
        if (!boggy_build(cub_models[i], cub_blocks[i], sizeof(cub_blocks[i])) ||
            !boggy_build(present_models[i], present_blocks[i], sizeof(present_blocks[i]))) {
            return;
        }
    }
    // The presents for the HUD; one that doesn't convert just has no icon.
    for (i = 0; i < PRESENT_COUNT; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(present_sprites[i], 0, hud_present_texels[i], sizeof(hud_present_texels[i]));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        hud_present_width[i] = 0;
        if (got > 0 && width > 0 && height > 0 && width * height <= HUD_PRESENT_MAX_TEXELS) {
            hud_present_width[i] = width;
            hud_present_height[i] = height;
        }
    }
    boggy_state = BOGGY_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// The floor under (x, z), looked for from above `y`; `y` if there's none near.
static f32 boggy_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < FLOOR_MAX_STEP && y - floor < FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

// A Banjo-Kazooie sound from (x, z), fading out between `near` and `far` from the player.
static void boggy_sfx_at(Actor *player, f32 x, f32 z, u32 sfx, f32 volume, f32 pitch, f32 near, f32 far) {
    f32 distance = distance_xz(x, z, player->x_position, player->z_position);

    if (distance >= far) {
        return;
    }
    if (distance > near) {
        volume *= (far - distance) / (far - near);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static void boggy_save(u32 bits) {
    boggy_saved |= bits;
    jiggy_save_set(current_file, BOGGY_SAVE_KEY(current_map), boggy_saved);
}

static s32 present_carried(s32 i) {
    return (boggy_saved & BOGGY_SAVE_TAKEN(i)) && !(boggy_saved & BOGGY_SAVE_GIVEN(i));
}

static s32 presents_given(void) {
    s32 n = 0;
    s32 i;

    for (i = 0; i < PRESENT_COUNT; i++) {
        if (boggy_saved & BOGGY_SAVE_GIVEN(i)) {
            n++;
        }
    }
    return n;
}

// Where the Golden Banana goes, from the saved spot (or in front of Boggy, if none was saved).
static void reward_place(void) {
    u32 spot = jiggy_save_get(current_file, BOGGY_SPOT_KEY(current_map));

    if (spot != 0) {
        reward_spot[0] = (s16)(spot >> 16);
        reward_spot[2] = (s16)(spot & 0xFFFF);
    } else {
        reward_spot[0] = boggy_pos[0] + func_global_asm_80612794((s16)boggy_yaw) * REWARD_FORWARD;
        reward_spot[2] = boggy_pos[2] + func_global_asm_80612790((s16)boggy_yaw) * REWARD_FORWARD;
    }
    reward_spot[1] = boggy_floor(reward_spot[0], boggy_pos[1], reward_spot[2]);
}

static void boggy_place(Actor *player) {
    s32 i;

    boggy_pos[0] = boggy_spot->x;
    boggy_pos[1] = boggy_floor(boggy_spot->x, boggy_spot->y, boggy_spot->z);
    boggy_pos[2] = boggy_spot->z;
    boggy_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, boggy_pos[0], boggy_pos[2]) & 0xFFF;
    for (i = 0; i < 3; i++) {
        cub_pos[i][0] = boggy_spot->cub[i][0];
        cub_pos[i][1] = boggy_floor(boggy_spot->cub[i][0], boggy_pos[1], boggy_spot->cub[i][1]);
        cub_pos[i][2] = boggy_spot->cub[i][1];
        cub_yaw[i] = func_global_asm_80665DE0(player->x_position, player->z_position, cub_pos[i][0], cub_pos[i][2]) & 0xFFF;
        cub_progress[i] = cub_phase[i];
    }
    for (i = 0; i < PRESENT_COUNT; i++) {
        present_pos[i][0] = boggy_spot->present[i][0];
        present_pos[i][1] = boggy_floor(boggy_spot->present[i][0], boggy_spot->present[i][1], boggy_spot->present[i][2]);
        present_pos[i][2] = boggy_spot->present[i][2];
    }
    boggy_holding = FALSE;
    talk_kind = TALK_NONE;
    reward_due = (boggy_saved & BOGGY_SAVE_DONE) != 0;
    if (reward_due) {
        reward_place();
    }
    boggy_placed = TRUE;
}

static s32 boggy_player_near(Actor *player, f32 radius) {
    f32 dy = boggy_pos[1] - player->y_position;

    return distance_xz(boggy_pos[0], boggy_pos[2], player->x_position, player->z_position) <= radius && dy >= -TALK_HEIGHT &&
           dy <= TALK_HEIGHT;
}

static void boggy_talk(BoggyTalk kind, const CavesSpeaker *speaker, const char *const *pages, s32 count, s32 hold) {
    if (jiggy_caves_talk_start(speaker, pages, count, hold)) {
        talk_kind = kind;
    }
}

static void boggy_start_reward(void);

// The kids' thanks for the presents just brought (and the fanfare, for the last).
static void boggy_deliver(Actor *player) {
    s32 count = 0;
    s32 given;
    s32 i;

    for (i = 0; i < PRESENT_COUNT; i++) {
        if (present_carried(i)) {
            boggy_save(BOGGY_SAVE_GIVEN(i));
            talk_pages[count++] = cubs_thanks[i];
            cub_progress[i] = 0.0f;
        }
    }
    given = presents_given();
    if (given == PRESENT_COUNT) {
        talk_pages[count++] = cubs_all;
        // Boggy turns to the player now, so the Golden Banana lands between them.
        boggy_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, boggy_pos[0], boggy_pos[2]) & 0xFFF;
        jiggy_bk_jingle_play(JINGLE_PUZZLE_SOLVED, 1.0f);
    } else {
        talk_pages[count++] = given == PRESENT_COUNT - 1 ? cubs_more_one : cubs_more_two;
    }
    jiggy_bk_sfx_play(SFX_PRESENT_BOUNCE, 25000.0f / 32767.0f, 0.76f);
    // Held through to the end of the cutscene after the last present.
    boggy_talk(TALK_DELIVER, &cubs_speaker, talk_pages, count, given == PRESENT_COUNT);
    if (given == PRESENT_COUNT && talk_kind != TALK_DELIVER) {
        boggy_start_reward();
    }
}

// Boggy's reminder: where the presents still out went.
static void boggy_remind_talk(void) {
    s32 count = 0;
    s32 i;

    if (boggy_saved & BOGGY_SAVE_DONE) {
        boggy_talk(TALK_REMIND, &boggy_speaker, boggy_after, ARRAY_COUNT_OF(boggy_after), FALSE);
        return;
    }
    talk_pages[count++] = boggy_remind;
    for (i = 0; i < PRESENT_COUNT; i++) {
        if (!(boggy_saved & BOGGY_SAVE_TAKEN(i))) {
            talk_pages[count++] = boggy_hints[i];
        }
    }
    if (count == 1) {
        // All found, just not handed over (he only sees them when the player's near the kids).
        talk_pages[count++] = boggy_given_back;
    }
    boggy_talk(TALK_REMIND, &boggy_speaker, talk_pages, count, FALSE);
}

// Starts a talk when the player stops by (see the top of this file).
static void boggy_check_talk(Actor *player) {
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;
    s32 standing = player->control_state == STATE_IDLE;
    s32 walking = player->control_state == STATE_WALK;
    s32 carrying = present_carried(0) || present_carried(1) || present_carried(2);
    s16 to_boggy;

    if (!boggy_armed) {
        boggy_armed = !boggy_player_near(player, REARM_RADIUS) &&
                      (!(boggy_saved & BOGGY_SAVE_MET) || object_timer - boggy_last_talk >= REPEAT_FRAMES);
    }
    if (!boggy_player_near(player, TALK_RADIUS) || !grounded || !(standing || walking)) {
        return;
    }
    if (carrying) {
        boggy_save(BOGGY_SAVE_MET);
        boggy_deliver(player);
    } else if (presents_given() == PRESENT_COUNT && !(boggy_saved & BOGGY_SAVE_DONE)) {
        // All handed over, but the map changed before the Jiggy came: Boggy gives it now.
        boggy_start_reward();
    } else if (!(boggy_saved & BOGGY_SAVE_MET)) {
        boggy_save(BOGGY_SAVE_MET);
        boggy_talk(TALK_ASK, &boggy_speaker, boggy_ask, ARRAY_COUNT_OF(boggy_ask), FALSE);
    } else if (boggy_armed && standing) {
        boggy_remind_talk();
    } else {
        return;
    }
    boggy_armed = FALSE;
    boggy_last_talk = object_timer;
    // The player turns to Boggy (as Bottles turns them).
    to_boggy = func_global_asm_80665DE0(boggy_pos[0], boggy_pos[2], player->x_position, player->z_position);
    player->y_rotation = to_boggy & 0xFFF;
    player->unkEE = to_boggy & 0xFFF;
}

// Boggy cheers, holding the player, then the Golden Banana appears (in boggy_update).
static void boggy_cheer(void) {
    boggy_holding = TRUE;
    cheer_timer = CHEER_SECONDS;
    jiggy_caves_lock_player(TRUE);
    jiggy_bk_sfx_play(SFX_BOGGY_WAHEY, 1.0f, 1.0f);
}

// Boggy's thanks, held into his cheer; straight to the cheer if the box won't open.
static void boggy_start_reward(void) {
    boggy_talk(TALK_REWARD, &boggy_speaker, boggy_reward_pages, ARRAY_COUNT_OF(boggy_reward_pages), TRUE);
    if (talk_kind != TALK_REWARD) {
        boggy_cheer();
    }
}

// A talk just ended.
static void boggy_talk_done(void) {
    BoggyTalk kind = talk_kind;

    talk_kind = TALK_NONE;
    if (kind == TALK_DELIVER && presents_given() == PRESENT_COUNT) {
        boggy_start_reward();
    } else if (kind == TALK_REWARD) {
        boggy_cheer();
    }
}

// The presents still on the ground: taken when touched.
static void presents_update(Actor *player, u32 frames) {
    s32 i;

    present_yaw += PRESENT_SPIN_PER_FRAME * frames;
    while (present_yaw >= 4096.0f) {
        present_yaw -= 4096.0f;
    }
    present_progress += frames / GAME_FPS / PRESENT_SECONDS;
    while (present_progress >= 1.0f) {
        present_progress -= 1.0f;
    }
    for (i = 0; i < PRESENT_COUNT; i++) {
        f32 dy = player->y_position - present_pos[i][1];

        if (boggy_saved & BOGGY_SAVE_TAKEN(i)) {
            continue;
        }
        if (dy > -PRESENT_TAKE_HEIGHT && dy < PRESENT_TAKE_HEIGHT &&
            distance_xz(present_pos[i][0], present_pos[i][2], player->x_position, player->z_position) <= PRESENT_TAKE_RADIUS) {
            boggy_save(BOGGY_SAVE_TAKEN(i));
            jiggy_bk_jingle_play(JINGLE_DING, 1.0f);
        }
    }
}

// Turns `yaw` a share of the way to face (x, z) from `pos`, over `frames` frames.
static void turn_to(f32 *yaw, const f32 pos[3], f32 x, f32 z, f32 rate, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, z, pos[0], pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - *yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        *yaw += turn * rate;
        if (*yaw < 0.0f) {
            *yaw += 4096.0f;
        } else if (*yaw >= 4096.0f) {
            *yaw -= 4096.0f;
        }
    }
}

static s32 passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

// The kids: sad until their present comes, then happy.
static void cubs_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    s32 i;

    for (i = 0; i < 3; i++) {
        s32 happy = (boggy_saved & BOGGY_SAVE_GIVEN(i)) != 0;
        f32 before = cub_progress[i];

        turn_to(&cub_yaw[i], cub_pos[i], player->x_position, player->z_position, TURN_RATE_CUB, frames);
        cub_progress[i] += seconds / (happy ? CUB_HAPPY_SECONDS : CUB_SAD_SECONDS);
        if (!jiggy_caves_talk_is_open()) {
            if (!happy && passed(before, cub_progress[i], 0.45f)) {
                boggy_sfx_at(player, cub_pos[i][0], cub_pos[i][2], SFX_KID_CRYING, 1.0f, 0.9f + boggy_random() * 0.2f,
                             KID_SFX_NEAR, KID_SFX_FAR);
            } else if (happy && passed(before, cub_progress[i], 0.4f)) {
                boggy_sfx_at(player, cub_pos[i][0], cub_pos[i][2], SFX_KID_HAPPY, 1.0f, cub_pitch[i][0], KID_SFX_NEAR,
                             KID_SFX_FAR);
            } else if (happy && passed(before, cub_progress[i], 0.75f)) {
                boggy_sfx_at(player, cub_pos[i][0], cub_pos[i][2], SFX_KID_HUIII, 1.0f, cub_pitch[i][1], KID_SFX_NEAR,
                             KID_SFX_FAR);
            }
        }
        while (cub_progress[i] >= 1.0f) {
            cub_progress[i] -= 1.0f;
        }
    }
}

// A frame of the quest, `frames` game frames on.
static void boggy_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;

    if (frames == 0) {
        return;
    }
    presents_update(player, frames);
    cubs_update(player, frames);
    boggy_breath += seconds / BOGGY_BREATH_SECONDS;
    while (boggy_breath >= 1.0f) {
        boggy_breath -= 1.0f;
    }
    if (!boggy_holding) {
        turn_to(&boggy_yaw, boggy_pos, player->x_position, player->z_position, TURN_RATE_BOGGY, frames);
    }
    if (talk_kind != TALK_NONE) {
        if (jiggy_caves_talk_update()) {
            boggy_talk_done();
        }
    } else if (boggy_holding) {
        jiggy_caves_lock_player(TRUE);
        cheer_timer -= seconds;
        if (cheer_timer <= 0.0f) {
            // The Golden Banana appears in front of him, and the player can move again.
            boggy_holding = FALSE;
            boggy_save(BOGGY_SAVE_DONE);
            reward_place();
            jiggy_save_set(current_file, BOGGY_SPOT_KEY(current_map),
                           ((u32)(u16)(s16)reward_spot[0] << 16) | (u16)(s16)reward_spot[2]);
            reward_due = TRUE;
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
            jiggy_caves_lock_player(FALSE);
            boggy_armed = FALSE;
            boggy_last_talk = object_timer;
        }
    } else if (!jiggy_caves_talk_is_open()) {
        boggy_check_talk(player);
    }
    if (reward_due && !boggy_holding) {
        if (jiggy_reward_update(CAVES_REWARD_BOGGY, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], player,
                                frames)) {
            reward_due = FALSE;
        }
    }
}

// Draws a built model at a spot, turned to `yaw` (12-bit angle units), at `scale`, stretched
// upward by `stretch`.
static Gfx *boggy_pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, const f32 pos[3], f32 yaw,
                            f32 scale, f32 stretch, Gfx *gfx, u32 gfx_capacity, Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 size[4][4];
    s32 i;

    guScaleF(size, scale, scale * stretch, scale);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(size, rotate, boggy_pose.world);
    boggy_pose.world[3][0] = pos[0];
    boggy_pose.world[3][1] = pos[1];
    boggy_pose.world[3][2] = pos[2];
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden. Boggy's selector 1 is his sled (hidden, as boggy3.c draws him at home) and 3
    // his eyes (open).
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        boggy_pose.selectors[i] = 0;
    }
    if (model == BOGGY_MODEL) {
        boggy_pose.selectors[3] = 1;
    } else {
        boggy_pose.selectors[1] = 1;
    }
    boggy_pose.model = model;
    boggy_pose.block = block;
    boggy_pose.animation = animation;
    boggy_pose.progress = progress;
    boggy_pose.gfx = gfx;
    boggy_pose.gfx_capacity = gfx_capacity;
    boggy_pose.mtx = mtx;
    boggy_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&boggy_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// Boggy, his kids and the presents once a frame, with the player.
Gfx *jiggy_boggy_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): back where they were.
    if ((s32)current_map != boggy_last_map || object_timer < boggy_last_timer) {
        if (talk_kind != TALK_NONE || boggy_holding) {
            jiggy_caves_talk_cancel();
            jiggy_caves_lock_player(FALSE);
        }
        talk_kind = TALK_NONE;
        boggy_holding = FALSE;
        boggy_last_map = current_map;
        boggy_placed = FALSE;
        boggy_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(boggy_spots); i++) {
            if (boggy_spots[i].map == current_map) {
                boggy_spot = &boggy_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        boggy_saved = boggy_spot != NULL ? jiggy_save_get(current_file, BOGGY_SAVE_KEY(current_map)) : 0;
    }
    if (boggy_spot == NULL) {
        boggy_last_timer = object_timer;
        return dl;
    }
    if (boggy_state == BOGGY_UNLOADED) {
        boggy_load();
    }
    if (boggy_state != BOGGY_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!boggy_placed) {
        boggy_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        boggy_place(actor);
        boggy_armed = !boggy_player_near(actor, REARM_RADIUS);
        boggy_last_talk = object_timer - REPEAT_FRAMES;
    }
    frames = object_timer - boggy_last_timer;
    boggy_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // They wait while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CAVES_CUTSCENE_LOCK) && talk_kind == TALK_NONE && !boggy_holding) {
        frames = 0;
    }
    boggy_update(actor, frames);

    dl = boggy_pose_draw(dl, BOGGY_MODEL, boggy_block, BOGGY_ANIM_IDLE, boggy_breath, boggy_pos, boggy_yaw, BOGGY_SCALE,
                         1.0f + BOGGY_BREATH * func_global_asm_80612794((s16)(boggy_breath * 4096.0f)),
                         boggy_gfx[buffer], BOGGY_GFX_MAX, boggy_mtx[buffer], BOGGY_MTX_MAX);
    for (i = 0; i < 3; i++) {
        dl = boggy_pose_draw(dl, cub_models[i], cub_blocks[i],
                             (boggy_saved & BOGGY_SAVE_GIVEN(i)) ? CUB_ANIM_HAPPY : CUB_ANIM_SAD, cub_progress[i], cub_pos[i],
                             cub_yaw[i], CUB_SCALE, 1.0f, cub_gfx[buffer][i], CUB_GFX_MAX, cub_mtx[buffer][i], CUB_MTX_MAX);
    }
    for (i = 0; i < PRESENT_COUNT; i++) {
        if (boggy_saved & BOGGY_SAVE_TAKEN(i)) {
            continue;
        }
        dl = boggy_pose_draw(dl, present_models[i], present_blocks[i], PRESENT_ANIM, present_progress, present_pos[i],
                             present_yaw + i * 1365.0f, PRESENT_SCALE, 1.0f, present_gfx[buffer][i], PRESENT_GFX_MAX,
                             present_mtx[buffer][i], PRESENT_MTX_MAX);
    }
    return dl;
}

// The carried presents in the HUD, right-aligned in the column under the feather and Mumbo
// Token counters (where Chimpy's orange goes in Jungle Japes), fading in when taken and out when
// handed over.
#define HUD_PRESENT_SIZE 32
#define HUD_RIGHT_MARGIN 56
#define HUD_Y (96 + 3 * 36)
#define HUD_FADE_STEP 0x20

Gfx *jiggy_boggy_hud_draw(Gfx *dl) {
    s32 x = D_global_asm_80744490 - HUD_RIGHT_MARGIN;
    s32 started = FALSE;
    s32 i;

    if (boggy_state != BOGGY_READY || cc_number_of_players != 1) {
        return dl;
    }
    for (i = PRESENT_COUNT - 1; i >= 0; i--) {
        s32 carrying = boggy_spot != NULL && present_carried(i);
        s32 width = hud_present_width[i];
        s32 height = hud_present_height[i];

        if (carrying) {
            hud_present_alpha[i] = hud_present_alpha[i] + HUD_FADE_STEP > 0xFF ? 0xFF : hud_present_alpha[i] + HUD_FADE_STEP;
        } else {
            hud_present_alpha[i] = hud_present_alpha[i] - HUD_FADE_STEP < 0 ? 0 : hud_present_alpha[i] - HUD_FADE_STEP;
        }
        if (hud_present_alpha[i] == 0 || width == 0) {
            continue;
        }
        if (!started) {
            dl = alignHUDGameplay(dl, ALIGN_RIGHT);
            gDPPipeSync(dl++);
            gDPSetCycleType(dl++, G_CYC_1CYCLE);
            gDPSetTexturePersp(dl++, G_TP_NONE);
            gDPSetTextureLUT(dl++, G_TT_NONE);
            gDPSetTextureFilter(dl++, G_TF_POINT);
            gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            // The sprite's own colors, its alpha scaled by the primitive's.
            gDPSetCombineLERP(dl++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0);
            started = TRUE;
        }
        x -= HUD_PRESENT_SIZE;
        gDPPipeSync(dl++);
        gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, hud_present_alpha[i]);
        gDPLoadTextureTile(dl++, hud_present_texels[i], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, height,
            0, 0, width - 1, height - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(dl++,
            x << 2, HUD_Y << 2, (x + HUD_PRESENT_SIZE) << 2, (HUD_Y + HUD_PRESENT_SIZE) << 2,
            G_TX_RENDERTILE,
            0, 0, (width << 10) / HUD_PRESENT_SIZE, (height << 10) / HUD_PRESENT_SIZE);
    }
    if (started) {
        gDPPipeSync(dl++);
        dl = popHUD(dl, ALIGN_RIGHT);
    }
    return dl;
}
