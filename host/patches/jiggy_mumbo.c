// Mumbo Jumbo: Banjo-Kazooie's shaman, read from the player's Banjo-Kazooie ROM, standing
// outside his skull hut in the Training Grounds, where the Mumbo Tokens (jiggy_mumbo_tokens.c)
// can be spent.
//
// Like chMumbo (src/core2/ch/mumbo.c), Mumbo (ASSET_3C6_MODEL_MUMBO, its awake branches shown
// as chMumbo_draw shows them) idles with his idle animation, turning to face the player when
// they're about. His hut is Banjo-Kazooie's skull hut (ASSET_3B3_MODEL_CCW_MUMBOS_HUT, the
// skull with feathers and a chimney that chCCWMumbosHut draws in Click Clock Wood; Mumbo's
// Mountain's skull is part of that level's own map model), a still model behind him with its
// mouth, the door, toward him. It has no collision of its own in DK64: the player is only
// kept out of it, pushed back to its edge.
//
// When the player stops by him he talks, in Bottles' text box (jiggy_bottles.c): Banjo-
// Kazooie's Mumbo portrait (ASSET_7FC_SPRITE_MUMBO, its mouth moving as the text types out)
// beside DK64's dialogue font, with Mumbo's talking sounds (SFX_407 to SFX_40B, as gczoombox
// plays them), his "erghhh" as he notices them (SFX_41_MUMBO_ERGHHH) and the game's cutscene
// lock holding the player. With fewer than four Mumbo Tokens he asks for four and says how many
// the player has. With four or more he takes four, as Banjo-Kazooie's Mumbo takes tokens for a
// transformation (item_adjustByDiffWithHud, the token counter coming up as the count drops),
// and does his spell (chMumbo's state 5: ASSET_6D_ANIM_MUMBO_TRANSFORM, the transformation
// music COMUSIC_1D_MUMBO_TRANSFORMATION, his chant SFX_5F_MUMBO_BUGABUGOW_MUFFLED and two
// SFX_1_MUMBO_UMENAKA as the spell goes off); then he says a line and a Golden Banana (drawn
// as a Jiggy by jiggy_collectibles.c) appears in front of him, with Banjo-Kazooie's Jiggy
// jingle. After that he gives a one-line remark.
//
// He talks as Bottles does: when the player comes close and stands still (or walks up, when
// what he has to say is new: his first ask, or the spell), not again until the player has gone
// this far off, and a remark he's given before at most once a minute.
//
// The Golden Banana is spawned as jiggy_conga.c spawns Conga's: an ACTOR_GOLDEN_BANANA from
// func_global_asm_806A5DF0 tied to DK64 permanent flag 0x31B (unused by the game, see
// jiggy_conga.c), which DK64 sets and saves when it's collected, spawned again on every visit
// until then. The trade (and his first ask) is kept with the DK64 save file (jiggy_save_get/set,
// key 0x4C000000 | map); the tokens spent come off the total jiggy_mumbo_tokens.c keeps.
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his
// text box in the HUD pass (jiggy_mumbo_hud_draw, patches_ui.c). Without a Banjo-Kazooie ROM
// nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

#define MUMBO_MODEL 0x3C6    // ASSET_3C6_MODEL_MUMBO
#define HUT_MODEL 0x3B3      // ASSET_3B3_MODEL_CCW_MUMBOS_HUT
#define MUMBO_PORTRAIT 0x7FC // ASSET_7FC_SPRITE_MUMBO
// Mumbo converts to about 56 KiB, with 43 bones; the hut to about 21 KiB, with no bones and 9
// commands.
#define MUMBO_BLOCK_MAX 0x10000
#define MUMBO_GFX_MAX 256
#define MUMBO_MTX_MAX 48
#define HUT_BLOCK_MAX 0x6000
#define HUT_GFX_MAX 32
#define HUT_MTX_MAX 4

// Banjo-Kazooie animations (chMumbo's D_80367490) and how long each plays, in seconds.
#define MUMBO_ANIM_IDLE 0x6C  // ASSET_6C_ANIM_MUMBO_IDLE
#define MUMBO_ANIM_SPELL 0x6D // ASSET_6D_ANIM_MUMBO_TRANSFORM
#define MUMBO_IDLE_SECONDS 6.67f
#define MUMBO_SPELL_SECONDS 7.5f

