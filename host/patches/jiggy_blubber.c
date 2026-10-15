// Captain Blubber: Treasure Trove Cove's hippo pirate, read from the player's Banjo-Kazooie ROM,
// in Gloomy Galleon, where the two worlds have collided; with his gold quest.
//
// Like chBlubber (src/TTC/ch/blubber.c), Blubber (ASSET_370_MODEL_BLUBBER) paces up and down
// (his walk, with his footfalls: SFX_26 at 0.14 and 0.68 through it) and now and then stops to
// cry (ASSET_B6_ANIM_BLUBBER_CRY, SFX_83_BLUBBER_CRYING three times through it, as his state 2
// plays it), on the open floor by where the player comes into the level.
//
// His gold bullions (ASSET_3C7_MODEL_GOLD_BULLION) are scattered round the level: one by
// Cranky's, one on the east side by Nipper (jiggy_nipper.c) and one on the lighthouse's island.
// Touching one takes it, with Banjo-Kazooie's gold bullion jingle (COMUSIC_2B_DING_B, as
// __chLevelCollectible_collide plays it); the bullions carried show in the HUD (Banjo-Kazooie's
// gold bullion sprite, 0x80F) until they're handed over. DK64 can't carry them in the player's
// hands, so they're kept as items, as Banjo-Kazooie's ITEM_18_GOLD_BULLIONS is.
//
// When the player first comes near he tells them what he's lost, in a text box. Coming back with
// bullions, he takes them all: a line for each one back; with the last, he dances
// (ASSET_B7_ANIM_BLUBBER_DANCE, with SFX_80 clacks and SFX_8C's "wahey" as his state 4 plays them)
// to Banjo-Kazooie's puzzle solved fanfare, holding the player in the game's cutscene lock, and
// then a Golden Banana (drawn as a Jiggy) appears beside him with Banjo-Kazooie's Jiggy jingle.
// After that he dances on, happy. His repeats follow Bottles' reminder rule (jiggy_bottles.c):
// only when the player stands by him, only after they've been 250 units off since, and at most
// once a minute.
//
// The Golden Banana is jiggy_rewards.c's reward 0x41 (Gloomy Galleon's range, 0x40-0x4F in
// docs/GAME_PLAN.md). The bullions taken and handed over and his first talk are kept with the
// DK64 save file (jiggy_save_get/set, key 0x53000000 | map: Galleon's prefix, character 0).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his
// text box and the carried bullions in the HUD pass (jiggy_blubber_hud_draw, patches_ui.c).
// Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define BLUBBER_MODEL 0x370    // ASSET_370_MODEL_BLUBBER
#define BULLION_MODEL 0x3C7    // ASSET_3C7_MODEL_GOLD_BULLION
#define BLUBBER_PORTRAIT 0x7F1 // ASSET_7F1_SPRITE_BLUBBER
#define BULLION_SPRITE 0x80F   // ASSET_80F_SPRITE_LIVE_GOLD_BULLION: a 32x32 bullion.
// Blubber converts to 28632 bytes, with 36 bones and 56 pose commands; a bullion to 4208 bytes,
// with no bones and 6 commands.
#define BLUBBER_BLOCK_MAX 0x7000
#define BLUBBER_GFX_MAX 96
#define BLUBBER_MTX_MAX 40
#define BULLION_BLOCK_MAX 0x1100
#define BULLION_GFX_MAX 16
#define BULLION_MTX_MAX 4
#define BULLION_COUNT 3

// Banjo-Kazooie animations (gChBlubberAnimations) and how long each plays, in seconds.
#define BLUBBER_ANIM_WALK 0xB5  // ASSET_B5_ANIM_BLUBBER_WALK
#define BLUBBER_ANIM_CRY 0xB6   // ASSET_B6_ANIM_BLUBBER_CRY
#define BLUBBER_ANIM_DANCE 0xB7 // ASSET_B7_ANIM_BLUBBER_DANCE
#define BLUBBER_WALK_SECONDS 2.0f
#define BLUBBER_CRY_SECONDS 2.0f
#define BLUBBER_DANCE_SECONDS 1.4f
// The chance a frame he stops walking to cry (subaddie_maybe_set_state_position_direction's
// 0.007), and the chance at the end of a cry that he walks on (0.78).
#define BLUBBER_CRY_CHANCE 0.007f
#define BLUBBER_WALK_ON_CHANCE 0.78f
// His walk, in DK64 units a second (his animation's stride, scaled), and his turn.
#define BLUBBER_WALK_SPEED 22.0f
#define BLUBBER_TURN_RATE 0.12f
// Dances in his celebration before the Golden Banana appears.
#define BLUBBER_CELEBRATE_DANCES 2

