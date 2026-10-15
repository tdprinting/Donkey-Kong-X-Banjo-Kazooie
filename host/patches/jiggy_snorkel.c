// Snorkel: Rusty Bucket Bay's stuck dolphin, read from the player's Banjo-Kazooie ROM, with the
// bay's whistle-button puzzle, in the room north of Frantic Factory's Foyer, where the two
// worlds have collided.
//
// Like chSnorkel (src/RBB/ch/dolphin.c), Snorkel (ASSET_41C_MODEL_SNORKEL) is stuck fast,
// wriggling (ASSET_13F_ANIM_SNORKEL_STUCK) and wailing (SFX_D1_SNORKEL_WAH at 0.22 and 0.54 of
// it). The first time the player comes near, he tells them how to free him in a text box while
// the game's cutscene lock holds them: three whistle switches stand in a row nearby
// (ASSET_413/414/415_MODEL_WHISTLE_SWITCH_1/2/3), and the ship's three whistles
// (ASSET_416_MODEL_WHISTLE) along the wall behind him; the switches must be pressed in the
// captain's tune, Rusty Bucket Bay's own code "312111" (chWhistleCtrl). A switch is pressed by
// landing on it from a jump, by a Simian Slam on it, or by Banjo's Beak Buster: it sinks and
// springs back up (chRBBWhistleSwitch, SFX_90_SWITCH_PRESS), and its whistle blows (chRBBWhistle:
// its animation and note, SFX_3FD/3FE/3FF). A right switch dings (COMUSIC_2B_DING_B); a wrong one
// buzzes (COMUSIC_2C_BUZZER), starts the tune over and Snorkel sings it again. The whole tune
// plays Banjo-Kazooie's puzzle fanfare (COMUSIC_2D_PUZZLE_SOLVED_FANFARE) in a short cutscene:
// the whistles blow together, the steam pops Snorkel loose (he swims, as when the anchor frees
// him, ASSET_13E_ANIM_SNORKEL_SWIM), he thanks the player and his Jiggy appears (a Golden Banana
// drawn as a Jiggy by jiggy_collectibles.c, reward 0x32 from jiggy_rewards.c, with
// COMUSIC_3D_JIGGY_SPAWN). Freed, he swims happy circles in the steam.
//
// After the first talk, standing still by him gets a reminder of the tune. The talk and the
// puzzle are kept with the DK64 save file (key 0x52010000 | map: Frantic Factory's characters,
// index 1); a tune half played is forgotten when the player leaves.
//
// The text box is Bottles' (jiggy_bottles.c): Banjo-Kazooie's Snorkel portrait
// (ASSET_838_SPRITE_SNORKEL, its mouth moving as the text types out) beside DK64's dialogue
// font, with his voice (SFX_D1_SNORKEL_WAH at 20000/32767, pitch 1.2, as gczoombox plays it).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his
// text box in the HUD pass (jiggy_snorkel_hud_draw, patches_ui.c). Without a Banjo-Kazooie ROM
// nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

#define SNORKEL_MODEL 0x41C   // ASSET_41C_MODEL_SNORKEL
#define WHISTLE_MODEL 0x416   // ASSET_416_MODEL_WHISTLE
#define SWITCH_MODEL_1 0x413  // ASSET_413_MODEL_WHISTLE_SWITCH_1 (then 2 and 3)
#define SNORKEL_PORTRAIT 0x838 // ASSET_838_SPRITE_SNORKEL
// Snorkel converts to about 5 KiB, with 15 bones and 7 pose commands at rest; a whistle to
// about 4 KiB, with 5 bones and 16 commands; each switch to about 6.2 KiB, with no bones and 6
// commands.
#define SNORKEL_BLOCK_MAX 0x1800
#define SNORKEL_GFX_MAX 40
#define SNORKEL_MTX_MAX 20
#define WHISTLE_BLOCK_MAX 0x1200
#define WHISTLE_GFX_MAX 40
#define WHISTLE_MTX_MAX 8
#define SWITCH_BLOCK_MAX 0x1A00
#define SWITCH_GFX_MAX 16
#define SWITCH_MTX_MAX 4
#define WHISTLE_COUNT 3