// Banjo-Kazooie sounds (sfx_e) and music (comusic_e), and how chMumbo plays them.
#define MUMBO_SFX_ERGHHH 0x41  // SFX_41_MUMBO_ERGHHH: as he wakes to the player, full volume
#define MUMBO_SFX_CHANT 0x5F   // SFX_5F_MUMBO_BUGABUGOW_MUFFLED: through the spell, 0.35 to 0.56
#define MUMBO_SFX_UMENAKA 0x01 // SFX_1_MUMBO_UMENAKA: twice at 0.57, as the spell goes off
#define MUMBO_SPELL_CHANT_AT 0.35f
#define MUMBO_SPELL_CHANT_END 0.56f
#define MUMBO_SPELL_ZAP_AT 0.57f
#define MUMBO_SPELL_MUSIC_AT 0.01f
#define JINGLE_MUMBO_SPELL 0x1D // COMUSIC_1D_MUMBO_TRANSFORMATION
// Mumbo's voice in the text box (gczoombox), one of these at 28000/32767, about pitch 1.1.
static const u16 mumbo_talk_sfx[] = {0x407, 0x408, 0x409, 0x40A, 0x40B};
#define MUMBO_TALK_SFX_EVERY 5 // Characters typed out between sounds.
// Banjo-Kazooie's Jiggy appearing (COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767).
#define JINGLE_JIGGY_SPAWN 0x3D

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo. The hut is drawn smaller
// than Banjo-Kazooie's (about 160 units tall and 125 across, against Mumbo's 45), so it sits in
// the Training Grounds' clearing rather than towering over the treehouse.
#define BK_TO_DK64 0.25f
#define HUT_SCALE 0.15f
// The hut's face (its mouth, the door) is along the model's +x.
#define HUT_FACE_YAW 1024.0f
// The player is kept this far (across) from the hut's middle, up to this high above its floor.
#define HUT_RADIUS 50.0f
#define HUT_HEIGHT 150.0f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// How fast he turns to face the player (a quarter of the way a frame, as Bottles does), and
// how near the player has to be for him to look at them.
#define MUMBO_TURN_RATE 0.25f
#define MUMBO_LOOK_RADIUS 250.0f
// He talks when the player comes this close, on the ground; not again until the player has
// walked this far off; and a remark he's given before at most this often (as Bottles).
#define MUMBO_REMIND_FRAMES (60 * 30) // A minute.
#define MUMBO_TALK_RADIUS 70.0f
#define MUMBO_REARM_RADIUS 250.0f
#define MUMBO_TALK_FEET_ABOVE 40.0f
#define MUMBO_TALK_FEET_BELOW 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing him and his hut on the floor under their spots: looked for from this far above the
// spot, and only trusted this close to the spot's height.
#define MUMBO_FLOOR_PROBE 40.0f
#define MUMBO_FLOOR_MAX_STEP 60.0f

// What his magic costs, in Mumbo Tokens; they're spent (see the top of this file).
#define MUMBO_TOKEN_COST 4
// jiggy_mumbo_tokens.c's save key for the token total.
#define TOKEN_SAVE_TOTAL_KEY 0x4DFFFFFF

// DK64 player states (Actor control_state) he talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

// The Golden Banana (as jiggy_conga.c spawns it).
#define REWARD_GOLDEN_BANANA 0x2D // ACTOR_GOLDEN_BANANA
#define REWARD_SPAWNER_TYPE (REWARD_GOLDEN_BANANA - 0x10)
#define FLAG_TYPE_PERMANENT 0
#define REWARD_HEIGHT 12.5f
#define REWARD_CLEARANCE 30.0f
#define REWARD_RESPAWN_FRAMES 90

// The text box (as jiggy_bottles.c draws it), in screen pixels. Mumbo's portrait is 64x64 in
// the ROM, as big as Bottles' 32x32 drawn twice the size, so it's drawn at its own size, in
// bands that each fit in TMEM.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SIZE 64
#define PORTRAIT_MAX_FRAMES 10
#define PORTRAIT_MAX_TEXELS (64 * 64)
#define PORTRAIT_BAND_TEXELS 2048 // 4 KiB of RGBA16.
// Frames 0 to 6 open his mouth; 7 on turn him away, which the box doesn't need.
#define PORTRAIT_TALK_FRAMES 7
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