// Banjo-Kazooie sounds (sfx_e) and how chBlubber plays them.
#define BLUBBER_SFX_CRYING 0x83   // SFX_83_BLUBBER_CRYING: 17000/32767, pitch 0.95, 0.93, 0.91
#define BLUBBER_SFX_STEP 0x26     // SFX_26_BANJO_LANDING_09: each footfall, 11000/32767, 0.75-0.85
#define BLUBBER_SFX_CLACK 0x80    // SFX_80_YUMYUM_CLACK: dancing, 13000/32767, 0.9
#define BLUBBER_SFX_WAHEY 0x8C    // SFX_8C_BOGGY_WAHEY: dancing, 27000/32767, 1.0
// His voice in the text box (gczoombox), one of these at 28000/32767, pitch 1.1.
static const u16 blubber_talk_sfx[] = {0xF5, 0xF6, 0xF7, 0xF8};
#define BLUBBER_TALK_SFX_EVERY 5 // Characters typed out between sounds.
// Banjo-Kazooie's music: a bullion taken (COMUSIC_2B_DING_B, 0x7FFF/32767), the puzzle solved
// fanfare (COMUSIC_2D, 0x7FFF/32767) and a Jiggy appearing (COMUSIC_3D_JIGGY_SPAWN).
#define JINGLE_DING_B 0x2B
#define JINGLE_PUZZLE_SOLVED 0x2D
#define JINGLE_JIGGY_SPAWN 0x3D
// Banjo-Kazooie fades these out between 1250 and 2500 units; scaled to DK64 units.
#define SFX_NEAR 310.0f
#define SFX_FAR 625.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// He talks when the player comes this close (chBlubber's 250 units, scaled, and a little more
// for DK64's bigger Kongs), on the ground; a repeat needs the player to have been this far off
// since, and comes at most this often (jiggy_bottles.c's reminder rule).
#define BLUBBER_TALK_RADIUS 75.0f
#define BLUBBER_REARM_RADIUS 250.0f
#define BLUBBER_REPEAT_FRAMES (60 * 30) // A minute.
#define BLUBBER_TALK_HEIGHT 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing things on the floor under their spots: looked for from this far above the spot, and
// only trusted this close to the spot's height.
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f
// The player is kept this far from his middle (DK64 doesn't know he's there).
#define BLUBBER_RADIUS 26.0f
#define PLAYER_RADIUS 10.0f

// The bullions on the ground: they turn slowly, and are taken within this distance and height.
#define BULLION_SPIN_PER_FRAME 24.0f // 12-bit angle units.
#define BULLION_TAKE_RADIUS 24.0f
#define BULLION_TAKE_HEIGHT 40.0f
#define BULLION_HOVER 2.0f

// DK64 player states (Actor control_state) he talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

// His Golden Banana: jiggy_rewards.c's id (docs/GAME_PLAN.md), standing this high over the floor.
#define BLUBBER_REWARD 0x41
#define REWARD_HEIGHT 12.5f

// The text box (as jiggy_bottles.c draws it), in screen pixels.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 10
#define PORTRAIT_MAX_TEXELS (32 * 32)
// Frames 0 to 7 move his mouth (gczoombox's 8 talking frames for Blubber).
#define PORTRAIT_TALK_FRAMES 8
#define PORTRAIT_TICKS_PER_FRAME 2
#define TEXT_STYLE 6 // DK64's dialogue font (uppercase letters, digits and . , ! ? : ; ' - ( ) %).
#define TEXT_LINE_HEIGHT 17
#define TEXT_NAME_Y 6
#define TEXT_FIRST_LINE_Y 24
// printStyledText's y for a box row (see jiggy_bottles.c).
#define TEXT_Y(y) (((y) - 117) * 4 / 5 + 120)
#define TEXT_MAX_LINES 4
#define TEXT_LINE_MAX 48
#define TEXT_CHARS_PER_FRAME 1
#define BOX_FADE_STEP 0x30
#define BOX_ALPHA 0xC8

// The carried bullions in the HUD: the 32x32 sprite this big, side by side, in the right-hand
// column below the feather and Mumbo Token counters (as jiggy_chimpy.c shows its orange).
#define HUD_BULLION_MAX_TEXELS (32 * 32)
#define HUD_BULLION_SIZE 28
#define HUD_BULLION_GAP 2
#define HUD_RIGHT_MARGIN 56
#define HUD_Y (96 + 3 * 36)
#define HUD_FADE_STEP 0x20

// Where he is, map by map. Gloomy Galleon: on the open floor just east of where the player
// comes in from the lobby (2119, 1612, 2812), inside the bananas' trails (the nearest, a Lanky
// banana at (2118, 1612, 2975), 93 units from his Golden Banana's spot) and clear of the notes
// (jiggy_notes.c, the nearest 104 off). He paces between two spots 80 units apart. His bullions:
// by Cranky's, between the coins (1551, 1611, 4225) and the door (1676, 1610, 4138); at the east
// side's north end, past two Chunky bananas (3167, 1670, 3780) and (3345, 1670, 3801); and on the
// lighthouse's island, between its door (1924, 1461, 558) and the tag barrel (1843, 1463, 722).
// Each is snapped to the floor (or water) found under it.
typedef struct {
    Maps map;
    f32 ax, y, az;          // One end of his pacing (y is the floor, if it isn't found).
    f32 bx, bz;             // The other end.
    f32 reward_x, reward_z; // Where his Golden Banana appears.
    f32 bullions[BULLION_COUNT][3];
} BlubberPlacement;
static const BlubberPlacement blubber_spots[] = {
    {MAP_GALLEON, 2230.0f, 1610.0f, 2850.0f, 2300.0f, 2890.0f, 2200.0f, 2930.0f,
     {{1610.0f, 1610.0f, 4110.0f}, {3256.0f, 1670.0f, 3860.0f}, {1960.0f, 1458.0f, 660.0f}}},
};

