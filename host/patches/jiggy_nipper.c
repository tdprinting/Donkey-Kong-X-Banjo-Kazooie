// Nipper: Treasure Trove Cove's hermit crab, read from the player's Banjo-Kazooie ROM, as a boss
// on Gloomy Galleon's east side, where the two worlds have collided.
//
// Like chNipper (src/TTC/ch/nipper.c), Nipper (ASSET_3D5_MODEL_NIPPER) sits in his shell idling
// (ASSET_C0_ANIM_NIPPER_IDLE) until the player comes near. The first time, he warns them off in a
// text box. Then he fights: two rounds of snapping his claws (ASSET_BF_ANIM_NIPPER_ATTACK, with
// SFX_3D_TICKER_WALKING at each snap, 0.5 and 0.95 through it), which hurt the player within
// their reach through DK64's own path for an enemy's hit, setAction(0xE), as jiggy_grublins.c
// does; then he tires (ASSET_BD_ANIM_NIPPER_VULNERABLE) for a while, and that's when he can be
// hit: one of the player's attacks within reach (the same moves as the Grublins: punches, rolls,
// aerial attacks, the Simian Slam and Banjo's own) or one of their shots (DK64's gun projectiles,
// and so Banjo's eggs). A hit plays his "ow" (ASSET_BE_ANIM_NIPPER_OW, SFX_79_TICKER_DEATH three
// times, as __chNipper_playDeathAnimation plays it) and he comes back snapping faster (chNipper's
// 1.2, 1.05 and 0.9 second snaps) and tiring for less. The first hit has him complain in the box
// (ASSET_A10_DIALOG_NIPPER_HURT's place). The third beats him: his death (ASSET_133_ANIM_NIPPER_
// DIE, SFX_78_EAGLECRY three times and SFX_7C_CHEBOOF at 0.6, as chNipper's dead state plays
// them), his last line, and a Golden Banana (drawn as a Jiggy) appears in front of him with
// Banjo-Kazooie's Jiggy jingle, while the player is held in the game's cutscene lock. After, he
// lies still, out of his shell (selector 3 hidden, as chNipper's state 7 draws him).
//
// He's drawn smaller than in Banjo-Kazooie (DK64's Kongs are smaller than his claws), and keeps
// the player out of his shell (DK64 doesn't know he's there).
//
// The Golden Banana is jiggy_rewards.c's reward 0x42 (Gloomy Galleon's range, 0x40-0x4F in
// docs/GAME_PLAN.md). His taunt and his defeat are kept with the DK64 save file (jiggy_save_get/
// set, key 0x53010000 | map: Galleon's prefix, character 1), so a beaten Nipper stays beaten.
//
// The text box is Bottles' (jiggy_bottles.c) with Banjo-Kazooie's Nipper portrait
// (ASSET_7FD_SPRITE_NIPPER, a 64x64 sprite drawn in two halves) and his voice (gczoombox's
// SFX_F9/FA/FB_GRUNTLING_NOISE at pitch 0.7). Drawn right after the player's own model
// (func_global_asm_80630DCC in patches_draw.c), his text box in the HUD pass
// (jiggy_nipper_hud_draw, patches_ui.c). Without a Banjo-Kazooie ROM nothing is drawn and nothing
// happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

#define NIPPER_MODEL 0x3D5    // ASSET_3D5_MODEL_NIPPER
#define NIPPER_PORTRAIT 0x7FD // ASSET_7FD_SPRITE_NIPPER: 64x64 frames.
// Nipper converts to 21016 bytes, with 16 bones and 34 pose commands (selector 3 drawn).
#define NIPPER_BLOCK_MAX 0x5400
#define NIPPER_GFX_MAX 64
#define NIPPER_MTX_MAX 20

