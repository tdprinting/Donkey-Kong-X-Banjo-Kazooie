// Nabnut: Click Clock Wood's squirrel, read from the player's Banjo-Kazooie ROM, as an NPC on
// Fungi Forest's mill plateau, where the two worlds have collided; with his acorn quest.
//
// As in Click Clock Wood's autumn (chAutumnOutdoorNabnut, src/CCW/ch/outsideautumnnabnut.c),
// Nabnut (ASSET_502_MODEL_NABNUT, drawn with that actor's appendages) sobs over his lost acorns
// (his cry animation) until the player brings him all of them. Click Clock Wood ties his quest
// to its seasons; here it's tied to Fungi Forest's day and night (DK64's
// PERMFLAG_PROGRESS_IS_NIGHTTIME): his five acorns (ASSET_48E_MODEL_ACORN, bobbing with
// chCarriedAcorn's animation) only glint by moonlight, so they're out at night only, scattered
// over the forest: by the mill, on the barn's plateau, past the mill's south slope, on the path
// up from the giant mushroom and on the walkway round the giant mushroom's west side.
//
// Touching an acorn takes it (Banjo-Kazooie's boing and pop, as chCarriedAcorn plays them),
// shown in the HUD with the count until it's given to him. Coming up to him with acorns, by day
// or night, he talks in a text box and takes them (Banjo-Kazooie's ding, COMUSIC_2B, as an acorn
// reaching him plays it); the fifth has him backflip for joy (his backflip, with Banjo-Kazooie's
// puzzle solved fanfare), thank the player, and give a Jiggy (a Golden Banana drawn as a Jiggy by
// jiggy_collectibles.c: reward 0x51 of jiggy_rewards.c) that appears beside him with
// Banjo-Kazooie's Jiggy jingle, and his mound of acorns (ASSET_501_MODEL_MOUND_OF_ACORNS) sits
// beside him from then on, him standing proud by it. The player is held in the game's cutscene
// lock from his talk until the Jiggy's due.
//
// Coming up to him otherwise, standing or walking (only standing, for a repeat), he talks: asks
// for his acorns (by day, telling the player to come back at night), or (after) thanks them.
// Repeats follow Bottles' reminder rule (jiggy_bottles.c).
//
// The acorns taken, how many he has, his first talk and the quest done are kept with the DK64
// save file (jiggy_save_get/set, key 0x54000000 | map: Fungi Forest's characters, Nabnut index 1
// in bits 16-23).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his text
// box and the acorn count in the HUD pass (jiggy_nabnut_hud_draw, patches_ui.c). Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define NABNUT_MODEL 0x502   // ASSET_502_MODEL_NABNUT
#define ACORN_MODEL 0x48E    // ASSET_48E_MODEL_ACORN
#define MOUND_MODEL 0x501    // ASSET_501_MODEL_MOUND_OF_ACORNS
#define NABNUT_PORTRAIT 0x831 // ASSET_831_SPRITE_NABNUT: also the HUD icon
// Nabnut converts to 20360 bytes (737 vertices, 42 bones, 8 pose commands); an acorn to 4696
// (3 bones); the mound to 4704 (no bones).
#define NABNUT_BLOCK_MAX 0x5000
#define NABNUT_GFX_MAX 32
#define NABNUT_MTX_MAX 46
#define ACORN_BLOCK_MAX 0x1300
#define ACORN_GFX_MAX 16
#define ACORN_MTX_MAX 6
#define MOUND_BLOCK_MAX 0x1300
#define MOUND_GFX_MAX 16
#define MOUND_MTX_MAX 2

// Banjo-Kazooie animations (chAutumnOutdoorNabnut's, chCarriedAcorn's) and how long each plays,
// in seconds.
#define NABNUT_ANIM_CRY 0x22C      // ASSET_22C_ANIM_NABNUT_CRY
#define NABNUT_ANIM_BACKFLIP 0x22D // ASSET_22D_ANIM_NABNUT_BACKFLIP
#define NABNUT_ANIM_STAND 0x22E    // ASSET_22E_ANIM_NABNUT_STAND
#define NABNUT_CRY_SECONDS 10.6f
#define NABNUT_BACKFLIP_SECONDS 3.13f
#define NABNUT_STAND_SECONDS 3.53f
#define ACORN_ANIM_IDLE 0x25B // ASSET_25B_ANIM_ACORN_IDLE
#define ACORN_IDLE_SECONDS 1.0f