// What he says. Each page is one press of A; lines are broken to fit the box when shown.
static const char *const blubber_ask[] = {
    "BOO HOO! ME GOLD! ME LOVELY GOLD IS GONE!",
    "A GREAT CLANKING SHARK SHOOK ME BULLIONS OVERBOARD!",
    "ONE BY CRANKY'S, ONE BY THAT NASTY CRAB, ONE BY THE LIGHTHOUSE.",
    "BRING 'EM BACK TO OL' BLUBBER, ME HEARTIES!",
};
static const char *const blubber_remind_3[] = {
    "SNIFF... ALL THREE BARS ARE STILL OUT THERE, MATEYS!",
};
static const char *const blubber_remind_2[] = {
    "TWO MORE BARS TO FIND, ME HEARTIES! SNIFF!",
};
static const char *const blubber_remind_1[] = {
    "JUST ONE MORE BAR! OL' BLUBBER CAN SMELL IT!",
};
static const char *const blubber_deliver_1[] = {
    "ME GOLD! HARR! THAT'S ONE BAR BACK IN OL' BLUBBER'S HOLD!",
};
static const char *const blubber_deliver_2[] = {
    "HARR HARR! TWO BARS! JUST ONE MORE, ME HEARTIES!",
};
static const char *const blubber_complete[] = {
    "ALL ME GOLD! HARR HARR HARR!",
    "HERE'S A SHINY JIGGY FOR YER TROUBLE, SHIPMATES!",
};
static const char *const blubber_thanks[] = {
    "HARR! OL' BLUBBER'S RICH AGAIN, THANKS TO YOU TWO!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): Galleon's prefix 0x53, character 0 (Blubber), and the map.
#define BLUBBER_SAVE_MAP_KEY(map) (0x53000000 | (0 << 16) | (u32)(map))
#define BLUBBER_SAVE_TAKEN(i) (0x1u << (i))     // Bullion i picked up.
#define BLUBBER_SAVE_DELIVERED(i) (0x10u << (i)) // Bullion i handed over.
#define BLUBBER_SAVE_MET 0x100
#define BLUBBER_SAVE_DONE 0x200 // All handed over and celebrated: his Golden Banana is due.

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern s16 D_global_asm_80744490; // Screen width.
extern s16 D_global_asm_80744494; // Screen height.
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_8076A0B3;
extern OSContPad D_global_asm_807ECD58; // Buttons held this frame, before the cutscene lock.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y and returns nonzero, or leaves it as it was.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// After it: whether there's water over that floor, writing the surface's height to *y if so (as
// the game's props check, func_global_asm_8064D0C4).
u8 func_global_asm_8066715C(f32 *y);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    BLUBBER_UNLOADED,
    BLUBBER_READY,
    BLUBBER_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or the portrait didn't convert.
} BlubberLoadState;

typedef enum {
    BLUBBER_WALK,      // Pacing to the other end (chBlubber's state 1).
    BLUBBER_CRY,       // Standing, crying (state 2).
    BLUBBER_CELEBRATE, // All his gold back: dancing while the player's held.
    BLUBBER_DANCE,     // After: dancing on (state 4).
} BlubberAction;

typedef enum {
    TALK_ASK,
    TALK_REMIND,
    TALK_DELIVER,
    TALK_COMPLETE,
    TALK_THANKS,
} BlubberTalk;