// Banjo-Kazooie animations (gChNipperAnimations) and how long each plays, in seconds.
#define NIPPER_ANIM_VULNERABLE 0xBD // ASSET_BD_ANIM_NIPPER_VULNERABLE
#define NIPPER_ANIM_OW 0xBE         // ASSET_BE_ANIM_NIPPER_OW
#define NIPPER_ANIM_ATTACK 0xBF     // ASSET_BF_ANIM_NIPPER_ATTACK
#define NIPPER_ANIM_IDLE 0xC0       // ASSET_C0_ANIM_NIPPER_IDLE
#define NIPPER_ANIM_DIE 0x133       // ASSET_133_ANIM_NIPPER_DIE
#define NIPPER_IDLE_SECONDS 2.0f
#define NIPPER_VULNERABLE_SECONDS 1.5f
#define NIPPER_OW_SECONDS 1.3f
#define NIPPER_DIE_SECONDS 3.0f
// His snaps speed up with each hit (__chNipper_setAnimationDuration), and he tires for less
// (chNipper's 120, 80 and 40 ticks, here in seconds the player can follow).
static const f32 nipper_attack_seconds[] = {1.2f, 1.05f, 0.9f};
static const f32 nipper_tired_seconds[] = {4.0f, 3.0f, 2.0f};
#define NIPPER_ATTACK_LOOPS 2
#define NIPPER_HITS 3

// Banjo-Kazooie sounds (sfx_e) and how chNipper plays them.
#define NIPPER_SFX_SNAP 0x3D      // SFX_3D_TICKER_WALKING: claws and steps, 12000/32767, 0.75
#define NIPPER_SFX_OW 0x79        // SFX_79_TICKER_DEATH: hit, three at once, 17000/32767, 0.5
#define NIPPER_SFX_CRY 0x78       // SFX_78_EAGLECRY: beaten, three at once, 20000/32767, 0.7
#define NIPPER_SFX_CHEBOOF 0x7C   // SFX_7C_CHEBOOF: falling over, 20000/32767, 0.9
// His voice in the text box (gczoombox), one of these at 30000/32767, pitch 0.7.
static const u16 nipper_talk_sfx[] = {0xF9, 0xFA, 0xFB};
#define NIPPER_TALK_SFX_EVERY 5 // Characters typed out between sounds.
// Banjo-Kazooie's Jiggy appearing (COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767).
#define JINGLE_JIGGY_SPAWN 0x3D
// Banjo-Kazooie fades these out between 1500 and 3000 units; scaled to DK64 units.
#define SFX_NEAR 375.0f
#define SFX_FAR 750.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo, and how much smaller than
// that he's drawn (his claws spread 1270 Banjo-Kazooie units: about 150 DK64 units here).
#define BK_TO_DK64 0.25f
#define NIPPER_SCALE (BK_TO_DK64 * 0.48f)
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// How fast he turns to face the player, while he isn't tired or hurt.
#define NIPPER_TURN_RATE 0.06f
// He fights while the player is this close (chNipper's 1300 units, scaled) and this far above or
// below; he warns them off the first time they come this close.
#define NIPPER_FIGHT_RADIUS 300.0f
#define NIPPER_FIGHT_HEIGHT 100.0f
#define NIPPER_TAUNT_RADIUS 170.0f
// His shell, for the player's attacks and shots and to keep the player out: this wide and tall;
// his claws reach this far from his middle.
#define NIPPER_BODY_RADIUS 40.0f
#define NIPPER_BODY_HEIGHT 60.0f
#define NIPPER_CLAW_RADIUS 72.0f
#define NIPPER_CLAW_HEIGHT 50.0f
#define PLAYER_RADIUS 10.0f
// The player's attacks reach this far past his shell (the Simian Slam a little further), within
// this height; punches only hit what the player faces (within this many 12-bit angle units).
#define ATTACK_REACH (NIPPER_BODY_RADIUS + 30.0f)
#define SLAM_REACH (NIPPER_BODY_RADIUS + 45.0f)
#define ATTACK_HEIGHT 60.0f
#define PUNCH_ARC 0x300
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing him on the floor under his spot: looked for from this far above the spot, and only
// trusted this close to the spot's height.
#define NIPPER_FLOOR_PROBE 40.0f
#define NIPPER_FLOOR_MAX_STEP 60.0f

// DK64 player states (Actor control_state) that hit him.
#define STATE_SIMIAN_SLAM 0x1C
#define STATE_PUNCH 0x26
#define STATE_PUNCH_2 0x27
#define STATE_PUNCH_3 0x28
#define STATE_RUNNING_ATTACK 0x29
#define STATE_AERIAL_ATTACK 0x2B
#define STATE_AERIAL_ATTACK_DIDDY 0x2E
#define STATE_AERIAL_ATTACK_CHUNKY 0x2F
// DK64's player action for a touch from an attacking enemy (see jiggy_grublins.c).
#define ACTION_ENEMY_HIT 0xE
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