// Banjo-Kazooie animations and how long each plays, in seconds.
#define SNORKEL_ANIM_SWIM 0x13E  // ASSET_13E_ANIM_SNORKEL_SWIM
#define SNORKEL_ANIM_STUCK 0x13F // ASSET_13F_ANIM_SNORKEL_STUCK
#define SNORKEL_SWIM_SECONDS 0.65f
#define SNORKEL_STUCK_SECONDS 5.5f
// Each whistle's own blowing animation (chRBBWhistleInfo: 0x136, 0x135, 0x134), played once.
static const u16 whistle_anims[WHISTLE_COUNT] = {0x136, 0x135, 0x134};
#define WHISTLE_BLOW_SECONDS 0.5f

// Banjo-Kazooie sounds (sfx_e) and music (comusic_e), and how RBB plays them.
#define SNORKEL_SFX_WAH 0xD1        // SFX_D1_SNORKEL_WAH: at 0.22 (15000/32767, 0.8-0.9) and 0.54
                                    // (19000/32767, 0.9-1.0) through his stuck wriggle
#define SWITCH_SFX_PRESS 0x90       // SFX_90_SWITCH_PRESS, 1.0
// Each whistle's note (chRBBWhistleInfo: 0x3FF, 0x3FE, 0x3FD), 0.1 s after it starts, 32000/32767.
static const u16 whistle_sfx[WHISTLE_COUNT] = {0x3FF, 0x3FE, 0x3FD};
#define WHISTLE_SFX_DELAY 0.1f
#define JINGLE_DING 0x2B            // COMUSIC_2B_DING_B, 28000/32767, 0.6 s after a right switch
#define JINGLE_BUZZER 0x2C          // COMUSIC_2C_BUZZER, 28000/32767, 1.0 s after a wrong one
#define JINGLE_PUZZLE_SOLVED 0x2D   // COMUSIC_2D_PUZZLE_SOLVED_FANFARE, 28000/32767, 0.6 s after
#define JINGLE_JIGGY_SPAWN 0x3D     // COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767
#define DING_DELAY 0.6f
#define BUZZER_DELAY 1.0f
#define SNORKEL_TALK_SFX_EVERY 6 // Characters typed out between his talking sounds.
// Banjo-Kazooie fades Snorkel out between 1500 and 2000 units; scaled to DK64 units.
#define SFX_NEAR 375.0f
#define SFX_FAR 500.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// The whistles are drawn at chRBBWhistle's scale (0.25), the rest at 1.
#define WHISTLE_SCALE 0.25f

// The tune (RBB_WHISTLE_CODE), as switch numbers.
static const u8 whistle_code[] = {3, 1, 2, 1, 1, 1};
#define WHISTLE_CODE_LENGTH (sizeof(whistle_code) / sizeof(whistle_code[0]))

// A switch is pressed when the player lands on it, slams it or Beak Busts it within this
// distance of its middle (it's 200 units across, scaled) and this height over it; it sinks this
// far (30 units, scaled) and comes back up at this speed (60 units a second, scaled), and can't
// be pressed again for a second (chRBBWhistleSwitch).
#define SWITCH_RADIUS 24.0f
#define SWITCH_PRESS_ABOVE 30.0f
#define SWITCH_PRESS_BELOW 8.0f
#define SWITCH_SINK 7.5f
#define SWITCH_RISE_SPEED 15.0f
#define SWITCH_PRESS_SECONDS 1.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Talking: the first time when the player comes this close on the ground; after that, a
// reminder when they stand still this close, once they've been this far off since the last.
#define SNORKEL_MEET_RADIUS 90.0f
#define SNORKEL_REMIND_RADIUS 60.0f
#define SNORKEL_REARM_RADIUS 200.0f
#define SNORKEL_TALK_HEIGHT 50.0f
// Freed, he swims round his spot this far out, this high, a lap in this long.
#define SWIM_RADIUS 28.0f
#define SWIM_HEIGHT 22.0f
#define SWIM_BOB 5.0f
#define SWIM_LAP_SECONDS 6.0f
// The solved cutscene, in seconds from the last switch: the fanfare, each whistle in turn (the
// tune's first three notes), Snorkel popping free and then thanking the player.
#define SOLVED_FANFARE_AT 0.6f
#define SOLVED_WHISTLE_AT 1.2f
#define SOLVED_WHISTLE_STEP 0.4f
#define SOLVED_FREE_AT 2.6f
#define SOLVED_TALK_AT 3.6f
// Placing things: the floor is looked for from this far above a spot, and only trusted this close.
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f