static u8 blubber_block[BLUBBER_BLOCK_MAX] __attribute__((aligned(16)));
static u8 bullion_block[BULLION_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx blubber_gfx[2][BLUBBER_GFX_MAX];
static Mtx blubber_mtx[2][BLUBBER_MTX_MAX];
static Gfx bullion_gfx[2][BULLION_COUNT][BULLION_GFX_MAX];
static Mtx bullion_mtx[2][BULLION_COUNT][BULLION_MTX_MAX];
static JiggyBkPose blubber_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static u16 hud_bullion_texels[HUD_BULLION_MAX_TEXELS] __attribute__((aligned(8)));
static s32 hud_bullion_width; // 0 when the sprite didn't convert: no HUD icons.
static s32 hud_bullion_height;
static s32 hud_bullion_alpha;
static s32 hud_bullion_shown; // How many were last drawn (kept while they fade out).
static BlubberLoadState blubber_state = BLUBBER_UNLOADED;

static s32 blubber_last_map = -1;
static u32 blubber_last_timer;
static const BlubberPlacement *blubber_spot; // This map's, or NULL.
static s32 blubber_placed;
static f32 blubber_pos[3];
static f32 blubber_ends[2][3]; // His pacing's two ends, on the floor.
static s32 blubber_to_b;
static f32 blubber_yaw; // 12-bit angle units, as a float so the turn eases.
static BlubberAction blubber_action;
static f32 blubber_progress; // 0 to 1 through the action's animation.
static s32 blubber_dances;
static s32 blubber_holding;  // The player's held from his complete talk until the Golden Banana's due.
static s32 blubber_armed;    // The player has been away since his last talk.
static u32 blubber_last_talk; // object_timer when his last talk began.
static u32 blubber_saved;     // BLUBBER_SAVE_* bits.
static f32 bullion_pos[BULLION_COUNT][3];
static f32 bullion_yaw;
static f32 reward_spot[3];
static s32 reward_jingle;
static u32 blubber_random_state = 0x424C5542;

// The dialogue (as jiggy_bottles.c keeps it).
static BlubberTalk talk_kind;
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static s32 talk_open;
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_length; // Characters in the page, counting a space between lines.
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_a_held = TRUE;

static f32 blubber_random(void) {
    blubber_random_state = blubber_random_state * 1664525 + 1013904223;
    return (blubber_random_state >> 8) * (1.0f / 16777216.0f);
}

static s32 blubber_build(u32 model, u8 *block, u32 capacity) {
    s32 size = jiggy_bk_model_build(model, block, capacity);

    return size > 0 && size <= (s32)capacity;
}

static void blubber_load(void) {
    s32 count;
    s32 got;
    s32 i;

    blubber_state = BLUBBER_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    if (!blubber_build(BLUBBER_MODEL, blubber_block, sizeof(blubber_block)) ||
        !blubber_build(BULLION_MODEL, bullion_block, sizeof(bullion_block))) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(BLUBBER_PORTRAIT);
    if (count <= 0) {
        return;
    }
    if (count > PORTRAIT_MAX_FRAMES) {
        count = PORTRAIT_MAX_FRAMES;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 width, height;

        got = jiggy_bk_sprite_frame_rgba16(BLUBBER_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
        width = (got >> 16) & 0xFFFF;
        height = got & 0xFFFF;
        if (got <= 0 || width == 0 || height == 0 || width * height > PORTRAIT_MAX_TEXELS) {
            continue;
        }
        portrait_width[portrait_frames] = width;
        portrait_height[portrait_frames] = height;
        portrait_frames++;
    }
    if (portrait_frames == 0) {
        return;
    }
    // The bullion for the HUD; without it there are no icons, and the rest still works.
    hud_bullion_width = 0;
    got = jiggy_bk_sprite_frame_rgba16(BULLION_SPRITE, 0, hud_bullion_texels, sizeof(hud_bullion_texels));
    if (got > 0) {
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        if (width > 0 && height > 0 && width * height <= HUD_BULLION_MAX_TEXELS) {
            hud_bullion_width = width;
            hud_bullion_height = height;
        }
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden (chBlubber's draw hides selector 4 too).
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        blubber_pose.selectors[i] = 0;
    }
    blubber_pose.selectors[1] = 1;
    blubber_state = BLUBBER_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// A Banjo-Kazooie sound from where Blubber is, fading out with distance from the player.
static void blubber_sfx(Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(blubber_pos[0], blubber_pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

// True when `at` is passed going from `from` to `to` through an animation.
static s32 blubber_passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

static void blubber_start(BlubberAction action, f32 progress) {
    blubber_action = action;
    blubber_progress = progress;
}

// The floor under (x, z), looked for from above `y`; `y` if there's none near. Where water
// covers it near `y`, the water's surface instead (with `water`).
static f32 blubber_floor(f32 x, f32 y, f32 z, s32 water) {
    f32 probe = y + FLOOR_PROBE;
    f32 floor = probe;
    f32 surface;
    f32 found = y;

    func_global_asm_80667110(x, z, &floor);
    if (floor != probe && floor - y < FLOOR_MAX_STEP && y - floor < FLOOR_MAX_STEP) {
        found = floor;
    }
    if (water && func_global_asm_8066715C(&surface) && surface > found && surface - y < FLOOR_MAX_STEP) {
        found = surface;
    }
    return found;
}

static u32 blubber_count(u32 bits) {
    u32 n = 0;
    s32 i;

    for (i = 0; i < BULLION_COUNT; i++) {
        if (bits & (1u << i)) {
            n++;
        }
    }
    return n;
}

static u32 blubber_taken_bits(void) {
    return blubber_saved & 0x7;
}

static u32 blubber_delivered_bits(void) {
    return (blubber_saved >> 4) & 0x7;
}

// Bullions taken but not yet handed over.
static u32 blubber_carried(void) {
    return blubber_count(blubber_taken_bits() & ~blubber_delivered_bits());
}

// Places him (pacing, or dancing once his gold's back), his bullions and his Golden Banana's spot.
static void blubber_place(Actor *player) {
    s32 i;

    blubber_ends[0][0] = blubber_spot->ax;
    blubber_ends[0][1] = blubber_floor(blubber_spot->ax, blubber_spot->y, blubber_spot->az, FALSE);
    blubber_ends[0][2] = blubber_spot->az;
    blubber_ends[1][0] = blubber_spot->bx;
    blubber_ends[1][1] = blubber_floor(blubber_spot->bx, blubber_ends[0][1], blubber_spot->bz, FALSE);
    blubber_ends[1][2] = blubber_spot->bz;
    blubber_pos[0] = blubber_ends[0][0];
    blubber_pos[1] = blubber_ends[0][1];
    blubber_pos[2] = blubber_ends[0][2];
    blubber_to_b = TRUE;
    for (i = 0; i < BULLION_COUNT; i++) {
        bullion_pos[i][0] = blubber_spot->bullions[i][0];
        bullion_pos[i][1] = blubber_floor(blubber_spot->bullions[i][0], blubber_spot->bullions[i][1],
                                          blubber_spot->bullions[i][2], TRUE) + BULLION_HOVER;
        bullion_pos[i][2] = blubber_spot->bullions[i][2];
    }
    reward_spot[0] = blubber_spot->reward_x;
    reward_spot[1] = blubber_floor(blubber_spot->reward_x, blubber_ends[0][1], blubber_spot->reward_z, FALSE);
    reward_spot[2] = blubber_spot->reward_z;
    reward_jingle = FALSE;
    blubber_holding = FALSE;
    blubber_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, blubber_pos[0], blubber_pos[2]) & 0xFFF;
    blubber_start((blubber_saved & BLUBBER_SAVE_DONE) ? BLUBBER_DANCE : BLUBBER_WALK, blubber_random());
    blubber_placed = TRUE;
}

// Breaks the page into lines that fit the box, a word at a time (as jiggy_bottles.c does).
static void talk_set_page(s32 page) {
    const char *text = talk_pages[page];
    s32 text_width = BOX_WIDTH - 2 * BOX_BORDER - BOX_PAD - PORTRAIT_SCALE * 32 - 2 * BOX_PAD;
    char candidate[TEXT_LINE_MAX];
    s32 line = 0;
    s32 length = 0;

    talk_page = page;
    talk_lines[0][0] = '\0';
    while (*text != '\0' && line < TEXT_MAX_LINES) {
        const char *word = text;
        s32 word_length = 0;
        s32 i;
        s32 n;

        while (word[word_length] != '\0' && word[word_length] != ' ') {
            word_length++;
        }
        n = 0;
        for (i = 0; talk_lines[line][i] != '\0'; i++) {
            candidate[n++] = talk_lines[line][i];
        }
        if (n > 0) {
            candidate[n++] = ' ';
        }
        for (i = 0; i < word_length && n < TEXT_LINE_MAX - 1; i++) {
            candidate[n++] = word[i];
        }
        candidate[n] = '\0';
        if (talk_lines[line][0] != '\0' && getCenterOfString(TEXT_STYLE, (u8 *)candidate) > text_width) {
            // The word starts the next line.
            line++;
            if (line >= TEXT_MAX_LINES) {
                break;
            }
            talk_lines[line][0] = '\0';
            continue;
        }
        for (i = 0; i <= n; i++) {
            talk_lines[line][i] = candidate[i];
        }
        text += word_length;
        while (*text == ' ') {
            text++;
        }
    }
    talk_line_count = line < TEXT_MAX_LINES ? line + 1 : TEXT_MAX_LINES;
    for (line = 0; line < talk_line_count; line++) {
        s32 i = 0;

        while (talk_lines[line][i] != '\0') {
            i++;
        }
        length += i + 1;
    }
    talk_length = length;
    talk_shown = 0;
    talk_ticks = 0;
}

// The player can't move while he talks (as jiggy_bottles.c holds them).
static void blubber_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void blubber_save(u32 bits) {
    blubber_saved |= bits;
    jiggy_save_set(current_file, BLUBBER_SAVE_MAP_KEY(current_map), blubber_saved);
}

static void talk_start(BlubberTalk kind) {
    u32 delivered = blubber_count(blubber_delivered_bits());
    u32 left = BULLION_COUNT - blubber_count(blubber_taken_bits() | blubber_delivered_bits());

    talk_kind = kind;
    switch (kind) {
        case TALK_ASK:
            talk_pages = blubber_ask;
            talk_page_count = ARRAY_COUNT_OF(blubber_ask);
            break;
        case TALK_REMIND:
            talk_pages = left >= 3 ? blubber_remind_3 : left == 2 ? blubber_remind_2 : blubber_remind_1;
            talk_page_count = 1;
            break;
        case TALK_DELIVER:
            talk_pages = delivered >= 2 ? blubber_deliver_2 : blubber_deliver_1;
            talk_page_count = 1;
            break;
        case TALK_COMPLETE:
            talk_pages = blubber_complete;
            talk_page_count = ARRAY_COUNT_OF(blubber_complete);
            break;
        default:
            talk_pages = blubber_thanks;
            talk_page_count = ARRAY_COUNT_OF(blubber_thanks);
            break;
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    blubber_lock_player(TRUE);
}

// The text box's frame: A to type out the rest of the page or go on to the next, and the
// talking sounds while it types.
static void talk_update(void) {
    s32 a_held = (D_global_asm_807ECD58.button & A_BUTTON) != 0;
    s32 a_pressed = a_held && !talk_a_held;
    s32 before = talk_shown;

    talk_a_held = a_held;
    talk_ticks++;
    if (talk_alpha < 0xFF) {
        talk_alpha += BOX_FADE_STEP;
        if (talk_alpha > 0xFF) {
            talk_alpha = 0xFF;
        }
    }
    if (talk_shown < talk_length) {
        if (a_pressed) {
            talk_shown = talk_length;
        } else {
            talk_shown += TEXT_CHARS_PER_FRAME;
        }
        if (talk_shown / BLUBBER_TALK_SFX_EVERY != before / BLUBBER_TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 pick = (u32)(blubber_random() * ARRAY_COUNT_OF(blubber_talk_sfx)) % ARRAY_COUNT_OF(blubber_talk_sfx);

            jiggy_bk_sfx_play(blubber_talk_sfx[pick], 28000.0f / 32767.0f, 1.1f);
        }
        return;
    }
    if (!a_pressed) {
        return;
    }
    if (talk_page + 1 < talk_page_count) {
        talk_set_page(talk_page + 1);
        return;
    }
    talk_open = FALSE;
    if (talk_kind == TALK_COMPLETE) {
        // He dances for joy; the player's held until his Golden Banana appears.
        blubber_holding = TRUE;
        blubber_dances = 0;
        blubber_start(BLUBBER_CELEBRATE, 0.0f);
        jiggy_bk_jingle_play(JINGLE_PUZZLE_SOLVED, 1.0f);
        return;
    }
    // The player can move again.
    blubber_lock_player(FALSE);
}

// Whether the player is close enough to him, and at his height, to talk.
static s32 blubber_player_near(Actor *player, f32 radius) {
    f32 dy = blubber_pos[1] - player->y_position;

    return distance_xz(blubber_pos[0], blubber_pos[2], player->x_position, player->z_position) <= radius &&
           dy >= -BLUBBER_TALK_HEIGHT && dy <= BLUBBER_TALK_HEIGHT;
}

// Starts a talk when the player stops by him (see the top of this file).
static void blubber_check_talk(Actor *player) {
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;
    s32 standing = player->control_state == STATE_IDLE;
    s32 walking = player->control_state == STATE_WALK;
    s16 to_blubber;
    u32 i;

    if (!blubber_armed) {
        blubber_armed = !blubber_player_near(player, BLUBBER_REARM_RADIUS) &&
                        (!(blubber_saved & BLUBBER_SAVE_MET) || object_timer - blubber_last_talk >= BLUBBER_REPEAT_FRAMES);
    }
    if (!blubber_player_near(player, BLUBBER_TALK_RADIUS) || !grounded) {
        return;
    }
    if (blubber_carried() > 0 && (standing || walking)) {
        // Gold: he takes all the player has, whatever he said last.
        u32 bits = BLUBBER_SAVE_MET;

        for (i = 0; i < BULLION_COUNT; i++) {
            if (blubber_taken_bits() & (1u << i)) {
                bits |= BLUBBER_SAVE_DELIVERED(i);
            }
        }
        blubber_save(bits);
        if (blubber_count(blubber_delivered_bits()) >= BULLION_COUNT) {
            talk_start(TALK_COMPLETE);
        } else {
            jiggy_bk_jingle_play(JINGLE_DING_B, 1.0f);
            talk_start(TALK_DELIVER);
        }
    } else if (blubber_armed && (standing || (walking && !(blubber_saved & BLUBBER_SAVE_MET)))) {
        talk_start((blubber_saved & BLUBBER_SAVE_DONE) ? TALK_THANKS : (blubber_saved & BLUBBER_SAVE_MET) ? TALK_REMIND : TALK_ASK);
        blubber_save(BLUBBER_SAVE_MET);
    } else {
        return;
    }
    blubber_armed = FALSE;
    blubber_last_talk = object_timer;
    // The player turns to him (as Bottles turns them).
    to_blubber = func_global_asm_80665DE0(blubber_pos[0], blubber_pos[2], player->x_position, player->z_position);
    player->y_rotation = to_blubber & 0xFFF;
    player->unkEE = to_blubber & 0xFFF;
}

// Turns a share of the way to face (x, z), over `frames` frames.
static void blubber_turn_to(f32 x, f32 z, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, z, blubber_pos[0], blubber_pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - blubber_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        blubber_yaw += turn * BLUBBER_TURN_RATE;
        if (blubber_yaw < 0.0f) {
            blubber_yaw += 4096.0f;
        } else if (blubber_yaw >= 4096.0f) {
            blubber_yaw -= 4096.0f;
        }
    }
}

// DK64 doesn't know he's there: he keeps the player out of his middle.
static void blubber_push_player(Actor *player) {
    f32 dx = player->x_position - blubber_pos[0];
    f32 dz = player->z_position - blubber_pos[2];
    f32 reach = BLUBBER_RADIUS + PLAYER_RADIUS;
    f32 distance = _sqrtf(dx * dx + dz * dz);

    if (distance >= reach || distance < 0.01f || player->y_position > blubber_pos[1] + BLUBBER_TALK_HEIGHT) {
        return;
    }
    player->x_position = blubber_pos[0] + dx * reach / distance;
    player->z_position = blubber_pos[2] + dz * reach / distance;
}

// The bullions lying about: taken when the player touches one.
static void bullions_update(Actor *player, u32 frames) {
    s32 i;

    bullion_yaw += BULLION_SPIN_PER_FRAME * frames;
    while (bullion_yaw >= 4096.0f) {
        bullion_yaw -= 4096.0f;
    }
    for (i = 0; i < BULLION_COUNT; i++) {
        f32 dy = player->y_position - bullion_pos[i][1];

        if ((blubber_saved & (BLUBBER_SAVE_TAKEN(i) | BLUBBER_SAVE_DELIVERED(i))) || dy <= -BULLION_TAKE_HEIGHT ||
            dy >= BULLION_TAKE_HEIGHT ||
            distance_xz(bullion_pos[i][0], bullion_pos[i][2], player->x_position, player->z_position) > BULLION_TAKE_RADIUS) {
            continue;
        }
        blubber_save(BLUBBER_SAVE_TAKEN(i));
        jiggy_bk_jingle_play(JINGLE_DING_B, 1.0f);
    }
}

// Paces to the end of his walk he's heading for; at it, turns round for the other.
static void blubber_walk(Actor *player, f32 seconds, u32 frames) {
    f32 *target = blubber_ends[blubber_to_b ? 1 : 0];
    f32 step = BLUBBER_WALK_SPEED * seconds;
    f32 distance = distance_xz(target[0], target[2], blubber_pos[0], blubber_pos[2]);

    blubber_turn_to(target[0], target[2], frames);
    if (distance <= step) {
        blubber_pos[0] = target[0];
        blubber_pos[1] = target[1];
        blubber_pos[2] = target[2];
        blubber_to_b = !blubber_to_b;
        return;
    }
    blubber_pos[0] += (target[0] - blubber_pos[0]) * step / distance;
    blubber_pos[1] += (target[1] - blubber_pos[1]) * step / distance;
    blubber_pos[2] += (target[2] - blubber_pos[2]) * step / distance;
}

// A frame of his behavior (__chBlubber_updateFunc), `frames` game frames on.
static void blubber_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = blubber_progress;
    s32 near = blubber_player_near(player, BLUBBER_TALK_RADIUS * 2.0f);

    if (frames == 0) {
        return;
    }
    bullions_update(player, frames);
    blubber_push_player(player);
    if (!talk_open && !blubber_holding) {
        blubber_check_talk(player);
    }

    switch (blubber_action) {
        case BLUBBER_WALK:
            blubber_progress += seconds / BLUBBER_WALK_SECONDS;
            if (blubber_passed(before, blubber_progress, 0.14f) || blubber_passed(before, blubber_progress, 0.68f)) {
                blubber_sfx(player, BLUBBER_SFX_STEP, 11000.0f / 32767.0f, 0.75f + blubber_random() * 0.1f);
            }
            while (blubber_progress >= 1.0f) {
                blubber_progress -= 1.0f;
            }
            // He stops for the player when they're near (or he's talking), facing them.
            if (talk_open || near) {
                blubber_start(BLUBBER_CRY, 0.0f);
                break;
            }
            blubber_walk(player, seconds, frames);
            if (blubber_random() < BLUBBER_CRY_CHANCE * frames) {
                blubber_start(BLUBBER_CRY, 0.0f);
            }
            break;
        case BLUBBER_CRY:
            blubber_turn_to(player->x_position, player->z_position, frames);
            blubber_progress += seconds / BLUBBER_CRY_SECONDS;
            if (blubber_passed(before, blubber_progress, 0.3f)) {
                blubber_sfx(player, BLUBBER_SFX_CRYING, 17000.0f / 32767.0f, 0.95f);
            }
            if (blubber_passed(before, blubber_progress, 0.53f)) {
                blubber_sfx(player, BLUBBER_SFX_CRYING, 17000.0f / 32767.0f, 0.93f);
            }
            if (blubber_passed(before, blubber_progress, 0.72f)) {
                blubber_sfx(player, BLUBBER_SFX_CRYING, 17000.0f / 32767.0f, 0.91f);
            }
            if (blubber_progress >= 1.0f) {
                blubber_progress -= 1.0f;
                if (!talk_open && !near && blubber_random() < BLUBBER_WALK_ON_CHANCE) {
                    blubber_start(BLUBBER_WALK, 0.0f);
                }
            }
            break;
        case BLUBBER_CELEBRATE:
        case BLUBBER_DANCE:
            blubber_turn_to(player->x_position, player->z_position, frames);
            blubber_progress += seconds / BLUBBER_DANCE_SECONDS;
            if (blubber_passed(before, blubber_progress, 0.3f) && !talk_open) {
                blubber_sfx(player, BLUBBER_SFX_WAHEY, 27000.0f / 32767.0f, 1.0f);
            }
            if (blubber_passed(before, blubber_progress, 0.28f) ||
                (blubber_passed(before, blubber_progress, 0.78f) && !talk_open)) {
                blubber_sfx(player, BLUBBER_SFX_CLACK, 13000.0f / 32767.0f, 0.9f);
            }
            if (blubber_progress >= 1.0f) {
                blubber_progress -= 1.0f;
                blubber_dances++;
                if (blubber_action == BLUBBER_CELEBRATE) {
                    blubber_lock_player(TRUE);
                    if (blubber_dances >= BLUBBER_CELEBRATE_DANCES) {
                        // Done: his Golden Banana appears and the player can move again.
                        blubber_save(BLUBBER_SAVE_DONE);
                        blubber_holding = FALSE;
                        blubber_lock_player(FALSE);
                        reward_jingle = TRUE;
                        blubber_armed = FALSE;
                        blubber_last_talk = object_timer;
                        blubber_start(BLUBBER_DANCE, 0.0f);
                    }
                }
            }
            break;
    }
    if (blubber_saved & BLUBBER_SAVE_DONE) {
        if (!jiggy_reward_update(BLUBBER_REWARD, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], player,
                                 frames) &&
            reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        }
        reward_jingle = FALSE;
    }
    if (talk_open) {
        talk_update();
    }
}

// Draws a built model at a spot, turned to `yaw` (12-bit angle units).
static Gfx *blubber_pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, const f32 pos[3], f32 yaw,
                              Gfx *gfx, u32 gfx_capacity, Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 scale[4][4];

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, blubber_pose.world);
    blubber_pose.world[3][0] = pos[0];
    blubber_pose.world[3][1] = pos[1];
    blubber_pose.world[3][2] = pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    blubber_pose.model = model;
    blubber_pose.block = block;
    blubber_pose.animation = animation;
    blubber_pose.progress = progress;
    blubber_pose.gfx = gfx;
    blubber_pose.gfx_capacity = gfx_capacity;
    blubber_pose.mtx = mtx;
    blubber_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&blubber_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Blubber and his bullions once a frame, with the player.
Gfx *jiggy_blubber_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    s32 animation;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): he's back where he was.
    if ((s32)current_map != blubber_last_map || object_timer < blubber_last_timer) {
        // Leaving the map mid-talk (or mid-dance) lets the player go.
        if (talk_open || blubber_holding) {
            blubber_lock_player(FALSE);
            talk_open = FALSE;
            blubber_holding = FALSE;
        }
        blubber_last_map = current_map;
        blubber_placed = FALSE;
        blubber_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(blubber_spots); i++) {
            if (blubber_spots[i].map == current_map) {
                blubber_spot = &blubber_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        blubber_saved = blubber_spot != NULL ? jiggy_save_get(current_file, BLUBBER_SAVE_MAP_KEY(current_map)) : 0;
        // A celebration cut short (the map left mid-dance) is finished now.
        if (blubber_spot != NULL && blubber_count(blubber_delivered_bits()) >= BULLION_COUNT &&
            !(blubber_saved & BLUBBER_SAVE_DONE)) {
            blubber_save(BLUBBER_SAVE_DONE);
        }
    }
    if (blubber_spot == NULL) {
        blubber_last_timer = object_timer;
        return dl;
    }
    if (blubber_state == BLUBBER_UNLOADED) {
        blubber_load();
    }
    if (blubber_state != BLUBBER_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!blubber_placed) {
        blubber_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        blubber_place(actor);
        blubber_armed = !blubber_player_near(actor, BLUBBER_REARM_RADIUS);
        blubber_last_talk = object_timer - BLUBBER_REPEAT_FRAMES;
    }
    frames = object_timer - blubber_last_timer;
    blubber_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && !blubber_holding) {
        frames = 0;
    }
    blubber_update(actor, frames);

    switch (blubber_action) {
        case BLUBBER_WALK:
            animation = BLUBBER_ANIM_WALK;
            break;
        case BLUBBER_CRY:
            animation = BLUBBER_ANIM_CRY;
            break;
        default:
            animation = BLUBBER_ANIM_DANCE;
            break;
    }
    dl = blubber_pose_draw(dl, BLUBBER_MODEL, blubber_block, animation, blubber_progress, blubber_pos, blubber_yaw,
                           blubber_gfx[buffer], BLUBBER_GFX_MAX, blubber_mtx[buffer], BLUBBER_MTX_MAX);
    for (i = 0; i < BULLION_COUNT; i++) {
        if (blubber_saved & (BLUBBER_SAVE_TAKEN(i) | BLUBBER_SAVE_DELIVERED(i))) {
            continue;
        }
        dl = blubber_pose_draw(dl, BULLION_MODEL, bullion_block, -1, 0.0f, bullion_pos[i], bullion_yaw + i * 1365.0f,
                               bullion_gfx[buffer][i], BULLION_GFX_MAX, bullion_mtx[buffer][i], BULLION_MTX_MAX);
    }
    return dl;
}