// His Golden Banana: jiggy_rewards.c's id (docs/GAME_PLAN.md), standing this high over the floor.
#define NIPPER_REWARD 0x42
#define REWARD_HEIGHT 12.5f

// The text box (as jiggy_bottles.c draws it), in screen pixels. His portrait is 64x64, drawn
// as is, in two halves that each fit in TMEM.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SIZE 64
#define PORTRAIT_MAX_TEXELS (64 * 64)
// Frames 0 to 5 move his mouth (gczoombox's talking frames for Nipper).
#define PORTRAIT_TALK_FRAMES 6
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

// Where he is, map by map. Gloomy Galleon: in the open lagoon-side area past the entrance
// tunnel, on the Diddy banana (2506, 1611, 3508), east of Blubber (jiggy_blubber.c); a Honeycomb
// (jiggy_honeycombs.c) is on the Diddy banana (2545, 1610, 3402), outside his claws. He faces
// west, where his Golden Banana appears on the Diddy banana (2306, 1610, 3492).
typedef struct {
    Maps map;
    f32 x, y, z;              // Where he sits (y is the floor, if the floor isn't found).
    f32 reward_x, reward_z;   // Where his Golden Banana appears.
} NipperPlacement;
static const NipperPlacement nipper_spots[] = {
    {MAP_GALLEON, 2506.0f, 1611.0f, 3508.0f, 2306.0f, 3492.0f},
};