// DK64 player states (Actor control_state).
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
#define STATE_SIMIAN_SLAM 0x1C
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

// The text box (as jiggy_bottles.c draws it), in screen pixels.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 10
#define PORTRAIT_MAX_TEXELS (32 * 32)
// Frames 0 to 9 move his mouth (gczoombox's 10 talking frames for Snorkel).
#define PORTRAIT_TALK_FRAMES 10
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

// Where it all is, map by map. Frantic Factory's north room (through the doorway at the north
// of the Foyer; its floor is x 1048 to 1484, z 2036 to 2276, all at 840): the whistles along
// the north wall, between DK64's bananas there; Snorkel in front of them, clear of the object in
// the room's middle (1267, 2196); the switches in a row to the west (1 to 3, west to east); his
// Jiggy east of him.
typedef struct {
    Maps map;
    f32 x, y, z;                        // Snorkel (y is the room's floor, if the floor isn't found).
    f32 whistle_x[WHISTLE_COUNT], whistle_z;
    f32 switch_x[WHISTLE_COUNT], switch_z;
    f32 reward_x, reward_z;
} SnorkelPlacement;
static const SnorkelPlacement snorkel_spots[] = {
    {MAP_FACTORY, 1268.0f, 840.0f, 2125.0f, {1230.0f, 1268.0f, 1306.0f}, 2075.0f, {1090.0f, 1150.0f, 1210.0f}, 2165.0f,
     1330.0f, 2160.0f},
};
// Reward ids (docs/GAME_PLAN.md: Frantic Factory's are 0x30 to 0x3F).
#define SNORKEL_REWARD 0x32
#define REWARD_HEIGHT 12.5f
#define REWARD_CLEARANCE 30.0f