// Where he is, map by map. The Training Grounds: on the open grass south of the treehouse
// (the player starts on its deck, at about (1346, 227, 1098)), past the notes, Jinjos,
// Honeycombs, feathers, tokens and the two pads, 150 or more units from each (the nearest, the
// note at (1372, 870)), and clear of the map's own plants and the pool to the east. He faces
// north, toward the treehouse and the collectibles; his hut is just behind him, facing the same
// way; his Golden Banana appears a little in front of him, 100 or more units from the rest.
typedef struct {
    Maps map;
    f32 x, y, z;            // Where he stands (y is the area's floor, if the floor isn't found).
    s16 yaw;                // Which way he and his hut face (12-bit angle units, 0 is +z).
    f32 hut_x, hut_z;       // The middle of his hut.
    f32 reward_x, reward_z; // Where his Golden Banana appears.
    s16 flag;               // Its DK64 permanent flag.
} MumboPlacement;
static const MumboPlacement mumbo_spots[] = {
    {MAP_TRAINING_GROUNDS, 1400.0f, 30.0f, 720.0f, 0, 1400.0f, 635.0f, 1430.0f, 770.0f, 0x31B},
};

// What he says. Each page is one press of A; lines are broken to fit the box when shown.
static char talk_count_line[TEXT_LINE_MAX]; // "YOU HAVE N."
static const char *const mumbo_ask[] = {
    "MUMBO JUMBO! BRING MUMBO FOUR SILVER SKULL TOKENS, AND MUMBO MAKE MAGIC!",
    talk_count_line,
};
static const char *const mumbo_spell[] = {
    "EEKUM BOKUM! MUMBO MAKE SHINY THING APPEAR!",
};
static const char *const mumbo_after_waiting[] = {
    "SHINY THING FOR BEAR AND MONKEY! TAKE BEFORE MUMBO CHANGE MIND!",
};
static const char *const mumbo_after[] = {
    "MUMBO MAGIC VERY STRONG! MUMBO GO BACK TO SLEEP NOW.",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): MUMBO_SAVE_* bits.
#define MUMBO_SAVE_MAP_KEY(map) (0x4C000000 | (u32)(map)) // 'L'
#define MUMBO_SAVE_ASKED 0x1  // He's asked for the tokens once.
#define MUMBO_SAVE_TRADED 0x2 // He's taken them and made his magic.

// Tells jiggy_mumbo_tokens.c the total has changed (and brings its counter up).
void jiggy_mumbo_tokens_refresh(void);

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
extern ActorSpawner *actor_spawner_pointer;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y and returns nonzero, or leaves it as it was.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
u8 isFlagSet(s16 flagIndex, u8 flagType);
void func_global_asm_806A5DF0(s16 actor, f32 x, f32 y, f32 z, s16 angle, u8 control_state, s16 flag, s32 arg7);

typedef enum {
    MUMBO_UNLOADED,
    MUMBO_READY,
    MUMBO_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or the portrait didn't convert.
} MumboLoadState;

typedef enum {
    MUMBO_IDLE,
    MUMBO_SPELL, // His spell dance (the tokens are already spent).
} MumboAction;

typedef enum {
    TALK_NONE = -1,
    TALK_ASK,           // Fewer than four tokens.
    TALK_SPELL,         // After the spell.
    TALK_AFTER_WAITING, // After, his Golden Banana not yet collected.
    TALK_AFTER,         // After, collected.
} MumboTalk;