// What he says. Each page is one press of A; lines are broken to fit the box when shown.
static const char *const nipper_taunt[] = {
    "OI! WHO'S THAT POKING ROUND NIPPER'S SHELL?",
    "THIS GALLEON'S MINE NOW! CLEAR OFF OR I'LL SNIP YOU IN TWO!",
};
static const char *const nipper_hurt_lines[] = {
    "OWWW! THAT HURT, YOU HAIRY HOOLIGANS! NOW I'M REALLY CRABBY!",
};
static const char *const nipper_defeat[] = {
    "OOOH... ALL RIGHT, ALL RIGHT!",
    "TAKE YOUR SHINY THING AND LEAVE POOR NIPPER IN PEACE!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): Galleon's prefix 0x53, character 1 (Nipper), and the map.
#define NIPPER_SAVE_MAP_KEY(map) (0x53000000 | (1 << 16) | (u32)(map))
#define NIPPER_SAVE_TAUNTED 0x1
#define NIPPER_SAVE_DEFEATED 0x2

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
// The actors that update this frame (as jiggy_banjo_eggs.c reads them).
typedef struct {
    Actor *actor;
    s32 unk4;
} NipperActiveActor;
extern NipperActiveActor D_global_asm_807FB930[];
extern u16 D_global_asm_807FBB34; // How many.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y and returns nonzero, or leaves it as it was.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// Whether the player can be hurt now (not invulnerable, not already knocked down).
s32 func_global_asm_806EB400(Actor *player);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
s32 jiggy_banjo_air_attack(Actor *player);

typedef enum {
    NIPPER_UNLOADED,
    NIPPER_READY,
    NIPPER_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model or the portrait didn't convert.
} NipperLoadState;

typedef enum {
    NIPPER_IDLE,       // In his shell, the player away (chNipper's state 1).
    NIPPER_ATTACK,     // Snapping (state 3).
    NIPPER_VULNERABLE, // Tired: he can be hit (state 2).
    NIPPER_OW,         // Hit (state 4).
    NIPPER_DYING,      // Beaten (state 6).
    NIPPER_DEAD,       // After (state 7).
} NipperAction;

typedef enum {
    TALK_TAUNT,
    TALK_HURT,
    TALK_DEFEAT,
} NipperTalk;

static u8 nipper_block[NIPPER_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx nipper_gfx[2][NIPPER_GFX_MAX];
static Mtx nipper_mtx[2][NIPPER_MTX_MAX];
static JiggyBkPose nipper_pose;
static u16 portrait_texels[PORTRAIT_TALK_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_TALK_FRAMES];
static s16 portrait_height[PORTRAIT_TALK_FRAMES];
static s32 portrait_frames;
static NipperLoadState nipper_state = NIPPER_UNLOADED;

static s32 nipper_last_map = -1;
static u32 nipper_last_timer;
static const NipperPlacement *nipper_spot; // This map's, or NULL.
static s32 nipper_placed;
static f32 nipper_pos[3];
static f32 nipper_yaw; // 12-bit angle units, as a float so the turn eases.
static NipperAction nipper_action;
static f32 nipper_progress; // 0 to 1 through the action's animation.
static f32 nipper_timer;    // Seconds left tired.
static s32 nipper_loops;    // Snapping rounds so far.
static s32 nipper_hits;
static s32 nipper_holding;  // The player's held from his defeat until the Golden Banana's due.
static u32 nipper_saved;    // NIPPER_SAVE_* bits.
static f32 reward_spot[3];
static s32 reward_jingle;
static u32 nipper_random_state = 0x4E495050;

// The dialogue (as jiggy_bottles.c keeps it).
static NipperTalk talk_kind;
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

static f32 nipper_random(void) {
    nipper_random_state = nipper_random_state * 1664525 + 1013904223;
    return (nipper_random_state >> 8) * (1.0f / 16777216.0f);
}

static void nipper_load(void) {
    s32 size;
    s32 count;
    s32 i;

    nipper_state = NIPPER_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(NIPPER_MODEL, nipper_block, sizeof(nipper_block));
    if (size <= 0 || size > (s32)sizeof(nipper_block)) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(NIPPER_PORTRAIT);
    if (count <= 0) {
        return;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(NIPPER_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        // Two halves of it have to fit in TMEM (2048 RGBA16 texels each).
        if (got <= 0 || width == 0 || height < 2 || width * height > PORTRAIT_MAX_TEXELS || width > PORTRAIT_SIZE) {
            continue;
        }
        portrait_width[portrait_frames] = width;
        portrait_height[portrait_frames] = height;
        portrait_frames++;
    }
    if (portrait_frames == 0) {
        return;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden; chNipper's draw shows selector 3 until he's dead.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        nipper_pose.selectors[i] = 0;
    }
    nipper_pose.selectors[1] = 1;
    nipper_pose.selectors[3] = 1;
    nipper_state = NIPPER_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// A Banjo-Kazooie sound from where Nipper is, fading out with distance from the player.
static void nipper_sfx(Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(nipper_pos[0], nipper_pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

// True when `at` is passed going from `from` to `to` through an animation.
static s32 nipper_passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

static void nipper_start(NipperAction action, f32 progress) {
    nipper_action = action;
    nipper_progress = progress;
}

// The floor under (x, z), looked for from above `y`; `y` if there's none near.
static f32 nipper_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + NIPPER_FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < NIPPER_FLOOR_MAX_STEP &&
        y - floor < NIPPER_FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

// Sits him on the floor at his spot, facing where his Golden Banana goes.
static void nipper_place(void) {
    nipper_pos[0] = nipper_spot->x;
    nipper_pos[1] = nipper_floor(nipper_spot->x, nipper_spot->y, nipper_spot->z);
    nipper_pos[2] = nipper_spot->z;
    reward_spot[0] = nipper_spot->reward_x;
    reward_spot[1] = nipper_floor(nipper_spot->reward_x, nipper_pos[1], nipper_spot->reward_z);
    reward_spot[2] = nipper_spot->reward_z;
    nipper_yaw = func_global_asm_80665DE0(reward_spot[0], reward_spot[2], nipper_pos[0], nipper_pos[2]) & 0xFFF;
    nipper_hits = 0;
    nipper_loops = 0;
    nipper_holding = FALSE;
    reward_jingle = FALSE;
    if (nipper_saved & NIPPER_SAVE_DEFEATED) {
        nipper_start(NIPPER_DEAD, 1.0f);
    } else {
        nipper_start(NIPPER_IDLE, nipper_random());
    }
    nipper_placed = TRUE;
}

// Breaks the page into lines that fit the box, a word at a time (as jiggy_bottles.c does).
static void talk_set_page(s32 page) {
    const char *text = talk_pages[page];
    s32 text_width = BOX_WIDTH - 2 * BOX_BORDER - BOX_PAD - PORTRAIT_SIZE - 2 * BOX_PAD;
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
static void nipper_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void nipper_save(u32 bits) {
    nipper_saved |= bits;
    jiggy_save_set(current_file, NIPPER_SAVE_MAP_KEY(current_map), nipper_saved);
}

static void talk_start(NipperTalk kind) {
    talk_kind = kind;
    if (kind == TALK_TAUNT) {
        talk_pages = nipper_taunt;
        talk_page_count = ARRAY_COUNT_OF(nipper_taunt);
    } else if (kind == TALK_HURT) {
        talk_pages = nipper_hurt_lines;
        talk_page_count = ARRAY_COUNT_OF(nipper_hurt_lines);
    } else {
        talk_pages = nipper_defeat;
        talk_page_count = ARRAY_COUNT_OF(nipper_defeat);
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    nipper_lock_player(TRUE);
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
        if (talk_shown / NIPPER_TALK_SFX_EVERY != before / NIPPER_TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 pick = (u32)(nipper_random() * ARRAY_COUNT_OF(nipper_talk_sfx)) % ARRAY_COUNT_OF(nipper_talk_sfx);

            jiggy_bk_sfx_play(nipper_talk_sfx[pick], 30000.0f / 32767.0f, 0.7f);
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
    if (talk_kind == TALK_TAUNT) {
        nipper_save(NIPPER_SAVE_TAUNTED);
        // The fight's on.
        nipper_loops = 0;
        nipper_start(NIPPER_ATTACK, 0.0f);
    } else if (talk_kind == TALK_DEFEAT) {
        // His Golden Banana appears, with Banjo-Kazooie's Jiggy jingle.
        nipper_holding = FALSE;
        reward_jingle = TRUE;
    }
    nipper_lock_player(FALSE);
}

// Whether the player is where he fights them.
static s32 nipper_player_near(Actor *player, f32 radius) {
    f32 dy = player->y_position - nipper_pos[1];

    return distance_xz(nipper_pos[0], nipper_pos[2], player->x_position, player->z_position) <= radius &&
           dy <= NIPPER_FIGHT_HEIGHT && dy >= -NIPPER_FIGHT_HEIGHT;
}

// Whether the player's attack hits him this frame (as jiggy_grublins.c checks, for his size).
static s32 nipper_attacked(Actor *player) {
    f32 distance = distance_xz(nipper_pos[0], nipper_pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - nipper_pos[1];
    s32 to_nipper;
    s32 off;

    if (dy > ATTACK_HEIGHT || dy < -ATTACK_HEIGHT) {
        return FALSE;
    }
    switch (player->control_state) {
        case STATE_SIMIAN_SLAM:
            return distance <= SLAM_REACH;
        case STATE_PUNCH:
        case STATE_PUNCH_2:
        case STATE_PUNCH_3:
            if (distance > ATTACK_REACH) {
                return FALSE;
            }
            // Against his shell, a punch always lands; further out, only in front of the player.
            if (distance <= NIPPER_BODY_RADIUS + PLAYER_RADIUS + 4.0f) {
                return TRUE;
            }
            to_nipper = func_global_asm_80665DE0(nipper_pos[0], nipper_pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_nipper - (player->y_rotation & 0xFFF)) & 0xFFF;
            return off <= PUNCH_ARC || off >= 0x1000 - PUNCH_ARC;
        case STATE_RUNNING_ATTACK:
        case STATE_AERIAL_ATTACK:
        case STATE_AERIAL_ATTACK_DIDDY:
        case STATE_AERIAL_ATTACK_CHUNKY:
            return distance <= ATTACK_REACH;
        default:
            // Banjo's Rat-a-tat Rap and Beak Buster, which DK64's states don't show.
            return jiggy_banjo_air_attack(player) && distance <= ATTACK_REACH;
    }
}

// Whether one of the player's own shots (a gun's projectile, or one of Banjo's eggs) is in him.
static s32 nipper_shot(Actor *player) {
    s32 i;

    for (i = 0; i < D_global_asm_807FBB34; i++) {
        Actor *shot = D_global_asm_807FB930[i].actor;
        f32 dy;

        if (shot == NULL || shot->unk11C != player) {
            continue;
        }
        switch (shot->unk58) {
            case ACTOR_PROJECTILE_COCONUTS:
            case ACTOR_PROJECTILE_PEANUT:
            case ACTOR_PROJECTILE_GRAPE:
            case ACTOR_PROJECTILE_FEATHER:
            case ACTOR_PROJECTILE_PINEAPPLE:
            case ACTOR_PROJECTILE_ORANGE:
                break;
            default:
                continue;
        }
        dy = shot->y_position - nipper_pos[1];
        if (dy >= 0.0f && dy <= NIPPER_BODY_HEIGHT &&
            distance_xz(nipper_pos[0], nipper_pos[2], shot->x_position, shot->z_position) <= NIPPER_BODY_RADIUS) {
            return TRUE;
        }
    }
    return FALSE;
}

// Hurts the player through DK64's own enemy hit (as jiggy_grublins.c does), knocking them away
// from Nipper.
static void nipper_hit_player(Actor *player) {
    // The knockback turns the player's heading round, so it's set toward Nipper first.
    player->unkEE = func_global_asm_80665DE0(nipper_pos[0], nipper_pos[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
}

// DK64 doesn't know he's there: his shell keeps the player out.
static void nipper_push_player(Actor *player) {
    f32 dx = player->x_position - nipper_pos[0];
    f32 dz = player->z_position - nipper_pos[2];
    f32 reach = NIPPER_BODY_RADIUS + PLAYER_RADIUS;
    f32 distance = _sqrtf(dx * dx + dz * dz);

    if (distance >= reach || distance < 0.01f || player->y_position > nipper_pos[1] + NIPPER_BODY_HEIGHT) {
        return;
    }
    player->x_position = nipper_pos[0] + dx * reach / distance;
    player->z_position = nipper_pos[2] + dz * reach / distance;
}

// Hit while tired (__chNipper_dieFunc): ow, or beaten on the third.
static void nipper_hurt(Actor *player) {
    nipper_hits++;
    if (nipper_hits >= NIPPER_HITS) {
        nipper_save(NIPPER_SAVE_DEFEATED);
        nipper_start(NIPPER_DYING, 0.0f);
        nipper_holding = TRUE;
        nipper_lock_player(TRUE);
        nipper_sfx(player, NIPPER_SFX_CRY, 20000.0f / 32767.0f, 0.7f);
        nipper_sfx(player, NIPPER_SFX_CRY, 20000.0f / 32767.0f, 0.7f);
        nipper_sfx(player, NIPPER_SFX_CRY, 20000.0f / 32767.0f, 0.7f);
        return;
    }
    nipper_start(NIPPER_OW, 0.0f);
    nipper_sfx(player, NIPPER_SFX_OW, 17000.0f / 32767.0f, 0.5f);
    nipper_sfx(player, NIPPER_SFX_OW, 17000.0f / 32767.0f, 0.5f);
    nipper_sfx(player, NIPPER_SFX_OW, 17000.0f / 32767.0f, 0.5f);
}

// Turns a share of the way to face the player, over `frames` frames.
static void nipper_turn_to_player(Actor *player, u32 frames) {
    s16 target = func_global_asm_80665DE0(player->x_position, player->z_position, nipper_pos[0], nipper_pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - nipper_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        nipper_yaw += turn * NIPPER_TURN_RATE;
        if (nipper_yaw < 0.0f) {
            nipper_yaw += 4096.0f;
        } else if (nipper_yaw >= 4096.0f) {
            nipper_yaw -= 4096.0f;
        }
    }
}

// A frame of his behavior (__chNipper_updateFunc), `frames` game frames on.
static void nipper_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = nipper_progress;
    s32 round = nipper_hits < NIPPER_HITS ? nipper_hits : NIPPER_HITS - 1;
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;
    s32 near = nipper_player_near(player, NIPPER_FIGHT_RADIUS);

    if (frames == 0) {
        return;
    }
    // While he talks, he only keeps up his animation.
    if (talk_open) {
        if (nipper_action != NIPPER_DEAD) {
            f32 loop = nipper_action == NIPPER_ATTACK       ? nipper_attack_seconds[round]
                       : nipper_action == NIPPER_VULNERABLE ? NIPPER_VULNERABLE_SECONDS
                                                            : NIPPER_IDLE_SECONDS;

            nipper_progress += seconds / loop;
            while (nipper_progress >= 1.0f) {
                nipper_progress -= 1.0f;
            }
        }
        talk_update();
        return;
    }
    if (nipper_action != NIPPER_DEAD && nipper_action != NIPPER_DYING) {
        nipper_push_player(player);
    }
    // The first time the player comes near, he warns them off.
    if (nipper_action == NIPPER_IDLE && !talk_open && !(nipper_saved & NIPPER_SAVE_TAUNTED) && grounded &&
        nipper_player_near(player, NIPPER_TAUNT_RADIUS)) {
        talk_start(TALK_TAUNT);
    }
    // Tired, he can be hit.
    if (nipper_action == NIPPER_VULNERABLE && !talk_open && (nipper_attacked(player) || nipper_shot(player))) {
        nipper_hurt(player);
        before = 0.0f;
    }

    switch (nipper_action) {
        case NIPPER_IDLE:
            nipper_progress += seconds / NIPPER_IDLE_SECONDS;
            if (nipper_passed(before, nipper_progress, 0.2f) || nipper_passed(before, nipper_progress, 0.6f)) {
                nipper_sfx(player, NIPPER_SFX_SNAP, 12000.0f / 32767.0f, 0.75f);
            }
            while (nipper_progress >= 1.0f) {
                nipper_progress -= 1.0f;
            }
            if ((nipper_saved & NIPPER_SAVE_TAUNTED) && !talk_open && near) {
                nipper_loops = 0;
                nipper_start(NIPPER_ATTACK, 0.0f);
            }
            break;
        case NIPPER_ATTACK:
            nipper_turn_to_player(player, frames);
            nipper_progress += seconds / nipper_attack_seconds[round];
            if (nipper_passed(before, nipper_progress, 0.5f) || nipper_passed(before, nipper_progress, 0.95f)) {
                nipper_sfx(player, NIPPER_SFX_SNAP, 12000.0f / 32767.0f, 0.75f);
            }
            // His claws catch the player within their reach.
            if (!talk_open && distance_xz(nipper_pos[0], nipper_pos[2], player->x_position, player->z_position) <= NIPPER_CLAW_RADIUS &&
                player->y_position - nipper_pos[1] <= NIPPER_CLAW_HEIGHT && nipper_pos[1] - player->y_position <= NIPPER_CLAW_HEIGHT &&
                func_global_asm_806EB400(player)) {
                nipper_hit_player(player);
            }
            if (nipper_progress >= 1.0f) {
                nipper_progress -= 1.0f;
                nipper_loops++;
                if (!near) {
                    nipper_start(NIPPER_IDLE, 0.0f);
                } else if (nipper_loops >= NIPPER_ATTACK_LOOPS) {
                    nipper_start(NIPPER_VULNERABLE, 0.0f);
                    nipper_timer = nipper_tired_seconds[round];
                }
            }
            break;
        case NIPPER_VULNERABLE:
            nipper_progress += seconds / NIPPER_VULNERABLE_SECONDS;
            while (nipper_progress >= 1.0f) {
                nipper_progress -= 1.0f;
            }
            nipper_timer -= seconds;
            if (nipper_timer <= 0.0f) {
                nipper_loops = 0;
                nipper_start(near ? NIPPER_ATTACK : NIPPER_IDLE, 0.0f);
            }
            break;
        case NIPPER_OW:
            nipper_progress += seconds / NIPPER_OW_SECONDS;
            if (nipper_progress >= 1.0f) {
                nipper_loops = 0;
                nipper_start(NIPPER_ATTACK, 0.0f);
                // The first hit, he complains.
                if (nipper_hits == 1) {
                    talk_start(TALK_HURT);
                }
            }
            break;
        case NIPPER_DYING:
            nipper_lock_player(TRUE);
            nipper_progress += seconds / NIPPER_DIE_SECONDS;
            if (nipper_passed(before, nipper_progress, 0.6f)) {
                nipper_sfx(player, NIPPER_SFX_CHEBOOF, 20000.0f / 32767.0f, 0.9f);
            }
            if (nipper_progress >= 1.0f) {
                nipper_start(NIPPER_DEAD, 1.0f);
                talk_start(TALK_DEFEAT);
            }
            break;
        case NIPPER_DEAD:
            break;
    }
    // Beaten (and done saying so): his Golden Banana, until it's collected.
    if ((nipper_saved & NIPPER_SAVE_DEFEATED) && nipper_action == NIPPER_DEAD && !nipper_holding && !talk_open) {
        if (!jiggy_reward_update(NIPPER_REWARD, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], player,
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

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Nipper once a frame, with the player.
Gfx *jiggy_nipper_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    f32 rotate[4][4];
    f32 scale[4][4];
    s32 animation;
    f32 progress;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): he's back where he was.
    if ((s32)current_map != nipper_last_map || object_timer < nipper_last_timer) {
        // Leaving the map mid-talk (or mid-death) lets the player go.
        if (talk_open || nipper_holding) {
            nipper_lock_player(FALSE);
            talk_open = FALSE;
            nipper_holding = FALSE;
        }
        nipper_last_map = current_map;
        nipper_placed = FALSE;
        nipper_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(nipper_spots); i++) {
            if (nipper_spots[i].map == current_map) {
                nipper_spot = &nipper_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        nipper_saved = nipper_spot != NULL ? jiggy_save_get(current_file, NIPPER_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (nipper_spot == NULL) {
        nipper_last_timer = object_timer;
        return dl;
    }
    if (nipper_state == NIPPER_UNLOADED) {
        nipper_load();
    }
    if (nipper_state != NIPPER_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!nipper_placed) {
        nipper_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        nipper_place();
    }
    frames = object_timer - nipper_last_timer;
    nipper_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && !nipper_holding) {
        frames = 0;
    }
    nipper_update(actor, frames);

    progress = nipper_progress;
    switch (nipper_action) {
        case NIPPER_ATTACK:
            animation = NIPPER_ANIM_ATTACK;
            break;
        case NIPPER_VULNERABLE:
            animation = NIPPER_ANIM_VULNERABLE;
            break;
        case NIPPER_OW:
            animation = NIPPER_ANIM_OW;
            break;
        case NIPPER_DYING:
        case NIPPER_DEAD:
            animation = NIPPER_ANIM_DIE;
            break;
        default:
            animation = NIPPER_ANIM_IDLE;
            break;
    }
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    guScaleF(scale, NIPPER_SCALE, NIPPER_SCALE, NIPPER_SCALE);
    guRotateF(rotate, nipper_yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, nipper_pose.world);
    nipper_pose.world[3][0] = nipper_pos[0];
    nipper_pose.world[3][1] = nipper_pos[1];
    nipper_pose.world[3][2] = nipper_pos[2];
    nipper_pose.model = NIPPER_MODEL;
    nipper_pose.block = nipper_block;
    nipper_pose.animation = animation;
    nipper_pose.progress = progress;
    nipper_pose.selectors[3] = nipper_action == NIPPER_DEAD ? 0 : 1;
    nipper_pose.gfx = nipper_gfx[buffer];
    nipper_pose.gfx_capacity = NIPPER_GFX_MAX;
    nipper_pose.mtx = nipper_mtx[buffer];
    nipper_pose.mtx_capacity = NIPPER_MTX_MAX;
    if (jiggy_bk_model_pose(&nipper_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, nipper_gfx[buffer]);
    return dl;
}

// A filled rectangle in the primitive color, blended over what's there.
static Gfx *box_fill(Gfx *dl, s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, r, g, b, a);
    gDPFillRectangle(dl++, x0, y0, x1, y1);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): his text
// box while he talks (drawn as jiggy_bottles_hud_draw draws Bottles').
Gfx *jiggy_nipper_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    s32 h;
    char shown[TEXT_LINE_MAX];

    if (!talk_open || nipper_state != NIPPER_READY || cc_number_of_players != 1) {
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

    // His portrait, its mouth moving while the text types out: two halves, each a TMEM load.
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
    for (h = 0; h < 2; h++) {
        s32 first = h * (height / 2);
        s32 last = h == 0 ? height / 2 - 1 : height - 1;
        s32 px = box_x + BOX_BORDER + BOX_PAD + (PORTRAIT_SIZE - width) / 2;
        s32 py = box_y + (BOX_HEIGHT - height) / 2;

        gDPLoadTextureTile(dl++, portrait_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, height,
            0, first, width - 1, last,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(dl++,
            px << 2, (py + first) << 2, (px + width) << 2, (py + last + 1) << 2,
            G_TX_RENDERTILE,
            0, first << 5, 1 << 10, 1 << 10);
    }
    gDPPipeSync(dl++);

    // His name and the page so far, in DK64's dialogue font.
    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SIZE + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"NIPPER", 1);
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