// Banjo-Kazooie sounds (sfx_e).
#define SFX_HEGH 0xC3          // His voice in the text box (gczoombox's Nabnut): 20000/32767, 1.7
#define SFX_BOING 0x3F2        // An acorn taken (chCarriedAcorn): 26000/32767, 0.95-1.05
#define SFX_TWINKLY_POP 0xC5   // An acorn taken (chCarriedAcorn): 25000/32767, 1.0
#define NABNUT_TALK_SFX_EVERY 5
// Banjo-Kazooie's jingles: acorns given (COMUSIC_2B_DING_B, 28000/32767), the puzzle solved
// fanfare (COMUSIC_2D) and the Jiggy appearing (COMUSIC_3D_JIGGY_SPAWN).
#define JINGLE_DING 0x2B
#define JINGLE_PUZZLE_SOLVED 0x2D
#define JINGLE_JIGGY_SPAWN 0x3D

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo (Nabnut about 60 units
// tall); the acorns at chCarriedAcorn's 0.8, about 20 tall.
#define BK_TO_DK64 0.25f
#define ACORN_SCALE (0.8f * BK_TO_DK64)
#define GAME_FPS 30.0f
#define NABNUT_TURN_RATE 0.1f
#define ACORN_SPIN_PER_FRAME 24.0f // 12-bit angle units.

#define NABNUT_TALK_RADIUS 75.0f
#define NABNUT_REARM_RADIUS 250.0f
#define NABNUT_REPEAT_FRAMES (60 * 30)
#define NABNUT_TALK_FEET 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
#define NABNUT_RADIUS 18.0f
#define PLAYER_RADIUS 10.0f
#define FLOOR_PROBE 30.0f
#define FLOOR_MAX_STEP 50.0f

#define ACORN_COUNT 5
#define ACORN_TAKE_RADIUS 22.0f
#define ACORN_TAKE_HEIGHT 40.0f

#define FLAG_NIGHTTIME 0xCE // DK64's PERMFLAG_PROGRESS_IS_NIGHTTIME
#define FLAG_TYPE_PERMANENT 0
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
#define CUTSCENE_LOCK 0x10

// His Jiggy: jiggy_rewards.c reward 0x51 (Fungi Forest's range is 0x50-0x5F).
#define NABNUT_REWARD_ID 0x51
#define REWARD_HEIGHT 6.0f

// The text box (as jiggy_bottles.c draws it), in screen pixels.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 5
#define PORTRAIT_MAX_TEXELS (32 * 32)
// Frames 0 to 4 move his mouth (gczoombox's 5 talking frames for Nabnut).
#define PORTRAIT_TALK_FRAMES 5
#define PORTRAIT_TICKS_PER_FRAME 2
#define TEXT_STYLE 6
#define TEXT_LINE_HEIGHT 17
#define TEXT_NAME_Y 6
#define TEXT_FIRST_LINE_Y 24
#define TEXT_Y(y) (((y) - 117) * 4 / 5 + 120)
#define TEXT_MAX_LINES 4
#define TEXT_LINE_MAX 48
#define TEXT_CHARS_PER_FRAME 1
#define BOX_FADE_STEP 0x30
#define BOX_ALPHA 0xC8

// The acorns carried, in the HUD (the right-hand column, as jiggy_eyrie.c's caterpillars).
#define HUD_ICON_SIZE 32
#define HUD_RIGHT_MARGIN 56
#define HUD_Y (96 + 3 * 36)
#define HUD_FADE_STEP 0x20

// Where he is. Fungi Forest: on the mill's plateau east of the mill (DK64's floor at 240), inside
// the notes' ring; his mound beside him and his Jiggy just west. The acorns, on DK64's floor:
// west of the mill (249), on the barn's plateau (191), on the slope south of the mill (289), on
// the path up from the giant mushroom (179), and on the walkway round the giant mushroom's
// west side (179).
typedef struct {
    Maps map;
    f32 x, y, z;
    f32 mound_x, mound_z;
    f32 reward_x, reward_z;
    f32 acorns[ACORN_COUNT][3];
} NabnutPlacement;
static const NabnutPlacement nabnut_spots[] = {
    {MAP_FUNGI, 1630.0f, 240.0f, 3850.0f, 1665.0f, 3815.0f, 1590.0f, 3800.0f,
     {{1020.0f, 249.0f, 3780.0f},
      {650.0f, 191.0f, 3950.0f},
      {1100.0f, 289.0f, 4700.0f},
      {1400.0f, 179.0f, 3000.0f},
      {350.0f, 179.0f, 1750.0f}}},
};

static const char *const nabnut_ask_day[] = {
    "OH DEAR, OH DEAR! I'M NABNUT, AND MY WINTER ACORNS HAVE ROLLED AWAY!",
    "THEY'RE TOO DULL TO SPOT BY DAY. THEY ONLY GLINT UNDER THE MOON...",
    "COME BACK AT NIGHT AND FIND ALL FIVE FOR ME?",
};
static const char *const nabnut_ask_night[] = {
    "OH DEAR, OH DEAR! I'M NABNUT, AND MY WINTER ACORNS HAVE ROLLED AWAY!",
    "NOW THE MOON'S UP YOU CAN SEE THEM GLINT. FIVE OF THEM, ALL OVER THE FOREST!",
};
static const char *const nabnut_deliver[] = {
    "MY ACORNS! OH, THANK YOU, THANK YOU!",
};
static const char *const nabnut_more_4[] = {"FOUR MORE TO FIND, PLEASE!"};
static const char *const nabnut_more_3[] = {"THREE MORE TO FIND, PLEASE!"};
static const char *const nabnut_more_2[] = {"ONLY TWO MORE TO GO!"};
static const char *const nabnut_more_1[] = {"JUST ONE MORE AND MY LARDER'S FULL!"};
static const char *const nabnut_done_text[] = {
    "HOORAY! ENOUGH ACORNS FOR THE WHOLE WINTER!",
    "I FOUND THIS SHINY THING IN MY STASH. YOU HAVE IT!",
};
static const char *const nabnut_thanks[] = {
    "MY LARDER'S FULL! I CAN SLEEP ALL WINTER NOW.",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): NABNUT_SAVE_* bits, the acorns taken (one bit each from
// NABNUT_SAVE_TAKEN_SHIFT) and how many he has (NABNUT_SAVE_GIVEN_SHIFT, 4 bits).
#define NABNUT_SAVE_MAP_KEY(map) (0x54000000 | (1u << 16) | (u32)(map))
#define NABNUT_SAVE_MET 0x1
#define NABNUT_SAVE_DONE 0x2
#define NABNUT_SAVE_TAKEN_SHIFT 4
#define NABNUT_SAVE_GIVEN_SHIFT 12
#define NABNUT_SAVE_GIVEN_MASK (0xFu << NABNUT_SAVE_GIVEN_SHIFT)

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
extern OSContPad D_global_asm_807ECD58;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z);
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
u8 isFlagSet(s16 flagIndex, u8 flagType);

typedef enum {
    NABNUT_UNLOADED,
    NABNUT_READY,
    NABNUT_UNAVAILABLE,
} NabnutLoadState;

typedef enum {
    NABNUT_IDLE,     // Crying, or (after) standing proud.
    NABNUT_BACKFLIP, // For joy, with the fifth acorn.
} NabnutAction;

typedef enum {
    TALK_ASK,
    TALK_DELIVER,
    TALK_MORE,
    TALK_DONE,
    TALK_THANKS,
} NabnutTalk;

static u8 nabnut_block[NABNUT_BLOCK_MAX] __attribute__((aligned(16)));
static u8 acorn_block[ACORN_BLOCK_MAX] __attribute__((aligned(16)));
static u8 mound_block[MOUND_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx nabnut_gfx[2][NABNUT_GFX_MAX];
static Mtx nabnut_mtx[2][NABNUT_MTX_MAX];
static Gfx acorn_gfx[2][ACORN_COUNT][ACORN_GFX_MAX];
static Mtx acorn_mtx[2][ACORN_COUNT][ACORN_MTX_MAX];
static Gfx mound_gfx[2][MOUND_GFX_MAX];
static Mtx mound_mtx[2][MOUND_MTX_MAX];
static JiggyBkPose nabnut_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static s32 hud_alpha;
static NabnutLoadState nabnut_state = NABNUT_UNLOADED;

static s32 nabnut_last_map = -1;
static u32 nabnut_last_timer;
static const NabnutPlacement *nabnut_spot;
static s32 nabnut_placed;
static f32 nabnut_pos[3];
static f32 nabnut_yaw;
static NabnutAction nabnut_action;
static f32 nabnut_progress;
static s32 nabnut_holding;
static s32 nabnut_armed;
static u32 nabnut_last_talk;
static u32 nabnut_saved;
static s32 nabnut_night;
static f32 acorn_pos[ACORN_COUNT][3];
static f32 acorn_yaw;
static f32 acorn_progress;
static f32 mound_pos[3];
static f32 reward_spot[3];
static s32 reward_jingle;
static u32 nabnut_random_state = 0x4E41424E;

static NabnutTalk talk_kind;
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static s32 talk_open;
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_length;
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_a_held = TRUE;

static f32 nabnut_random(void) {
    nabnut_random_state = nabnut_random_state * 1664525 + 1013904223;
    return (nabnut_random_state >> 8) * (1.0f / 16777216.0f);
}

static s32 nabnut_build(u32 model, u8 *block, u32 capacity) {
    s32 size = jiggy_bk_model_build(model, block, capacity);

    return size > 0 && size <= (s32)capacity;
}

static void nabnut_load(void) {
    s32 count;
    s32 i;

    nabnut_state = NABNUT_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    if (!nabnut_build(NABNUT_MODEL, nabnut_block, sizeof(nabnut_block)) ||
        !nabnut_build(ACORN_MODEL, acorn_block, sizeof(acorn_block)) ||
        !nabnut_build(MOUND_MODEL, mound_block, sizeof(mound_block))) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(NABNUT_PORTRAIT);
    if (count <= 0) {
        return;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(NABNUT_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
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
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        nabnut_pose.selectors[i] = 0;
    }
    nabnut_pose.selectors[1] = 1;
    nabnut_state = NABNUT_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

static f32 nabnut_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < FLOOR_MAX_STEP && y - floor < FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

static s32 nabnut_done(void) {
    return (nabnut_saved & NABNUT_SAVE_DONE) != 0;
}

static s32 acorn_taken(s32 i) {
    return (nabnut_saved >> (NABNUT_SAVE_TAKEN_SHIFT + i)) & 1;
}

static s32 nabnut_given(void) {
    return (nabnut_saved & NABNUT_SAVE_GIVEN_MASK) >> NABNUT_SAVE_GIVEN_SHIFT;
}

static s32 nabnut_carried(void) {
    s32 taken = 0;
    s32 i;

    for (i = 0; i < ACORN_COUNT; i++) {
        taken += acorn_taken(i);
    }
    return taken > nabnut_given() ? taken - nabnut_given() : 0;
}

static void nabnut_save(void) {
    jiggy_save_set(current_file, NABNUT_SAVE_MAP_KEY(current_map), nabnut_saved);
}

static void nabnut_start(NabnutAction action, f32 progress) {
    nabnut_action = action;
    nabnut_progress = progress;
}

static void nabnut_place(Actor *player) {
    s32 i;

    nabnut_pos[0] = nabnut_spot->x;
    nabnut_pos[1] = nabnut_floor(nabnut_spot->x, nabnut_spot->y, nabnut_spot->z);
    nabnut_pos[2] = nabnut_spot->z;
    mound_pos[0] = nabnut_spot->mound_x;
    mound_pos[1] = nabnut_floor(nabnut_spot->mound_x, nabnut_spot->y, nabnut_spot->mound_z);
    mound_pos[2] = nabnut_spot->mound_z;
    reward_spot[0] = nabnut_spot->reward_x;
    reward_spot[1] = nabnut_floor(nabnut_spot->reward_x, nabnut_spot->y, nabnut_spot->reward_z) + REWARD_HEIGHT;
    reward_spot[2] = nabnut_spot->reward_z;
    for (i = 0; i < ACORN_COUNT; i++) {
        acorn_pos[i][0] = nabnut_spot->acorns[i][0];
        acorn_pos[i][1] = nabnut_floor(nabnut_spot->acorns[i][0], nabnut_spot->acorns[i][1], nabnut_spot->acorns[i][2]);
        acorn_pos[i][2] = nabnut_spot->acorns[i][2];
    }
    reward_jingle = FALSE;
    nabnut_holding = FALSE;
    nabnut_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, nabnut_pos[0], nabnut_pos[2]) & 0xFFF;
    nabnut_start(NABNUT_IDLE, nabnut_random());
    nabnut_placed = TRUE;
}

static void talk_set_page(s32 page) {
    const char *text = talk_pages[page];
    s32 text_width = BOX_WIDTH - 2 * BOX_BORDER - BOX_PAD - PORTRAIT_SCALE * 32 - 2 * BOX_PAD;
    char candidate[TEXT_LINE_MAX];
    s32 line = 0;
    s32 length = 0;

    talk_page = page;
    talk_lines[0][0] = '\0';
    while (*text != '\0' && line < TEXT_MAX_LINES) {
        s32 word_length = 0;
        s32 i;
        s32 n;

        while (text[word_length] != '\0' && text[word_length] != ' ') {
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
            candidate[n++] = text[i];
        }
        candidate[n] = '\0';
        if (talk_lines[line][0] != '\0' && getCenterOfString(TEXT_STYLE, (u8 *)candidate) > text_width) {
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

static void nabnut_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void talk_start(NabnutTalk kind) {
    s32 left = ACORN_COUNT - nabnut_given();

    talk_kind = kind;
    switch (kind) {
        case TALK_ASK:
            if (nabnut_night) {
                talk_pages = nabnut_ask_night;
                talk_page_count = ARRAY_COUNT_OF(nabnut_ask_night);
            } else {
                talk_pages = nabnut_ask_day;
                talk_page_count = ARRAY_COUNT_OF(nabnut_ask_day);
            }
            break;
        case TALK_DELIVER:
            talk_pages = nabnut_deliver;
            talk_page_count = ARRAY_COUNT_OF(nabnut_deliver);
            break;
        case TALK_MORE:
            talk_pages = left >= 4 ? nabnut_more_4 : left == 3 ? nabnut_more_3 : left == 2 ? nabnut_more_2 : nabnut_more_1;
            talk_page_count = 1;
            break;
        case TALK_DONE:
            talk_pages = nabnut_done_text;
            talk_page_count = ARRAY_COUNT_OF(nabnut_done_text);
            break;
        default:
            talk_pages = nabnut_thanks;
            talk_page_count = ARRAY_COUNT_OF(nabnut_thanks);
            break;
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    talk_a_held = TRUE;
    nabnut_lock_player(TRUE);
}

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
        if (talk_shown / NABNUT_TALK_SFX_EVERY != before / NABNUT_TALK_SFX_EVERY && talk_shown < talk_length) {
            jiggy_bk_sfx_play(SFX_HEGH, 20000.0f / 32767.0f, 1.65f + nabnut_random() * 0.1f);
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
    if (talk_kind == TALK_DELIVER) {
        // He takes what he's brought.
        s32 given = nabnut_given() + nabnut_carried();

        if (given > ACORN_COUNT) {
            given = ACORN_COUNT;
        }
        nabnut_saved = (nabnut_saved & ~NABNUT_SAVE_GIVEN_MASK) | ((u32)given << NABNUT_SAVE_GIVEN_SHIFT);
        jiggy_bk_jingle_play(JINGLE_DING, 28000.0f / 32767.0f);
        if (given >= ACORN_COUNT) {
            // All five: a backflip for joy, and the player's held until his thanks are done.
            nabnut_saved |= NABNUT_SAVE_DONE;
            nabnut_save();
            nabnut_holding = TRUE;
            jiggy_bk_jingle_play(JINGLE_PUZZLE_SOLVED, 1.0f);
            jiggy_bk_sfx_play(SFX_BOING, 26000.0f / 32767.0f, 1.0f);
            nabnut_start(NABNUT_BACKFLIP, 0.0f);
            return;
        }
        nabnut_save();
        talk_start(TALK_MORE);
        return;
    }
    if (talk_kind == TALK_DONE) {
        reward_jingle = TRUE;
    }
    nabnut_holding = FALSE;
    nabnut_lock_player(FALSE);
}

static s32 nabnut_player_near(Actor *player, f32 radius) {
    f32 dy = nabnut_pos[1] - player->y_position;

    return distance_xz(nabnut_pos[0], nabnut_pos[2], player->x_position, player->z_position) <= radius &&
           dy >= -NABNUT_TALK_FEET && dy <= NABNUT_TALK_FEET;
}

static void nabnut_check_talk(Actor *player) {
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;
    s32 standing = player->control_state == STATE_IDLE;
    s32 walking = player->control_state == STATE_WALK;
    s16 to_nabnut;

    if (!nabnut_armed) {
        nabnut_armed = !nabnut_player_near(player, NABNUT_REARM_RADIUS) &&
                       (!(nabnut_saved & NABNUT_SAVE_MET) || object_timer - nabnut_last_talk >= NABNUT_REPEAT_FRAMES);
    }
    if (!nabnut_player_near(player, NABNUT_TALK_RADIUS) || !grounded) {
        return;
    }
    if (!nabnut_done() && nabnut_carried() > 0 && (standing || walking)) {
        nabnut_saved |= NABNUT_SAVE_MET;
        nabnut_save();
        talk_start(TALK_DELIVER);
    } else if (nabnut_armed && (standing || (walking && !(nabnut_saved & NABNUT_SAVE_MET)))) {
        nabnut_saved |= NABNUT_SAVE_MET;
        nabnut_save();
        talk_start(nabnut_done() ? TALK_THANKS : TALK_ASK);
    } else {
        return;
    }
    nabnut_armed = FALSE;
    nabnut_last_talk = object_timer;
    to_nabnut = func_global_asm_80665DE0(nabnut_pos[0], nabnut_pos[2], player->x_position, player->z_position);
    player->y_rotation = to_nabnut & 0xFFF;
    player->unkEE = to_nabnut & 0xFFF;
}

static void nabnut_turn_to(f32 x, f32 z, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, z, nabnut_pos[0], nabnut_pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - nabnut_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        nabnut_yaw += turn * NABNUT_TURN_RATE;
        if (nabnut_yaw < 0.0f) {
            nabnut_yaw += 4096.0f;
        } else if (nabnut_yaw >= 4096.0f) {
            nabnut_yaw -= 4096.0f;
        }
    }
}

// The acorns, out at night until taken.
static void acorns_update(Actor *player, u32 frames) {
    s32 i;

    acorn_yaw += ACORN_SPIN_PER_FRAME * frames;
    while (acorn_yaw >= 4096.0f) {
        acorn_yaw -= 4096.0f;
    }
    acorn_progress += frames / GAME_FPS / ACORN_IDLE_SECONDS;
    while (acorn_progress >= 1.0f) {
        acorn_progress -= 1.0f;
    }
    if (!nabnut_night || nabnut_done()) {
        return;
    }
    for (i = 0; i < ACORN_COUNT; i++) {
        f32 dy;

        if (acorn_taken(i)) {
            continue;
        }
        dy = player->y_position - acorn_pos[i][1];
        if (dy > -ACORN_TAKE_HEIGHT && dy < ACORN_TAKE_HEIGHT &&
            distance_xz(acorn_pos[i][0], acorn_pos[i][2], player->x_position, player->z_position) <= ACORN_TAKE_RADIUS) {
            nabnut_saved |= 1u << (NABNUT_SAVE_TAKEN_SHIFT + i);
            nabnut_save();
            jiggy_bk_sfx_play(SFX_BOING, 26000.0f / 32767.0f, 0.95f + nabnut_random() * 0.1f);
            jiggy_bk_sfx_play(SFX_TWINKLY_POP, 25000.0f / 32767.0f, 1.0f);
        }
    }
}

static void nabnut_push_player(Actor *player) {
    f32 dx = player->x_position - nabnut_pos[0];
    f32 dz = player->z_position - nabnut_pos[2];
    f32 reach = NABNUT_RADIUS + PLAYER_RADIUS;
    f32 distance = _sqrtf(dx * dx + dz * dz);

    if (distance >= reach || distance < 0.01f || player->y_position > nabnut_pos[1] + NABNUT_TALK_FEET) {
        return;
    }
    player->x_position = nabnut_pos[0] + dx * reach / distance;
    player->z_position = nabnut_pos[2] + dz * reach / distance;
}

static void nabnut_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;

    if (frames == 0) {
        return;
    }
    acorns_update(player, frames);
    nabnut_push_player(player);
    if (!talk_open && !nabnut_holding) {
        nabnut_check_talk(player);
    }
    switch (nabnut_action) {
        case NABNUT_IDLE:
            nabnut_turn_to(player->x_position, player->z_position, frames);
            nabnut_progress += seconds / (nabnut_done() ? NABNUT_STAND_SECONDS : NABNUT_CRY_SECONDS);
            while (nabnut_progress >= 1.0f) {
                nabnut_progress -= 1.0f;
            }
            break;
        case NABNUT_BACKFLIP:
            nabnut_lock_player(TRUE);
            nabnut_progress += seconds / NABNUT_BACKFLIP_SECONDS;
            if (nabnut_progress >= 1.0f) {
                nabnut_start(NABNUT_IDLE, 0.0f);
                talk_start(TALK_DONE);
            }
            break;
    }
    if (nabnut_done() && !nabnut_holding && !talk_open) {
        if (!jiggy_reward_update(NABNUT_REWARD_ID, reward_spot[0], reward_spot[1], reward_spot[2], player, frames) &&
            reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        }
        reward_jingle = FALSE;
    }
    if (talk_open) {
        talk_update();
    }
}

static Gfx *nabnut_pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, const f32 pos[3], f32 yaw,
                             f32 scale, Gfx *gfx, u32 gfx_capacity, Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 scaling[4][4];

    guScaleF(scaling, scale, scale, scale);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scaling, rotate, nabnut_pose.world);
    nabnut_pose.world[3][0] = pos[0];
    nabnut_pose.world[3][1] = pos[1];
    nabnut_pose.world[3][2] = pos[2];
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    nabnut_pose.model = model;
    nabnut_pose.block = block;
    nabnut_pose.animation = animation;
    nabnut_pose.progress = progress;
    nabnut_pose.gfx = gfx;
    nabnut_pose.gfx_capacity = gfx_capacity;
    nabnut_pose.mtx = mtx;
    nabnut_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&nabnut_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Nabnut's appendages, as __chAutumnOutsideNabnut_draw sets them (`on` TRUE), or all hidden
// past the first (for the acorns and the mound).
static void nabnut_selectors(s32 on) {
    nabnut_pose.selectors[3] = 0;
    nabnut_pose.selectors[4] = 0;
    nabnut_pose.selectors[5] = on ? 1 : 0;
    nabnut_pose.selectors[6] = 0;
    nabnut_pose.selectors[7] = on ? 1 : 0;
    nabnut_pose.selectors[8] = on ? 1 : 0;
    nabnut_pose.selectors[9] = 0;
    nabnut_pose.selectors[10] = on ? 1 : 0;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// Nabnut, his acorns and his mound once a frame, with the player.
Gfx *jiggy_nabnut_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    s32 animation;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != nabnut_last_map || object_timer < nabnut_last_timer) {
        if (talk_open || nabnut_holding) {
            nabnut_lock_player(FALSE);
            talk_open = FALSE;
            nabnut_holding = FALSE;
        }
        nabnut_last_map = current_map;
        nabnut_placed = FALSE;
        nabnut_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(nabnut_spots); i++) {
            if (nabnut_spots[i].map == current_map) {
                nabnut_spot = &nabnut_spots[i];
            }
        }
        nabnut_saved = nabnut_spot != NULL ? jiggy_save_get(current_file, NABNUT_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (nabnut_spot == NULL) {
        nabnut_last_timer = object_timer;
        return dl;
    }
    if (nabnut_state == NABNUT_UNLOADED) {
        nabnut_load();
    }
    if (nabnut_state != NABNUT_READY) {
        return dl;
    }
    nabnut_night = isFlagSet(FLAG_NIGHTTIME, FLAG_TYPE_PERMANENT) != 0;
    if (!nabnut_placed) {
        nabnut_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        nabnut_place(actor);
        nabnut_armed = !nabnut_player_near(actor, NABNUT_REARM_RADIUS);
        nabnut_last_talk = object_timer - NABNUT_REPEAT_FRAMES;
    }
    frames = object_timer - nabnut_last_timer;
    nabnut_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && !nabnut_holding) {
        frames = 0;
    }
    nabnut_update(actor, frames);

    if (nabnut_action == NABNUT_BACKFLIP) {
        animation = NABNUT_ANIM_BACKFLIP;
    } else {
        animation = nabnut_done() ? NABNUT_ANIM_STAND : NABNUT_ANIM_CRY;
    }
    nabnut_selectors(TRUE);
    dl = nabnut_pose_draw(dl, NABNUT_MODEL, nabnut_block, animation, nabnut_progress, nabnut_pos, nabnut_yaw, BK_TO_DK64,
                          nabnut_gfx[buffer], NABNUT_GFX_MAX, nabnut_mtx[buffer], NABNUT_MTX_MAX);
    nabnut_selectors(FALSE);
    if (nabnut_done()) {
        dl = nabnut_pose_draw(dl, MOUND_MODEL, mound_block, -1, 0.0f, mound_pos, 0.0f, BK_TO_DK64, mound_gfx[buffer],
                              MOUND_GFX_MAX, mound_mtx[buffer], MOUND_MTX_MAX);
    } else if (nabnut_night) {
        for (i = 0; i < ACORN_COUNT; i++) {
            if (acorn_taken(i)) {
                continue;
            }
            dl = nabnut_pose_draw(dl, ACORN_MODEL, acorn_block, ACORN_ANIM_IDLE, acorn_progress, acorn_pos[i],
                                  acorn_yaw + i * 700.0f, ACORN_SCALE, acorn_gfx[buffer][i], ACORN_GFX_MAX,
                                  acorn_mtx[buffer][i], ACORN_MTX_MAX);
        }
    }
    return dl;
}

static Gfx *box_fill(Gfx *dl, s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, r, g, b, a);
    gDPFillRectangle(dl++, x0, y0, x1, y1);
    return dl;
}

// The acorns carried, in the HUD: Nabnut's head and "X n", fading in and out.
static Gfx *hud_count_draw(Gfx *dl) {
    s32 carried = nabnut_spot != NULL && !nabnut_done() ? nabnut_carried() : 0;
    s32 x;
    char text[4];

    if (carried > 0) {
        hud_alpha = hud_alpha + HUD_FADE_STEP > 0xFF ? 0xFF : hud_alpha + HUD_FADE_STEP;
    } else {
        hud_alpha = hud_alpha - HUD_FADE_STEP < 0 ? 0 : hud_alpha - HUD_FADE_STEP;
    }
    if (hud_alpha == 0) {
        return dl;
    }
    if (carried == 0) {
        carried = 1;
    }
    x = D_global_asm_80744490 - HUD_RIGHT_MARGIN - HUD_ICON_SIZE - 32;
    dl = alignHUDGameplay(dl, ALIGN_RIGHT);
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, hud_alpha);
    gDPLoadTextureTile(dl++, portrait_texels[0], G_IM_FMT_RGBA, G_IM_SIZ_16b, portrait_width[0], portrait_height[0], 0, 0,
                       portrait_width[0] - 1, portrait_height[0] - 1, 0, G_TX_CLAMP, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(dl++, x << 2, HUD_Y << 2, (x + HUD_ICON_SIZE) << 2, (HUD_Y + HUD_ICON_SIZE) << 2, G_TX_RENDERTILE,
                        0, 0, (portrait_width[0] << 10) / HUD_ICON_SIZE, (portrait_height[0] << 10) / HUD_ICON_SIZE);
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_RIGHT);
    text[0] = 'X';
    text[1] = ' ';
    text[2] = '0' + (carried % 10);
    text[3] = '\0';
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, hud_alpha);
    dl = printStyledText(dl, TEXT_STYLE, (x + HUD_ICON_SIZE + 4) * 4, TEXT_Y(HUD_Y + 8) * 4, (u8 *)text, 1);
    gDPPipeSync(dl++);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the acorns
// carried, and his text box while he talks (drawn as jiggy_bottles_hud_draw draws Bottles').
Gfx *jiggy_nabnut_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (nabnut_state != NABNUT_READY || cc_number_of_players != 1) {
        return dl;
    }
    dl = hud_count_draw(dl);
    if (!talk_open) {
        return dl;
    }
    box_x = (D_global_asm_80744490 - BOX_WIDTH) / 2;
    box_y = D_global_asm_80744494 - BOX_BOTTOM_MARGIN - BOX_HEIGHT;

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
    gDPLoadTextureTile(dl++, portrait_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b, width, height, 0, 0, width - 1,
                       height - 1, 0, G_TX_CLAMP, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    {
        s32 px = box_x + BOX_BORDER + BOX_PAD;
        s32 py = box_y + (BOX_HEIGHT - height * PORTRAIT_SCALE) / 2;

        gSPTextureRectangle(dl++, px << 2, py << 2, (px + width * PORTRAIT_SCALE) << 2, (py + height * PORTRAIT_SCALE) << 2,
                            G_TX_RENDERTILE, 0, 0, (1 << 10) / PORTRAIT_SCALE, (1 << 10) / PORTRAIT_SCALE);
    }
    gDPPipeSync(dl++);

    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SCALE * 32 + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"NABNUT", 1);
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