static u8 mumbo_block[MUMBO_BLOCK_MAX] __attribute__((aligned(16)));
static u8 hut_block[HUT_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx mumbo_gfx[2][MUMBO_GFX_MAX];
static Mtx mumbo_mtx[2][MUMBO_MTX_MAX];
static Gfx hut_gfx[2][HUT_GFX_MAX];
static Mtx hut_mtx[2][HUT_MTX_MAX];
static JiggyBkPose mumbo_pose;
static JiggyBkPose hut_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static MumboLoadState mumbo_state = MUMBO_UNLOADED;

static s32 mumbo_last_map = -1;
static u32 mumbo_last_timer;
static const MumboPlacement *mumbo_spot; // This map's, or NULL.
static s32 mumbo_placed;
static f32 mumbo_pos[3];
static f32 hut_pos[3];
static f32 mumbo_yaw; // 12-bit angle units, as a float so the turn eases.
static MumboAction mumbo_action;
static f32 mumbo_progress; // 0 to 1 through the action's animation.
static u32 mumbo_saved;    // MUMBO_SAVE_* bits.
static s32 mumbo_armed;    // The player has been away since his last talk.
static u32 mumbo_last_talk; // object_timer when his last talk began.
static s32 mumbo_chant;     // The chant's handle, or 0.
static u32 mumbo_random_state = 0x4D554D42;

// His Golden Banana (as jiggy_conga.c keeps Conga's).
static f32 reward_spot[3];
static s32 reward_due;
static ActorSpawner *reward_spawner;
static s32 reward_wait;
static s32 reward_jingle;

// The dialogue (as jiggy_bottles.c keeps it).
static MumboTalk talk_kind = TALK_NONE;
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

static f32 mumbo_random(void) {
    mumbo_random_state = mumbo_random_state * 1664525 + 1013904223;
    return (mumbo_random_state >> 8) * (1.0f / 16777216.0f);
}

static void mumbo_load(void) {
    s32 size;
    s32 count;
    s32 i;

    mumbo_state = MUMBO_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(MUMBO_MODEL, mumbo_block, sizeof(mumbo_block));
    if (size <= 0 || size > (s32)sizeof(mumbo_block)) {
        return;
    }
    size = jiggy_bk_model_build(HUT_MODEL, hut_block, sizeof(hut_block));
    if (size <= 0 || size > (s32)sizeof(hut_block)) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(MUMBO_PORTRAIT);
    if (count <= 0) {
        return;
    }
    if (count > PORTRAIT_MAX_FRAMES) {
        count = PORTRAIT_MAX_FRAMES;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(MUMBO_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        if (got <= 0 || width == 0 || height == 0 || width * height > PORTRAIT_MAX_TEXELS ||
            width > PORTRAIT_BAND_TEXELS) {
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
    // rest hidden. Mumbo's awake pieces are 4 and 6 (chMumbo_draw, for his own hut; 5, 7 and
    // 8 are his summer and autumn things in Click Clock Wood).
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        mumbo_pose.selectors[i] = 0;
        hut_pose.selectors[i] = 0;
    }
    mumbo_pose.selectors[1] = 1;
    mumbo_pose.selectors[4] = 1;
    mumbo_pose.selectors[6] = 1;
    hut_pose.selectors[1] = 1;
    mumbo_state = MUMBO_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// The floor under (x, z), looked for from above `y`; `y` if there's none near.
static f32 mumbo_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + MUMBO_FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < MUMBO_FLOOR_MAX_STEP &&
        y - floor < MUMBO_FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

// Stands him and his hut over the floor at their spots, and finds where his Golden Banana goes.
static void mumbo_place(void) {
    mumbo_pos[0] = mumbo_spot->x;
    mumbo_pos[1] = mumbo_floor(mumbo_spot->x, mumbo_spot->y, mumbo_spot->z);
    mumbo_pos[2] = mumbo_spot->z;
    hut_pos[0] = mumbo_spot->hut_x;
    hut_pos[1] = mumbo_floor(mumbo_spot->hut_x, mumbo_spot->y, mumbo_spot->hut_z);
    hut_pos[2] = mumbo_spot->hut_z;
    reward_spot[0] = mumbo_spot->reward_x;
    reward_spot[1] = mumbo_floor(mumbo_spot->reward_x, mumbo_pos[1], mumbo_spot->reward_z);
    reward_spot[2] = mumbo_spot->reward_z;
    mumbo_yaw = mumbo_spot->yaw & 0xFFF;
    mumbo_action = MUMBO_IDLE;
    mumbo_progress = mumbo_random();
    reward_spawner = NULL;
    reward_wait = 0;
    reward_jingle = FALSE;
    // Paid before, his Golden Banana not yet collected: it's back in front of him.
    reward_due = (mumbo_saved & MUMBO_SAVE_TRADED) && !isFlagSet(mumbo_spot->flag, FLAG_TYPE_PERMANENT);
    mumbo_placed = TRUE;
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

// The player can't move while he talks or casts (as jiggy_bottles.c holds them).
static void mumbo_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void mumbo_stop_chant(void) {
    if (mumbo_chant > 0) {
        jiggy_bk_sound_stop(mumbo_chant);
        mumbo_chant = 0;
    }
}

// "YOU HAVE N." for the tokens the player has.
static void talk_set_count(s32 tokens) {
    static const char prefix[] = "YOU HAVE ";
    char digits[12];
    s32 digit_count = 0;
    s32 n = 0;
    s32 i;

    if (tokens < 0) {
        tokens = 0;
    }
    do {
        digits[digit_count++] = '0' + tokens % 10;
        tokens /= 10;
    } while (tokens > 0 && digit_count < (s32)sizeof(digits));
    for (i = 0; prefix[i] != '\0'; i++) {
        talk_count_line[n++] = prefix[i];
    }
    while (digit_count > 0) {
        talk_count_line[n++] = digits[--digit_count];
    }
    talk_count_line[n++] = '.';
    talk_count_line[n] = '\0';
}

static void talk_start(MumboTalk kind) {
    talk_kind = kind;
    switch (kind) {
        case TALK_ASK:
            talk_set_count(jiggy_save_get(current_file, TOKEN_SAVE_TOTAL_KEY));
            talk_pages = mumbo_ask;
            talk_page_count = ARRAY_COUNT_OF(mumbo_ask);
            break;
        case TALK_SPELL:
            talk_pages = mumbo_spell;
            talk_page_count = ARRAY_COUNT_OF(mumbo_spell);
            break;
        case TALK_AFTER_WAITING:
            talk_pages = mumbo_after_waiting;
            talk_page_count = ARRAY_COUNT_OF(mumbo_after_waiting);
            break;
        default:
            talk_pages = mumbo_after;
            talk_page_count = ARRAY_COUNT_OF(mumbo_after);
            break;
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    mumbo_lock_player(TRUE);
}

static void mumbo_save(u32 bits) {
    mumbo_saved |= bits;
    jiggy_save_set(current_file, MUMBO_SAVE_MAP_KEY(current_map), mumbo_saved);
}

// What he'd say to the player now.
static MumboTalk mumbo_next_talk(void) {
    if (!(mumbo_saved & MUMBO_SAVE_TRADED)) {
        return (s32)jiggy_save_get(current_file, TOKEN_SAVE_TOTAL_KEY) >= MUMBO_TOKEN_COST ? TALK_SPELL : TALK_ASK;
    }
    return isFlagSet(mumbo_spot->flag, FLAG_TYPE_PERMANENT) ? TALK_AFTER : TALK_AFTER_WAITING;
}

// Whether it's something the player hasn't heard: then walking up to him is enough, and it
// needn't wait out the minute.
static s32 mumbo_talk_is_new(MumboTalk kind) {
    return kind == TALK_SPELL || (kind == TALK_ASK && !(mumbo_saved & MUMBO_SAVE_ASKED));
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
        if (talk_shown / MUMBO_TALK_SFX_EVERY != before / MUMBO_TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 pick = (u32)(mumbo_random() * ARRAY_COUNT_OF(mumbo_talk_sfx)) % ARRAY_COUNT_OF(mumbo_talk_sfx);

            jiggy_bk_sfx_play(mumbo_talk_sfx[pick], 28000.0f / 32767.0f, 1.0f + mumbo_random() * 0.2f);
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
    mumbo_lock_player(FALSE);
    if (talk_kind == TALK_ASK && !(mumbo_saved & MUMBO_SAVE_ASKED)) {
        mumbo_save(MUMBO_SAVE_ASKED);
    } else if (talk_kind == TALK_SPELL) {
        // His Golden Banana appears, with Banjo-Kazooie's Jiggy jingle.
        reward_due = TRUE;
        reward_wait = 0;
        reward_jingle = TRUE;
    }
}

// Whether the player is close enough, and level enough with him, to talk.
static s32 mumbo_player_near(Actor *player, f32 radius) {
    f32 dx = mumbo_pos[0] - player->x_position;
    f32 dy = mumbo_pos[1] - player->y_position;
    f32 dz = mumbo_pos[2] - player->z_position;

    return dx * dx + dz * dz <= radius * radius && dy >= -MUMBO_TALK_FEET_ABOVE && dy <= MUMBO_TALK_FEET_BELOW;
}

// Takes the tokens and starts his spell (chMumbo's state 4 to 5 with enough tokens).
static void mumbo_cast(void) {
    s32 tokens = jiggy_save_get(current_file, TOKEN_SAVE_TOTAL_KEY);

    // Saved now, so the trade stands whatever happens before the Golden Banana's collected.
    jiggy_save_set(current_file, TOKEN_SAVE_TOTAL_KEY, tokens - MUMBO_TOKEN_COST);
    jiggy_mumbo_tokens_refresh();
    mumbo_save(MUMBO_SAVE_TRADED);
    mumbo_action = MUMBO_SPELL;
    mumbo_progress = 0.0f;
}

// Whether his Golden Banana spawned this visit is still in the game's spawner list (as
// jiggy_conga.c checks Conga's).
static s32 reward_spawner_alive(void) {
    ActorSpawner *spawner;

    for (spawner = actor_spawner_pointer; spawner != NULL; spawner = spawner->next_spawner) {
        if (spawner == reward_spawner) {
            return spawner->actor_type == REWARD_SPAWNER_TYPE;
        }
    }
    return FALSE;
}

// Spawns his Golden Banana while it's due and not collected (as jiggy_conga.c does).
static void reward_update(Actor *player, u32 frames) {
    f32 dx, dz;

    if (!reward_due) {
        return;
    }
    // Collected: DK64 set the flag when the player picked it up.
    if (isFlagSet(mumbo_spot->flag, FLAG_TYPE_PERMANENT)) {
        reward_due = FALSE;
        return;
    }
    if (reward_spawner != NULL) {
        if (reward_spawner_alive()) {
            return;
        }
        // Collected (the flag's coming), or gone with the map.
        reward_spawner = NULL;
        reward_wait = REWARD_RESPAWN_FRAMES;
    }
    if (reward_wait > 0) {
        reward_wait -= frames;
        return;
    }
    dx = player->x_position - reward_spot[0];
    dz = player->z_position - reward_spot[2];
    if (dx * dx + dz * dz < REWARD_CLEARANCE * REWARD_CLEARANCE) {
        return;
    }
    func_global_asm_806A5DF0(REWARD_GOLDEN_BANANA, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], 0, 0,
                             mumbo_spot->flag, 0);
    // func_global_asm_80689250 puts the new spawner at the head of the list.
    reward_spawner = actor_spawner_pointer;
    if (reward_jingle) {
        jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        reward_jingle = FALSE;
    }
}

// Turns a share of the way toward `target` (12-bit angle units), over `frames` frames.
static void mumbo_turn_to(s16 target, u32 frames) {
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = (target & 0xFFF) - mumbo_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        mumbo_yaw += turn * MUMBO_TURN_RATE;
        if (mumbo_yaw < 0.0f) {
            mumbo_yaw += 4096.0f;
        } else if (mumbo_yaw >= 4096.0f) {
            mumbo_yaw -= 4096.0f;
        }
    }
}

// Keeps the player out of the hut, which has no collision of its own: back to its edge.
static void hut_push_player(Actor *player) {
    f32 dx = player->x_position - hut_pos[0];
    f32 dz = player->z_position - hut_pos[2];
    f32 dy = player->y_position - hut_pos[1];
    f32 distance;

    if (dy < -MUMBO_TALK_FEET_BELOW || dy > HUT_HEIGHT || dx * dx + dz * dz >= HUT_RADIUS * HUT_RADIUS) {
        return;
    }
    distance = _sqrtf(dx * dx + dz * dz);
    if (distance < 0.01f) {
        // Right in the middle: out through the door.
        dx = func_global_asm_80612794(mumbo_spot->yaw);
        dz = func_global_asm_80612790(mumbo_spot->yaw);
        distance = 1.0f;
    }
    player->x_position = hut_pos[0] + dx / distance * HUT_RADIUS;
    player->z_position = hut_pos[2] + dz / distance * HUT_RADIUS;
}

// A frame of his behavior (chMumbo_update), `frames` game frames on.
static void mumbo_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = mumbo_progress;
    s16 to_player = func_global_asm_80665DE0(player->x_position, player->z_position, mumbo_pos[0], mumbo_pos[2]);
    MumboTalk next;
    s32 is_new;

    if (frames == 0) {
        return;
    }
    switch (mumbo_action) {
        case MUMBO_IDLE:
            // He looks at the player while they're about, and back out from his hut after.
            if (talk_open || distance_xz(mumbo_pos[0], mumbo_pos[2], player->x_position, player->z_position) <= MUMBO_LOOK_RADIUS) {
                mumbo_turn_to(to_player, frames);
            } else {
                mumbo_turn_to(mumbo_spot->yaw, frames);
            }
            mumbo_progress += seconds / MUMBO_IDLE_SECONDS;
            while (mumbo_progress >= 1.0f) {
                mumbo_progress -= 1.0f;
            }
            if (talk_open) {
                break;
            }
            next = mumbo_next_talk();
            is_new = mumbo_talk_is_new(next);
            if (!mumbo_armed) {
                mumbo_armed = !mumbo_player_near(player, MUMBO_REARM_RADIUS) &&
                              (is_new || object_timer - mumbo_last_talk >= MUMBO_REMIND_FRAMES);
                break;
            }
            if (mumbo_player_near(player, MUMBO_TALK_RADIUS) &&
                player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT &&
                (player->control_state == STATE_IDLE || (is_new && player->control_state == STATE_WALK))) {
                s16 to_mumbo = func_global_asm_80665DE0(mumbo_pos[0], mumbo_pos[2], player->x_position, player->z_position);

                mumbo_armed = FALSE;
                mumbo_last_talk = object_timer;
                // The player turns to him (func_8028F94C(2, this->position)).
                player->y_rotation = to_mumbo & 0xFFF;
                player->unkEE = to_mumbo & 0xFFF;
                jiggy_bk_sfx_play(MUMBO_SFX_ERGHHH, 1.0f, 1.0f);
                if (next == TALK_SPELL) {
                    mumbo_lock_player(TRUE);
                    mumbo_cast();
                    mumbo_yaw = to_player & 0xFFF;
                } else {
                    talk_start(next);
                }
            }
            break;
        case MUMBO_SPELL:
            mumbo_lock_player(TRUE);
            mumbo_progress += seconds / MUMBO_SPELL_SECONDS;
            if (before < MUMBO_SPELL_MUSIC_AT && mumbo_progress >= MUMBO_SPELL_MUSIC_AT) {
                jiggy_bk_jingle_play(JINGLE_MUMBO_SPELL, 1.0f);
            }
            if (before < MUMBO_SPELL_CHANT_AT && mumbo_progress >= MUMBO_SPELL_CHANT_AT) {
                mumbo_chant = jiggy_bk_sfx_play(MUMBO_SFX_CHANT, 1.0f, 1.0f);
            }
            if (before < MUMBO_SPELL_CHANT_END && mumbo_progress >= MUMBO_SPELL_CHANT_END) {
                mumbo_stop_chant();
            }
            if (before < MUMBO_SPELL_ZAP_AT && mumbo_progress >= MUMBO_SPELL_ZAP_AT) {
                jiggy_bk_sfx_play(MUMBO_SFX_UMENAKA, 1.0f, 1.0f);
                jiggy_bk_sfx_play(MUMBO_SFX_UMENAKA, 1.0f, 1.0f);
            }
            if (mumbo_progress >= 1.0f) {
                mumbo_stop_chant();
                mumbo_action = MUMBO_IDLE;
                mumbo_progress = 0.0f;
                talk_start(TALK_SPELL);
            }
            break;
    }
    reward_update(player, frames);
    if (talk_open) {
        talk_update();
    }
}

// Draws a built model at a spot, scaled and turned to `yaw` (12-bit angle units).
static Gfx *mumbo_pose_draw(Gfx *dl, JiggyBkPose *pose, s32 model, void *block, s32 animation, f32 progress,
                            const f32 pos[3], f32 scale_by, f32 yaw, Gfx *gfx, u32 gfx_capacity, Mtx *mtx,
                            u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 scale[4][4];

    guScaleF(scale, scale_by, scale_by, scale_by);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, pose->world);
    pose->world[3][0] = pos[0];
    pose->world[3][1] = pos[1];
    pose->world[3][2] = pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    pose->model = model;
    pose->block = block;
    pose->animation = animation;
    pose->progress = progress;
    pose->gfx = gfx;
    pose->gfx_capacity = gfx_capacity;
    pose->mtx = mtx;
    pose->mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Mumbo and his hut once a frame, with the player.
Gfx *jiggy_mumbo_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): he's back where he was.
    if ((s32)current_map != mumbo_last_map || object_timer < mumbo_last_timer) {
        // Leaving the map mid-talk or mid-spell lets the player go (the trade's already saved).
        if (talk_open || mumbo_action == MUMBO_SPELL) {
            mumbo_lock_player(FALSE);
            mumbo_stop_chant();
            talk_open = FALSE;
        }
        mumbo_last_map = current_map;
        mumbo_placed = FALSE;
        mumbo_spot = NULL;
        mumbo_action = MUMBO_IDLE;
        for (i = 0; i < ARRAY_COUNT_OF(mumbo_spots); i++) {
            if (mumbo_spots[i].map == current_map) {
                mumbo_spot = &mumbo_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        mumbo_saved = mumbo_spot != NULL ? jiggy_save_get(current_file, MUMBO_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (mumbo_spot == NULL) {
        mumbo_last_timer = object_timer;
        return dl;
    }
    if (mumbo_state == MUMBO_UNLOADED) {
        mumbo_load();
    }
    if (mumbo_state != MUMBO_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!mumbo_placed) {
        mumbo_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        mumbo_place();
        mumbo_armed = !mumbo_player_near(actor, MUMBO_REARM_RADIUS);
    }
    frames = object_timer - mumbo_last_timer;
    mumbo_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && mumbo_action != MUMBO_SPELL) {
        frames = 0;
    }
    hut_push_player(actor);
    mumbo_update(actor, frames);

    dl = mumbo_pose_draw(dl, &hut_pose, HUT_MODEL, hut_block, -1, 0.0f, hut_pos, HUT_SCALE,
                         mumbo_spot->yaw - HUT_FACE_YAW, hut_gfx[buffer], HUT_GFX_MAX, hut_mtx[buffer], HUT_MTX_MAX);
    dl = mumbo_pose_draw(dl, &mumbo_pose, MUMBO_MODEL, mumbo_block,
                         mumbo_action == MUMBO_SPELL ? MUMBO_ANIM_SPELL : MUMBO_ANIM_IDLE, mumbo_progress, mumbo_pos,
                         BK_TO_DK64, mumbo_yaw, mumbo_gfx[buffer], MUMBO_GFX_MAX, mumbo_mtx[buffer], MUMBO_MTX_MAX);
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
Gfx *jiggy_mumbo_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 band;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (!talk_open || mumbo_state != MUMBO_READY || cc_number_of_players != 1) {
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

    // His portrait, its mouth moving while the text types out: in bands of rows, each one
    // TMEM load.
    frame = 0;
    if (talk_shown < talk_length) {
        frame = (talk_ticks / PORTRAIT_TICKS_PER_FRAME) % (2 * portrait_frames - 2 > 0 ? 2 * portrait_frames - 2 : 1);
        if (frame >= portrait_frames) {
            frame = 2 * portrait_frames - 2 - frame;
        }
    }
    width = portrait_width[frame];
    height = portrait_height[frame];
    band = PORTRAIT_BAND_TEXELS / width;
    gDPPipeSync(dl++);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    {
        s32 px = box_x + BOX_BORDER + BOX_PAD + (PORTRAIT_SIZE - width) / 2;
        s32 py = box_y + (BOX_HEIGHT - height) / 2;
        s32 row;

        for (row = 0; row < height; row += band) {
            s32 rows = height - row < band ? height - row : band;

            gDPLoadTextureTile(dl++, portrait_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
                width, height,
                0, row, width - 1, row + rows - 1,
                0,
                G_TX_CLAMP, G_TX_CLAMP,
                G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(dl++,
                px << 2, (py + row) << 2, (px + width) << 2, (py + row + rows) << 2,
                G_TX_RENDERTILE,
                0, row << 5, 1 << 10, 1 << 10);
        }
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
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"MUMBO", 1);
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