// A filled rectangle in the primitive color, blended over what's there.
static Gfx *box_fill(Gfx *dl, s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, r, g, b, a);
    gDPFillRectangle(dl++, x0, y0, x1, y1);
    return dl;
}

// The carried bullions in the HUD, side by side, HUD_BULLION_SIZE tall, their right edge
// HUD_RIGHT_MARGIN in from the screen's; they fade in when taken and out when handed over.
static Gfx *hud_bullions_draw(Gfx *dl) {
    s32 carried = blubber_spot != NULL ? (s32)blubber_carried() : 0;
    s32 display_width;
    s32 step;
    s32 x;
    s32 i;

    if (hud_bullion_width == 0) {
        return dl;
    }
    if (carried > 0) {
        hud_bullion_shown = carried;
        hud_bullion_alpha = hud_bullion_alpha + HUD_FADE_STEP > 0xFF ? 0xFF : hud_bullion_alpha + HUD_FADE_STEP;
    } else {
        hud_bullion_alpha = hud_bullion_alpha - HUD_FADE_STEP < 0 ? 0 : hud_bullion_alpha - HUD_FADE_STEP;
    }
    if (hud_bullion_alpha == 0 || hud_bullion_shown == 0) {
        return dl;
    }
    display_width = hud_bullion_width * HUD_BULLION_SIZE / hud_bullion_height;
    // Texels a pixel, in 5.10 fixed point.
    step = (hud_bullion_height << 10) / HUD_BULLION_SIZE;

    dl = alignHUDGameplay(dl, ALIGN_RIGHT);
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    // The sprite's own colors, its alpha scaled by the primitive's.
    gDPSetCombineLERP(dl++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, hud_bullion_alpha);
    gDPLoadTextureTile(dl++, hud_bullion_texels, G_IM_FMT_RGBA, G_IM_SIZ_16b,
        hud_bullion_width, hud_bullion_height,
        0, 0, hud_bullion_width - 1, hud_bullion_height - 1,
        0,
        G_TX_CLAMP, G_TX_CLAMP,
        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    for (i = 0; i < hud_bullion_shown; i++) {
        x = D_global_asm_80744490 - HUD_RIGHT_MARGIN - display_width - i * (display_width + HUD_BULLION_GAP);
        gSPTextureRectangle(dl++,
            x << 2, HUD_Y << 2, (x + display_width) << 2, (HUD_Y + HUD_BULLION_SIZE) << 2,
            G_TX_RENDERTILE,
            0, 0, step, step);
    }
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_RIGHT);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the carried
// bullions, and his text box while he talks (drawn as jiggy_bottles_hud_draw draws Bottles').
Gfx *jiggy_blubber_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (blubber_state != BLUBBER_READY || cc_number_of_players != 1) {
        return dl;
    }
    dl = hud_bullions_draw(dl);
    if (!talk_open) {
        return dl;
    }
    box_x = (D_global_asm_80744490 - BOX_WIDTH) / 2;
    box_y = D_global_asm_80744494 - BOX_BOTTOM_MARGIN - BOX_HEIGHT;

    // The box: a light border round a dark, see-through middle.
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gSPClearGeometryMode(dl++, G_ZBUFFER);
    gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    dl = box_fill(dl, box_x, box_y, box_x + BOX_WIDTH, box_y + BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x, box_y + BOX_HEIGHT - BOX_BORDER, box_x + BOX_WIDTH, box_y + BOX_HEIGHT, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x, box_y + BOX_BORDER, box_x + BOX_BORDER, box_y + BOX_HEIGHT - BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x + BOX_WIDTH - BOX_BORDER, box_y + BOX_BORDER, box_x + BOX_WIDTH, box_y + BOX_HEIGHT - BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x + BOX_BORDER, box_y + BOX_BORDER, box_x + BOX_WIDTH - BOX_BORDER, box_y + BOX_HEIGHT - BOX_BORDER,
                  0x10, 0x10, 0x28, BOX_ALPHA * talk_alpha / 0xFF);

    // His portrait, its mouth moving while the text types out.
    frame = 0;
    if (talk_shown < talk_length) {
        frame = (talk_ticks / PORTRAIT_TICKS_PER_FRAME) % (2 * portrait_frames - 2 > 0 ? 2 * portrait_frames - 2 : 1);
        if (frame >= portrait_frames) {
            frame = 2 * portrait_frames - 2 - frame;
        }
    }
    width = portrait_width[frame];
    height = portrait_height[frame];
    gDPPipeSync(dl++);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    gDPLoadTextureTile(dl++, portrait_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
        width, height,
        0, 0, width - 1, height - 1,
        0,
        G_TX_CLAMP, G_TX_CLAMP,
        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    {
        s32 px = box_x + BOX_BORDER + BOX_PAD;
        s32 py = box_y + (BOX_HEIGHT - height * PORTRAIT_SCALE) / 2;

        gSPTextureRectangle(dl++,
            px << 2, py << 2, (px + width * PORTRAIT_SCALE) << 2, (py + height * PORTRAIT_SCALE) << 2,
            G_TX_RENDERTILE,
            0, 0, (1 << 10) / PORTRAIT_SCALE, (1 << 10) / PORTRAIT_SCALE);
    }
    gDPPipeSync(dl++);

    // His name and the page so far, in DK64's dialogue font.
    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SCALE * 32 + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"CAPTAIN BLUBBER", 1);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    remaining = talk_shown;
    for (line = 0; line < talk_line_count && remaining > 0; line++) {
        s32 n = 0;

        while (talk_lines[line][n] != '\0' && n < remaining) {
            shown[n] = talk_lines[line][n];
            n++;
        }
        shown[n] = '\0';
        remaining -= n + 1;
        if (n == 0) {
            continue;
        }
        text_y = TEXT_Y(box_y + TEXT_FIRST_LINE_Y + line * TEXT_LINE_HEIGHT);
        dl = printStyledText(dl, TEXT_STYLE, text_x * 4, text_y * 4, (u8 *)shown, 1);
    }
    gDPPipeSync(dl++);
    return dl;
}