// What he says. Each page is one press of A; lines are broken to fit the box when shown.
static const char *const snorkel_meet[] = {
    "WAH! I'M SNORKEL! GRUNTY'S GOONS FLUSHED ME OUT OF THE BAY!",
    "NOW MY TAIL'S STUCK FAST IN THIS FACTORY FLOOR. WAH!",
    "BOUNCE ON THE WHISTLE BUTTONS IN THE CAPTAIN'S TUNE:",
    "THREE, ONE, TWO, ONE, ONE, ONE! THE STEAM WILL POP ME FREE!",
};
static const char *const snorkel_remind[] = {
    "WAH! THE TUNE GOES THREE, ONE, TWO, ONE, ONE, ONE!",
};
static const char *const snorkel_wrong[] = {
    "WAH! WRONG TUNE! IT'S THREE, ONE, TWO, ONE, ONE, ONE!",
};
static const char *const snorkel_solved[] = {
    "WHEEE! I'M FREE! I FOUND THIS JIGGY IN THE PIPES. IT'S YOURS!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): SNORKEL_SAVE_* bits. Frantic Factory's characters are 0x52, with
// the character's index (Snorkel is 1) in bits 16 to 23.
#define SNORKEL_SAVE_KEY(map) (0x52000000 | (1u << 16) | (u32)(map))
#define SNORKEL_SAVE_MET 0x1
#define SNORKEL_SAVE_SOLVED 0x2

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
// The floor under (x, z): writes its height to *y, leaving it as it was if there's none.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
s32 jiggy_banjo_air_attack(Actor *player);

typedef enum {
    SNORKEL_UNLOADED,
    SNORKEL_READY,
    SNORKEL_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or the portrait didn't convert.
} SnorkelLoadState;

typedef enum {
    SNORKEL_STUCK,
    SNORKEL_FREE, // Swimming.
} SnorkelAction;

typedef enum {
    TALK_MEET,
    TALK_REMIND,
    TALK_WRONG,
    TALK_SOLVED,
} SnorkelTalk;

static u8 snorkel_block[SNORKEL_BLOCK_MAX] __attribute__((aligned(16)));
static u8 whistle_block[WHISTLE_BLOCK_MAX] __attribute__((aligned(16)));
static u8 switch_blocks[WHISTLE_COUNT][SWITCH_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx snorkel_gfx[2][SNORKEL_GFX_MAX];
static Mtx snorkel_mtx[2][SNORKEL_MTX_MAX];
static Gfx whistle_gfx[2][WHISTLE_COUNT][WHISTLE_GFX_MAX];
static Mtx whistle_mtx[2][WHISTLE_COUNT][WHISTLE_MTX_MAX];
static Gfx switch_gfx[2][WHISTLE_COUNT][SWITCH_GFX_MAX];
static Mtx switch_mtx[2][WHISTLE_COUNT][SWITCH_MTX_MAX];
static JiggyBkPose snorkel_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static SnorkelLoadState snorkel_state = SNORKEL_UNLOADED;

static s32 snorkel_last_map = -1;
static u32 snorkel_last_timer;
static const SnorkelPlacement *snorkel_spot; // This map's, or NULL.
static s32 snorkel_placed;
static f32 snorkel_pos[3];
static f32 snorkel_floor;
static SnorkelAction snorkel_action;
static f32 snorkel_progress; // 0 to 1 through his animation.
static f32 snorkel_lap;      // 0 to 1 round his swim.
static u32 snorkel_saved;    // SNORKEL_SAVE_* bits.
static s32 snorkel_armed;    // A reminder can be given.
static u32 snorkel_random_state = 0x534E4F52;

// The whistles and their switches.
static f32 whistle_pos[WHISTLE_COUNT][3];
static f32 whistle_progress[WHISTLE_COUNT]; // Through blowing, or -1 while still.
static f32 switch_pos[WHISTLE_COUNT][3];
static f32 switch_sink[WHISTLE_COUNT];    // How far down it is now.
static f32 switch_timer[WHISTLE_COUNT];   // Seconds before it can be pressed again.
static s32 code_index;                    // How far through the tune.
static s32 player_was_airborne;
static f32 jingle_timer; // Seconds until the pending jingle (a ding, the buzzer or the fanfare).
static s32 jingle_pending;
static s32 wrong_pending; // Sing the tune again after the buzzer.
static s32 solved_cutscene;
static f32 solved_seconds;
static s32 reward_due;
static s32 reward_jingle;

// The dialogue (as jiggy_bottles.c keeps it).
static SnorkelTalk talk_kind;
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

static f32 snorkel_random(void) {
    snorkel_random_state = snorkel_random_state * 1664525 + 1013904223;
    return (snorkel_random_state >> 8) * (1.0f / 16777216.0f);
}

static f32 snorkel_random_between(f32 low, f32 high) {
    return low + snorkel_random() * (high - low);
}

static s32 build(u32 model, void *block, u32 size) {
    s32 got = jiggy_bk_model_build(model, block, size);

    return got > 0 && got <= (s32)size;
}

static void snorkel_load(void) {
    s32 count;
    s32 i;

    snorkel_state = SNORKEL_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    if (!build(SNORKEL_MODEL, snorkel_block, sizeof(snorkel_block)) ||
        !build(WHISTLE_MODEL, whistle_block, sizeof(whistle_block))) {
        return;
    }
    for (i = 0; i < WHISTLE_COUNT; i++) {
        if (!build(SWITCH_MODEL_1 + i, switch_blocks[i], sizeof(switch_blocks[i]))) {
            return;
        }
    }
    count = jiggy_bk_sprite_frame_count(SNORKEL_PORTRAIT);
    if (count <= 0) {
        return;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES && portrait_frames < PORTRAIT_MAX_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(SNORKEL_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

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
    snorkel_state = SNORKEL_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// True when `at` is passed going from `from` to `to`.
static s32 passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

// The floor under (x, z) near `y`, or `y` if there's none near.
static f32 floor_under(f32 x, f32 y, f32 z) {
    f32 probe = y + FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < FLOOR_MAX_STEP && y - floor < FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

// A Banjo-Kazooie sound from where Snorkel is, fading out with distance from the player.
static void snorkel_sfx(Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(snorkel_pos[0], snorkel_pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static void snorkel_place(Actor *player) {
    s32 i;

    snorkel_floor = floor_under(snorkel_spot->x, snorkel_spot->y, snorkel_spot->z);
    snorkel_pos[0] = snorkel_spot->x;
    snorkel_pos[1] = snorkel_floor;
    snorkel_pos[2] = snorkel_spot->z;
    for (i = 0; i < WHISTLE_COUNT; i++) {
        whistle_pos[i][0] = snorkel_spot->whistle_x[i];
        whistle_pos[i][1] = floor_under(snorkel_spot->whistle_x[i], snorkel_floor, snorkel_spot->whistle_z);
        whistle_pos[i][2] = snorkel_spot->whistle_z;
        whistle_progress[i] = -1.0f;
        switch_pos[i][0] = snorkel_spot->switch_x[i];
        switch_pos[i][1] = floor_under(snorkel_spot->switch_x[i], snorkel_floor, snorkel_spot->switch_z);
        switch_pos[i][2] = snorkel_spot->switch_z;
        switch_sink[i] = 0.0f;
        switch_timer[i] = 0.0f;
    }
    code_index = 0;
    jingle_pending = 0;
    wrong_pending = FALSE;
    solved_cutscene = FALSE;
    player_was_airborne = FALSE;
    snorkel_armed = FALSE;
    snorkel_progress = snorkel_random();
    snorkel_lap = snorkel_random();
    reward_jingle = FALSE;
    if (snorkel_saved & SNORKEL_SAVE_SOLVED) {
        snorkel_action = SNORKEL_FREE;
        reward_due = !jiggy_reward_collected(SNORKEL_REWARD);
    } else {
        snorkel_action = SNORKEL_STUCK;
        reward_due = FALSE;
    }
    snorkel_placed = TRUE;
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

// The player can't move while he talks or the whistles blow (as jiggy_bottles.c holds them).
static void snorkel_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void talk_start(SnorkelTalk kind) {
    talk_kind = kind;
    switch (kind) {
        case TALK_MEET:
            talk_pages = snorkel_meet;
            talk_page_count = ARRAY_COUNT_OF(snorkel_meet);
            break;
        case TALK_REMIND:
            talk_pages = snorkel_remind;
            talk_page_count = ARRAY_COUNT_OF(snorkel_remind);
            break;
        case TALK_WRONG:
            talk_pages = snorkel_wrong;
            talk_page_count = ARRAY_COUNT_OF(snorkel_wrong);
            break;
        default:
            talk_pages = snorkel_solved;
            talk_page_count = ARRAY_COUNT_OF(snorkel_solved);
            break;
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    snorkel_lock_player(TRUE);
}

static void snorkel_save(u32 bits) {
    snorkel_saved |= bits;
    jiggy_save_set(current_file, SNORKEL_SAVE_KEY(current_map), snorkel_saved);
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
        if (talk_shown / SNORKEL_TALK_SFX_EVERY != before / SNORKEL_TALK_SFX_EVERY && talk_shown < talk_length) {
            jiggy_bk_sfx_play(SNORKEL_SFX_WAH, 20000.0f / 32767.0f, 1.2f + snorkel_random() * 0.1f);
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
    // The last page: the player can move again.
    talk_open = FALSE;
    snorkel_lock_player(FALSE);
    snorkel_armed = FALSE;
    if (talk_kind == TALK_MEET) {
        snorkel_save(SNORKEL_SAVE_MET);
    } else if (talk_kind == TALK_SOLVED) {
        // His Jiggy appears, with Banjo-Kazooie's Jiggy jingle.
        solved_cutscene = FALSE;
        reward_due = TRUE;
        reward_jingle = TRUE;
    }
}

static void whistle_blow(s32 i) {
    whistle_progress[i] = 0.0f;
}

// A switch pressed (chRBBWhistleSwitch_setState and chWhistleCtrl_newEvent): it sinks, its
// whistle blows and, once Snorkel's told the player the tune, it's checked against the tune.
static void switch_press(s32 i) {
    switch_sink[i] = SWITCH_SINK;
    switch_timer[i] = SWITCH_PRESS_SECONDS;
    jiggy_bk_sfx_play(SWITCH_SFX_PRESS, 1.0f, 1.0f);
    whistle_blow(i);
    if (!(snorkel_saved & SNORKEL_SAVE_MET) || (snorkel_saved & SNORKEL_SAVE_SOLVED)) {
        return;
    }
    if (i + 1 == whistle_code[code_index]) {
        code_index++;
        if (code_index >= (s32)WHISTLE_CODE_LENGTH) {
            // The whole tune: the cutscene takes it from here.
            code_index = 0;
            snorkel_save(SNORKEL_SAVE_SOLVED);
            solved_cutscene = TRUE;
            solved_seconds = 0.0f;
            jingle_pending = 0;
            snorkel_lock_player(TRUE);
            return;
        }
        jingle_pending = JINGLE_DING;
        jingle_timer = DING_DELAY;
    } else {
        code_index = 0;
        jingle_pending = JINGLE_BUZZER;
        jingle_timer = BUZZER_DELAY;
        wrong_pending = TRUE;
    }
}

// Whether the player presses switch i this frame: landing on it, slamming it or Beak Busting it.
static s32 switch_pressed_by(s32 i, Actor *player, s32 landed) {
    f32 dy = player->y_position - switch_pos[i][1];

    if (switch_timer[i] > 0.0f || dy > SWITCH_PRESS_ABOVE || dy < -SWITCH_PRESS_BELOW ||
        distance_xz(switch_pos[i][0], switch_pos[i][2], player->x_position, player->z_position) > SWITCH_RADIUS) {
        return FALSE;
    }
    return landed || player->control_state == STATE_SIMIAN_SLAM || jiggy_banjo_air_attack(player);
}

// A frame of the whistles, the switches and Snorkel, `frames` game frames on.
static void snorkel_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = snorkel_progress;
    s32 airborne = player->y_position - player->floor > PLAYER_AIRBORNE_HEIGHT;
    s32 landed = player_was_airborne && !airborne;
    f32 distance = distance_xz(snorkel_pos[0], snorkel_pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - snorkel_floor;
    s32 i;

    if (frames == 0) {
        return;
    }
    player_was_airborne = airborne;

    // The switches: pressed, then springing back up.
    for (i = 0; i < WHISTLE_COUNT; i++) {
        if (switch_timer[i] > 0.0f) {
            switch_timer[i] -= seconds;
        }
        if (switch_sink[i] > 0.0f) {
            switch_sink[i] -= SWITCH_RISE_SPEED * seconds;
            if (switch_sink[i] < 0.0f) {
                switch_sink[i] = 0.0f;
            }
        }
        if (!talk_open && !solved_cutscene && switch_pressed_by(i, player, landed)) {
            switch_press(i);
        }
        // The whistles' blowing.
        if (whistle_progress[i] >= 0.0f) {
            f32 was = whistle_progress[i];

            whistle_progress[i] += seconds / WHISTLE_BLOW_SECONDS;
            if (passed(was * WHISTLE_BLOW_SECONDS, whistle_progress[i] * WHISTLE_BLOW_SECONDS, WHISTLE_SFX_DELAY)) {
                jiggy_bk_sfx_play(whistle_sfx[i], 32000.0f / 32767.0f, 1.0f);
            }
            if (whistle_progress[i] >= 1.0f) {
                whistle_progress[i] = -1.0f;
            }
        }
    }
    // A ding or the buzzer, a moment after the switch (then, after a wrong one, the tune again).
    if (jingle_pending != 0) {
        jingle_timer -= seconds;
        if (jingle_timer <= 0.0f) {
            jiggy_bk_jingle_play(jingle_pending, 28000.0f / 32767.0f);
            jingle_pending = 0;
            if (wrong_pending && !talk_open) {
                wrong_pending = FALSE;
                talk_start(TALK_WRONG);
            }
        }
    }
    // The tune played: the fanfare, the whistles together, Snorkel popping free, his thanks.
    if (solved_cutscene && !talk_open) {
        f32 was = solved_seconds;

        snorkel_lock_player(TRUE);
        solved_seconds += seconds;
        if (passed(was, solved_seconds, SOLVED_FANFARE_AT)) {
            jiggy_bk_jingle_play(JINGLE_PUZZLE_SOLVED, 28000.0f / 32767.0f);
        }
        for (i = 0; i < WHISTLE_COUNT; i++) {
            if (passed(was, solved_seconds, SOLVED_WHISTLE_AT + i * SOLVED_WHISTLE_STEP)) {
                whistle_blow(whistle_code[i] - 1);
            }
        }
        if (passed(was, solved_seconds, SOLVED_FREE_AT)) {
            snorkel_action = SNORKEL_FREE;
            snorkel_progress = 0.0f;
            snorkel_lap = 0.0f;
            snorkel_sfx(player, SNORKEL_SFX_WAH, 25000.0f / 32767.0f, 1.3f);
        }
        if (passed(was, solved_seconds, SOLVED_TALK_AT)) {
            talk_start(TALK_SOLVED);
        }
    }

    // Snorkel.
    if (snorkel_action == SNORKEL_STUCK) {
        snorkel_progress += seconds / SNORKEL_STUCK_SECONDS;
        if (passed(before, snorkel_progress, 0.22f)) {
            snorkel_sfx(player, SNORKEL_SFX_WAH, 15000.0f / 32767.0f, snorkel_random_between(0.8f, 0.9f));
        }
        if (passed(before, snorkel_progress, 0.54f)) {
            snorkel_sfx(player, SNORKEL_SFX_WAH, 19000.0f / 32767.0f, snorkel_random_between(0.9f, 1.0f));
        }
        while (snorkel_progress >= 1.0f) {
            snorkel_progress -= 1.0f;
        }
        // The first time the player comes near, he tells them the tune; after that, standing
        // still by him gets a reminder.
        if (!talk_open && !solved_cutscene && jingle_pending == 0 && !airborne && dy < SNORKEL_TALK_HEIGHT &&
            dy > -SNORKEL_TALK_HEIGHT) {
            if (!(snorkel_saved & SNORKEL_SAVE_MET)) {
                if (distance < SNORKEL_MEET_RADIUS &&
                    (player->control_state == STATE_IDLE || player->control_state == STATE_WALK)) {
                    talk_start(TALK_MEET);
                }
            } else if (snorkel_armed && distance < SNORKEL_REMIND_RADIUS && player->control_state == STATE_IDLE) {
                talk_start(TALK_REMIND);
            }
        }
        if (distance > SNORKEL_REARM_RADIUS) {
            snorkel_armed = TRUE;
        }
    } else {
        snorkel_progress += seconds / SNORKEL_SWIM_SECONDS;
        while (snorkel_progress >= 1.0f) {
            snorkel_progress -= 1.0f;
        }
        snorkel_lap += seconds / SWIM_LAP_SECONDS;
        while (snorkel_lap >= 1.0f) {
            snorkel_lap -= 1.0f;
        }
    }

    if (talk_open) {
        talk_update();
    }
    // His Jiggy, once he's free: the first time, it waits for the player to step off its spot,
    // so the jingle plays as it appears.
    if (reward_due && !talk_open &&
        (!reward_jingle ||
         distance_xz(player->x_position, player->z_position, snorkel_spot->reward_x, snorkel_spot->reward_z) >= REWARD_CLEARANCE)) {
        f32 y = floor_under(snorkel_spot->reward_x, snorkel_floor, snorkel_spot->reward_z);

        if (jiggy_reward_update(SNORKEL_REWARD, snorkel_spot->reward_x, y + REWARD_HEIGHT, snorkel_spot->reward_z, player,
                                frames)) {
            reward_due = FALSE;
        } else if (reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
            reward_jingle = FALSE;
        }
    }
}

// Draws a built model at a spot, turned to `yaw` (12-bit angle units) and scaled.
static Gfx *pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, const f32 pos[3], f32 yaw,
                      f32 model_scale, Gfx *gfx, u32 gfx_capacity, Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 s = BK_TO_DK64 * model_scale;
    s32 i;

    guScaleF(scale, s, s, s);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, snorkel_pose.world);
    snorkel_pose.world[3][0] = pos[0];
    snorkel_pose.world[3][1] = pos[1];
    snorkel_pose.world[3][2] = pos[2];
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        snorkel_pose.selectors[i] = 0;
    }
    snorkel_pose.selectors[1] = 1;
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    snorkel_pose.model = model;
    snorkel_pose.block = block;
    snorkel_pose.animation = animation;
    snorkel_pose.progress = progress;
    snorkel_pose.gfx = gfx;
    snorkel_pose.gfx_capacity = gfx_capacity;
    snorkel_pose.mtx = mtx;
    snorkel_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&snorkel_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); runs and draws
// Snorkel, the whistles and their switches once a frame, with the player.
Gfx *jiggy_snorkel_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    f32 pos[3];
    f32 yaw;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over).
    if ((s32)current_map != snorkel_last_map || object_timer < snorkel_last_timer) {
        // Leaving the map mid-talk or mid-cutscene lets the player go.
        if (talk_open || solved_cutscene) {
            snorkel_lock_player(FALSE);
            talk_open = FALSE;
            if (solved_cutscene) {
                // Solved but not thanked yet: his Jiggy's waiting next time.
                solved_cutscene = FALSE;
            }
        }
        snorkel_last_map = current_map;
        snorkel_placed = FALSE;
        snorkel_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(snorkel_spots); i++) {
            if (snorkel_spots[i].map == current_map) {
                snorkel_spot = &snorkel_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        snorkel_saved = snorkel_spot != NULL ? jiggy_save_get(current_file, SNORKEL_SAVE_KEY(current_map)) : 0;
    }
    if (snorkel_spot == NULL) {
        snorkel_last_timer = object_timer;
        return dl;
    }
    if (snorkel_state == SNORKEL_UNLOADED) {
        snorkel_load();
    }
    if (snorkel_state != SNORKEL_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!snorkel_placed) {
        snorkel_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        snorkel_place(actor);
    }
    frames = object_timer - snorkel_last_timer;
    snorkel_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && !solved_cutscene) {
        frames = 0;
    }
    snorkel_update(actor, frames);

    // Snorkel: stuck where he is, facing the room; or swimming round his spot.
    if (snorkel_action == SNORKEL_STUCK) {
        pos[0] = snorkel_pos[0];
        pos[1] = snorkel_pos[1];
        pos[2] = snorkel_pos[2];
        yaw = 0.0f;
        dl = pose_draw(dl, SNORKEL_MODEL, snorkel_block, SNORKEL_ANIM_STUCK, snorkel_progress, pos, yaw, 1.0f,
                       snorkel_gfx[buffer], SNORKEL_GFX_MAX, snorkel_mtx[buffer], SNORKEL_MTX_MAX);
    } else {
        s16 angle = (s16)(snorkel_lap * 4096.0f) & 0xFFF;

        pos[0] = snorkel_pos[0] + func_global_asm_80612794(angle) * SWIM_RADIUS;
        pos[1] = snorkel_pos[1] + SWIM_HEIGHT + func_global_asm_80612794((s16)(snorkel_lap * 4096.0f * 3.0f) & 0xFFF) * SWIM_BOB;
        pos[2] = snorkel_pos[2] + func_global_asm_80612790(angle) * SWIM_RADIUS;
        // Facing along the circle (a quarter turn on from facing out of it).
        yaw = (f32)((angle + 0x400) & 0xFFF);
        dl = pose_draw(dl, SNORKEL_MODEL, snorkel_block, SNORKEL_ANIM_SWIM, snorkel_progress, pos, yaw, 1.0f,
                       snorkel_gfx[buffer], SNORKEL_GFX_MAX, snorkel_mtx[buffer], SNORKEL_MTX_MAX);
    }
    for (i = 0; i < WHISTLE_COUNT; i++) {
        dl = pose_draw(dl, WHISTLE_MODEL, whistle_block, whistle_progress[i] >= 0.0f ? whistle_anims[i] : -1,
                       whistle_progress[i] >= 0.0f ? whistle_progress[i] : 0.0f, whistle_pos[i], 0.0f, WHISTLE_SCALE,
                       whistle_gfx[buffer][i], WHISTLE_GFX_MAX, whistle_mtx[buffer][i], WHISTLE_MTX_MAX);
        pos[0] = switch_pos[i][0];
        pos[1] = switch_pos[i][1] - switch_sink[i];
        pos[2] = switch_pos[i][2];
        dl = pose_draw(dl, SWITCH_MODEL_1 + i, switch_blocks[i], -1, 0.0f, pos, 0.0f, 1.0f, switch_gfx[buffer][i],
                       SWITCH_GFX_MAX, switch_mtx[buffer][i], SWITCH_MTX_MAX);
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

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): his text
// box while he talks (drawn as jiggy_bottles_hud_draw draws Bottles').
Gfx *jiggy_snorkel_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (!talk_open || snorkel_state != SNORKEL_READY || cc_number_of_players != 1) {
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
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"SNORKEL", 1);
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
