// Eyrie: Click Clock Wood's eaglet, read from the player's Banjo-Kazooie ROM, as an NPC on Fungi
// Forest's owl tree ring path, where the two worlds have collided; with his caterpillar quest.
//
// As in Click Clock Wood's summer (chEyrieBaby, src/CCW/code_3310.c), Eyrie
// (ASSET_484_MODEL_EAGLE_BABY) waits hungry, flapping and squawking (his waiting animation and
// its sounds), for caterpillars. Click Clock Wood ties him to its seasons; here he's tied to
// Fungi Forest's day and night (DK64's PERMFLAG_PROGRESS_IS_NIGHTTIME): by day he's awake, and
// three caterpillars (ASSET_485_MODEL_CATERPILLAR, crawling about with their crawl animation and
// squeak, as chcaterpillar does) are out on the ring path round the owl tree; by night the
// caterpillars hide and Eyrie sleeps (his sleeping animation, eyes shut), snoring.
//
// Touching a caterpillar takes it (Banjo-Kazooie's pop, SFX_C5), shown in the HUD with his
// count until it's fed to him. Coming up to him by day with caterpillars: he talks in a text box,
// eats each (his eating animation and lip smack, and Banjo-Kazooie's ding, COMUSIC_2B, as each
// caterpillar reaching him plays it); the third has him yawn and grow (his yawn, with
// Banjo-Kazooie's puzzle solved fanfare) to the size he is in Click Clock Wood's autumn, and he
// gives a Jiggy (a Golden Banana drawn as a Jiggy by jiggy_collectibles.c: reward 0x50 of
// jiggy_rewards.c) that appears beside him with Banjo-Kazooie's Jiggy jingle. The player is held
// in the game's cutscene lock from his talk until he's done eating (and growing).
//
// Coming up to him otherwise, standing or walking (only standing, for a repeat), he talks: asks
// for caterpillars, mumbles in his sleep at night, or (after) thanks the player. Repeats follow
// Bottles' reminder rule (jiggy_bottles.c): only standing, after being 250 units off, at most once
// a minute.
//
// The caterpillars taken, how many he's eaten, his first talk and the quest done are kept with
// the DK64 save file (jiggy_save_get/set, key 0x54000000 | map: Fungi Forest's characters, Eyrie
// index 0 in bits 16-23).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his text
// box and the caterpillar count in the HUD pass (jiggy_eyrie_hud_draw, patches_ui.c). Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define EYRIE_MODEL 0x484       // ASSET_484_MODEL_EAGLE_BABY
#define CATERPILLAR_MODEL 0x485 // ASSET_485_MODEL_CATERPILLAR
#define EYRIE_PORTRAIT 0x83F    // ASSET_83F_SPRITE_EEYRIE_YOUNG
#define CATERPILLAR_SPRITE 0x809 // ASSET_809_SPRITE_CATERPILLAR: its talking head, the HUD icon
// Eyrie converts to 15680 bytes (605 vertices, 36 bones, 8 pose commands); a caterpillar to
// 5000 (10 bones, 24 pose commands).
#define EYRIE_BLOCK_MAX 0x4000
#define EYRIE_GFX_MAX 32
#define EYRIE_MTX_MAX 40
#define CATERPILLAR_BLOCK_MAX 0x1400
#define CATERPILLAR_GFX_MAX 48
#define CATERPILLAR_MTX_MAX 12

// Banjo-Kazooie animations (chEyrieBabyProgression's summer set) and how long each plays, in
// seconds; the autumn waiting animation once he's grown.
#define EYRIE_ANIM_WAIT 0x191
#define EYRIE_ANIM_YAWN 0x192
#define EYRIE_ANIM_SLEEP 0x194
#define EYRIE_ANIM_EAT 0x21A
#define EYRIE_ANIM_WAIT_GROWN 0x195
#define EYRIE_WAIT_SECONDS 6.0f
#define EYRIE_YAWN_SECONDS 8.0f
#define EYRIE_SLEEP_SECONDS 6.0f
#define EYRIE_EAT_SECONDS 2.0f
#define CATERPILLAR_ANIM_CRAWL 0x18D // ASSET_18D_ANIM_CATERPILLAR_UNK: its crawl (chcaterpillar's pull out)
#define CATERPILLAR_CRAWL_SECONDS 2.0f

// Banjo-Kazooie sounds (sfx_e) and how chEyrieBaby and chcaterpillar play them.
#define SFX_CLUCKER_AHH 0x4A      // Waiting, at 0.11, 0.22 and 0.34: 25000/32767, 1.1
#define SFX_CLAW_SWIPE 0x02       // Waiting (his wing flaps), at 0.55 to 0.87: 25000/32767, 1.0-1.1
#define SFX_EYRIE_LIPSMACK 0x106  // Eating, at 0.95: 25000/32767, 1.0
#define SFX_EYRIE_YAWN 0x105      // Yawning: 25000/32767, 0.9-1.1
#define SFX_EYRIE_MAMA 0x18C      // "Mama!"
#define SFX_SNORE_IN 0x5D         // Sleeping, at 0.2: 25000/32767, 1.0
#define SFX_SNORE_OUT 0x5E        // Sleeping, at 0.72: 25000/32767, 1.0
#define SFX_CATERPILLAR_SQUEAK 0xAD // Crawling, at 0.1: 15000/32767, 0.78-0.81
#define SFX_TWINKLY_POP 0xC5      // A caterpillar taken (as an acorn is): 25000/32767, 1.0
// His voice in the text box (gczoombox's young Eyrie): SFX_4A at 20000/32767, pitch 1.5.
#define EYRIE_TALK_SFX SFX_CLUCKER_AHH
#define EYRIE_TALK_SFX_EVERY 5
// Banjo-Kazooie's jingles: each caterpillar eaten (COMUSIC_2B_DING_B, 28000/32767), the puzzle
// solved fanfare (COMUSIC_2D, 0x7FFF/32767) and the Jiggy appearing (COMUSIC_3D_JIGGY_SPAWN).
#define JINGLE_DING 0x2B
#define JINGLE_PUZZLE_SOLVED 0x2D
#define JINGLE_JIGGY_SPAWN 0x3D
// Sounds from Eyrie fade out between these distances (Banjo-Kazooie's 500 and 2500, scaled).
#define SFX_NEAR 125.0f
#define SFX_FAR 625.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo; Eyrie is drawn smaller
// (Click Clock Wood's nest is far bigger than DK64's Kongs), 85 units tall hungry and 125 once
// he's grown; a caterpillar is about 32 units long.
#define BK_TO_DK64 0.25f
#define EYRIE_SCALE 0.15f
#define EYRIE_GROWN_SCALE 0.22f
#define CATERPILLAR_SCALE 0.2f
#define GAME_FPS 30.0f
#define EYRIE_TURN_RATE 0.1f

// Talking: the player this close to him and on the ground; a repeat needs the player to have
// been this far off since, and comes at most this often (jiggy_bottles.c's reminder rule).
#define EYRIE_TALK_RADIUS 80.0f
#define EYRIE_REARM_RADIUS 250.0f
#define EYRIE_REPEAT_FRAMES (60 * 30)
#define EYRIE_TALK_FEET 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// The player is kept this far from him (DK64 doesn't know he's there).
#define EYRIE_RADIUS 30.0f
#define PLAYER_RADIUS 10.0f
// Placing on the floor: looked for from this far above a spot, trusted this close to it.
#define FLOOR_PROBE 30.0f
#define FLOOR_MAX_STEP 50.0f

// The caterpillars: three to feed him; each crawls about its spot (within this far, at this
// speed in units a second), and is taken within this distance and height.
#define CATERPILLAR_COUNT 3
#define CATERPILLAR_WANDER 35.0f
#define CATERPILLAR_SPEED 9.0f
#define CATERPILLAR_TAKE_RADIUS 22.0f
#define CATERPILLAR_TAKE_HEIGHT 40.0f
#define CATERPILLAR_STEP 12.0f // The most the floor may rise or fall in a step.

// DK64's PERMFLAG_PROGRESS_IS_NIGHTTIME: Fungi Forest's clock set to night.
#define FLAG_NIGHTTIME 0xCE
#define FLAG_TYPE_PERMANENT 0
// DK64 player states (Actor control_state) he talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

// His Jiggy: jiggy_rewards.c reward 0x50 (Fungi Forest's range is 0x50-0x5F), this high above
// the ground.
#define EYRIE_REWARD_ID 0x50
#define REWARD_HEIGHT 6.0f

// The text box (as jiggy_bottles.c draws it), in screen pixels.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 8
#define PORTRAIT_MAX_TEXELS (32 * 32)
// Frames 0 to 7 move his beak (gczoombox's 8 talking frames for the young Eyrie).
#define PORTRAIT_TALK_FRAMES 8
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

// The caterpillars carried, in the HUD: the caterpillar's head and the count, in the
// right-hand column (where Chimpy's orange goes in Japes, jiggy_chimpy.c).
#define HUD_ICON_SIZE 32
#define HUD_RIGHT_MARGIN 56
#define HUD_Y (96 + 3 * 36)
#define HUD_FADE_STEP 0x20

// Where he is. Fungi Forest: on the wide north side of the owl tree's ring path (DK64's floor
// at 83), between two of the notes (73 and 101 units off), and the caterpillars on the ring path
// three quarters of the way between DK64's Tiny bananas (clear of them, the notes and the little
// mushrooms). His Jiggy lands on the path west of him.
typedef struct {
    Maps map;
    f32 x, y, z;
    f32 reward_x, reward_z;
    f32 caterpillars[CATERPILLAR_COUNT][3];
} EyriePlacement;
static const EyriePlacement eyrie_spots[] = {
    {MAP_FUNGI, 4450.0f, 83.0f, 3245.0f, 4400.0f, 3250.0f,
     {{4100.0f, 83.0f, 3324.0f}, {4878.0f, 83.0f, 3658.0f}, {4240.0f, 83.0f, 4135.0f}}},
};

// What he says. Each page is one press of A.
static const char *const eyrie_ask[] = {
    "CHEEP! I'M EYRIE, AND I'M SOOO HUNGRY!",
    "CATERPILLARS CRAWL ROUND THIS PATH IN THE DAYTIME.",
    "BRING ME THREE AND I'LL GROW BIG AND STRONG!",
};
static const char *const eyrie_sleep[] = {
    "ZZZ... MAMA... ZZZ...",
    "CATERPILLARS... ONLY COME OUT... WHEN THE SUN'S UP... ZZZ...",
};
static const char *const eyrie_deliver[] = {
    "CATERPILLARS! FOR ME? GULP, GULP!",
};
// After eating, by how many he still wants.
static const char *const eyrie_more_two[] = {
    "MMM! TWO MORE, PLEASE!",
};
static const char *const eyrie_more_one[] = {
    "MMM! JUST ONE MORE AND I'LL BE ALL GROWN UP!",
};
static const char *const eyrie_grown[] = {
    "LOOK AT ME, I'M HUGE! SOON I'LL FLY HIGHER THAN THAT CLOCK!",
    "HERE, THIS SHINY THING WAS IN MY NEST. IT'S YOURS!",
};
static const char *const eyrie_thanks[] = {
    "CHEEP! THANKS FOR THE GRUB, YOU TWO!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): EYRIE_SAVE_* bits, the caterpillars taken (one bit each from
// EYRIE_SAVE_TAKEN_SHIFT) and how many he's eaten (EYRIE_SAVE_FED_SHIFT, 4 bits).
#define EYRIE_SAVE_MAP_KEY(map) (0x54000000 | (0u << 16) | (u32)(map))
#define EYRIE_SAVE_MET 0x1
#define EYRIE_SAVE_DONE 0x2
#define EYRIE_SAVE_TAKEN_SHIFT 4
#define EYRIE_SAVE_FED_SHIFT 8
#define EYRIE_SAVE_FED_MASK (0xFu << EYRIE_SAVE_FED_SHIFT)

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
u8 isFlagSet(s16 flagIndex, u8 flagType);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    EYRIE_UNLOADED,
    EYRIE_READY,
    EYRIE_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or the portrait didn't convert.
} EyrieLoadState;

typedef enum {
    EYRIE_WAITING,  // Awake, hungry (or grown).
    EYRIE_SLEEPING, // At night.
    EYRIE_EATING,   // A caterpillar, after his talk.
    EYRIE_GROWING,  // His yawn, growing, after the third.
} EyrieAction;

typedef enum {
    TALK_ASK,
    TALK_SLEEP,
    TALK_DELIVER,
    TALK_MORE,
    TALK_GROWN,
    TALK_THANKS,
} EyrieTalk;

typedef struct {
    f32 home[3];
    f32 pos[3];
    f32 target[2];
    f32 yaw;
    f32 progress;
} Caterpillar;

static u8 eyrie_block[EYRIE_BLOCK_MAX] __attribute__((aligned(16)));
static u8 caterpillar_block[CATERPILLAR_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx eyrie_gfx[2][EYRIE_GFX_MAX];
static Mtx eyrie_mtx[2][EYRIE_MTX_MAX];
static Gfx caterpillar_gfx[2][CATERPILLAR_COUNT][CATERPILLAR_GFX_MAX];
static Mtx caterpillar_mtx[2][CATERPILLAR_COUNT][CATERPILLAR_MTX_MAX];
static JiggyBkPose eyrie_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static u16 hud_icon_texels[PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s32 hud_icon_width; // 0 when the sprite didn't convert: the count shows alone.
static s32 hud_icon_height;
static s32 hud_alpha;
static EyrieLoadState eyrie_state = EYRIE_UNLOADED;

static s32 eyrie_last_map = -1;
static u32 eyrie_last_timer;
static const EyriePlacement *eyrie_spot;
static s32 eyrie_placed;
static f32 eyrie_pos[3];
static f32 eyrie_yaw;
static f32 eyrie_scale;
static EyrieAction eyrie_action;
static f32 eyrie_progress;
static s32 eyrie_holding; // The player's held while he eats and grows.
static s32 eyrie_armed;
static u32 eyrie_last_talk;
static u32 eyrie_saved;
static s32 eyrie_night;
static s32 eyrie_to_eat; // Caterpillars left to eat in this feeding.
static Caterpillar caterpillars[CATERPILLAR_COUNT];
static f32 reward_spot[3];
static s32 reward_jingle;
static u32 eyrie_random_state = 0x45595249;

static EyrieTalk talk_kind;
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

static f32 eyrie_random(void) {
    eyrie_random_state = eyrie_random_state * 1664525 + 1013904223;
    return (eyrie_random_state >> 8) * (1.0f / 16777216.0f);
}

static s32 eyrie_build(u32 model, u8 *block, u32 capacity) {
    s32 size = jiggy_bk_model_build(model, block, capacity);

    return size > 0 && size <= (s32)capacity;
}

static void eyrie_load(void) {
    s32 count;
    s32 got;
    s32 i;

    eyrie_state = EYRIE_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    if (!eyrie_build(EYRIE_MODEL, eyrie_block, sizeof(eyrie_block)) ||
        !eyrie_build(CATERPILLAR_MODEL, caterpillar_block, sizeof(caterpillar_block))) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(EYRIE_PORTRAIT);
    if (count <= 0) {
        return;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 width, height;

        got = jiggy_bk_sprite_frame_rgba16(EYRIE_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
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
    hud_icon_width = 0;
    got = jiggy_bk_sprite_frame_rgba16(CATERPILLAR_SPRITE, 0, hud_icon_texels, sizeof(hud_icon_texels));
    if (got > 0) {
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        if (width > 0 && height > 0 && width * height <= PORTRAIT_MAX_TEXELS) {
            hud_icon_width = width;
            hud_icon_height = height;
        }
    }
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        eyrie_pose.selectors[i] = 0;
    }
    eyrie_pose.selectors[1] = 1;
    eyrie_state = EYRIE_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// The floor under (x, z), looked for from FLOOR_PROBE over `y`; `y` if there's none near.
static f32 eyrie_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < FLOOR_MAX_STEP && y - floor < FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

// A Banjo-Kazooie sound from (x, z), fading out with distance from the player.
static void eyrie_sfx(f32 x, f32 z, Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(x, z, player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static s32 eyrie_done(void) {
    return (eyrie_saved & EYRIE_SAVE_DONE) != 0;
}

static s32 caterpillar_taken(s32 i) {
    return (eyrie_saved >> (EYRIE_SAVE_TAKEN_SHIFT + i)) & 1;
}

static s32 eyrie_fed(void) {
    return (eyrie_saved & EYRIE_SAVE_FED_MASK) >> EYRIE_SAVE_FED_SHIFT;
}

static s32 eyrie_carried(void) {
    s32 taken = 0;
    s32 i;

    for (i = 0; i < CATERPILLAR_COUNT; i++) {
        taken += caterpillar_taken(i);
    }
    return taken > eyrie_fed() ? taken - eyrie_fed() : 0;
}

static void eyrie_save(void) {
    jiggy_save_set(current_file, EYRIE_SAVE_MAP_KEY(current_map), eyrie_saved);
}

static void eyrie_start(EyrieAction action, f32 progress) {
    eyrie_action = action;
    eyrie_progress = progress;
}

static void eyrie_place(Actor *player) {
    s32 i;

    eyrie_pos[0] = eyrie_spot->x;
    eyrie_pos[1] = eyrie_floor(eyrie_spot->x, eyrie_spot->y, eyrie_spot->z);
    eyrie_pos[2] = eyrie_spot->z;
    reward_spot[0] = eyrie_spot->reward_x;
    reward_spot[1] = eyrie_floor(eyrie_spot->reward_x, eyrie_spot->y, eyrie_spot->reward_z) + REWARD_HEIGHT;
    reward_spot[2] = eyrie_spot->reward_z;
    reward_jingle = FALSE;
    eyrie_holding = FALSE;
    eyrie_scale = eyrie_done() ? EYRIE_GROWN_SCALE : EYRIE_SCALE;
    eyrie_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, eyrie_pos[0], eyrie_pos[2]) & 0xFFF;
    eyrie_start(eyrie_night ? EYRIE_SLEEPING : EYRIE_WAITING, eyrie_random());
    for (i = 0; i < CATERPILLAR_COUNT; i++) {
        Caterpillar *c = &caterpillars[i];

        c->home[0] = eyrie_spot->caterpillars[i][0];
        c->home[1] = eyrie_floor(c->home[0], eyrie_spot->caterpillars[i][1], eyrie_spot->caterpillars[i][2]);
        c->home[2] = eyrie_spot->caterpillars[i][2];
        c->pos[0] = c->home[0];
        c->pos[1] = c->home[1];
        c->pos[2] = c->home[2];
        c->target[0] = c->home[0];
        c->target[1] = c->home[2];
        c->yaw = eyrie_random() * 4096.0f;
        c->progress = eyrie_random();
    }
    eyrie_placed = TRUE;
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

static void eyrie_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void talk_start(EyrieTalk kind) {
    talk_kind = kind;
    switch (kind) {
        case TALK_ASK:
            talk_pages = eyrie_ask;
            talk_page_count = ARRAY_COUNT_OF(eyrie_ask);
            break;
        case TALK_SLEEP:
            talk_pages = eyrie_sleep;
            talk_page_count = ARRAY_COUNT_OF(eyrie_sleep);
            break;
        case TALK_DELIVER:
            talk_pages = eyrie_deliver;
            talk_page_count = ARRAY_COUNT_OF(eyrie_deliver);
            break;
        case TALK_MORE:
            if (CATERPILLAR_COUNT - eyrie_fed() >= 2) {
                talk_pages = eyrie_more_two;
                talk_page_count = ARRAY_COUNT_OF(eyrie_more_two);
            } else {
                talk_pages = eyrie_more_one;
                talk_page_count = ARRAY_COUNT_OF(eyrie_more_one);
            }
            break;
        case TALK_GROWN:
            talk_pages = eyrie_grown;
            talk_page_count = ARRAY_COUNT_OF(eyrie_grown);
            break;
        default:
            talk_pages = eyrie_thanks;
            talk_page_count = ARRAY_COUNT_OF(eyrie_thanks);
            break;
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    talk_a_held = TRUE;
    eyrie_lock_player(TRUE);
}

// The text box's frame: A to type out the rest of the page or go on to the next.
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
        if (talk_shown / EYRIE_TALK_SFX_EVERY != before / EYRIE_TALK_SFX_EVERY && talk_shown < talk_length) {
            // Asleep, he mumbles lower.
            jiggy_bk_sfx_play(EYRIE_TALK_SFX, 20000.0f / 32767.0f,
                              (talk_kind == TALK_SLEEP ? 1.0f : 1.5f) + eyrie_random() * 0.1f);
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
        // He eats what he's brought; the player's held until he's done.
        eyrie_holding = TRUE;
        eyrie_start(EYRIE_EATING, 0.0f);
        return;
    }
    if (talk_kind == TALK_GROWN) {
        // His Jiggy appears beside him.
        reward_jingle = TRUE;
    }
    eyrie_holding = FALSE;
    eyrie_lock_player(FALSE);
}

static s32 eyrie_player_near(Actor *player, f32 radius) {
    f32 dy = eyrie_pos[1] - player->y_position;

    return distance_xz(eyrie_pos[0], eyrie_pos[2], player->x_position, player->z_position) <= radius &&
           dy >= -EYRIE_TALK_FEET && dy <= EYRIE_TALK_FEET;
}

// Starts a talk when the player stops by him (see the top of this file).
static void eyrie_check_talk(Actor *player) {
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;
    s32 standing = player->control_state == STATE_IDLE;
    s32 walking = player->control_state == STATE_WALK;
    s16 to_eyrie;

    if (!eyrie_armed) {
        eyrie_armed = !eyrie_player_near(player, EYRIE_REARM_RADIUS) &&
                      (!(eyrie_saved & EYRIE_SAVE_MET) || object_timer - eyrie_last_talk >= EYRIE_REPEAT_FRAMES);
    }
    if (!eyrie_player_near(player, EYRIE_TALK_RADIUS) || !grounded) {
        return;
    }
    if (!eyrie_night && !eyrie_done() && eyrie_carried() > 0 && (standing || walking)) {
        // Caterpillars: he eats them, whatever he said last.
        eyrie_saved |= EYRIE_SAVE_MET;
        eyrie_save();
        eyrie_to_eat = eyrie_carried();
        talk_start(TALK_DELIVER);
    } else if (eyrie_armed && (standing || (walking && !(eyrie_saved & EYRIE_SAVE_MET)))) {
        eyrie_saved |= EYRIE_SAVE_MET;
        eyrie_save();
        talk_start(eyrie_done() ? TALK_THANKS : eyrie_night ? TALK_SLEEP : TALK_ASK);
    } else {
        return;
    }
    eyrie_armed = FALSE;
    eyrie_last_talk = object_timer;
    to_eyrie = func_global_asm_80665DE0(eyrie_pos[0], eyrie_pos[2], player->x_position, player->z_position);
    player->y_rotation = to_eyrie & 0xFFF;
    player->unkEE = to_eyrie & 0xFFF;
}

static void eyrie_turn_to(f32 *yaw, const f32 pos[3], f32 x, f32 z, f32 rate, u32 frames) {
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

// True when `at` is passed going from `from` to `to` through an animation.
static s32 passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

// The caterpillars, out by day until taken: each crawls to a spot near its home, then another.
static void caterpillars_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    s32 i;

    if (eyrie_night || eyrie_done()) {
        return;
    }
    for (i = 0; i < CATERPILLAR_COUNT; i++) {
        Caterpillar *c = &caterpillars[i];
        f32 before = c->progress;
        f32 distance;
        f32 dy;

        if (caterpillar_taken(i)) {
            continue;
        }
        dy = player->y_position - c->pos[1];
        if (dy > -CATERPILLAR_TAKE_HEIGHT && dy < CATERPILLAR_TAKE_HEIGHT &&
            distance_xz(c->pos[0], c->pos[2], player->x_position, player->z_position) <= CATERPILLAR_TAKE_RADIUS) {
            eyrie_saved |= 1u << (EYRIE_SAVE_TAKEN_SHIFT + i);
            eyrie_save();
            jiggy_bk_sfx_play(SFX_TWINKLY_POP, 25000.0f / 32767.0f, 1.0f);
            jiggy_bk_sfx_play(SFX_CATERPILLAR_SQUEAK, 15000.0f / 32767.0f, 1.0f);
            continue;
        }
        c->progress += seconds / CATERPILLAR_CRAWL_SECONDS;
        if (passed(before, c->progress, 0.1f)) {
            eyrie_sfx(c->pos[0], c->pos[2], player, SFX_CATERPILLAR_SQUEAK, 15000.0f / 32767.0f * 0.6f,
                      0.78f + eyrie_random() * 0.03f);
        }
        while (c->progress >= 1.0f) {
            c->progress -= 1.0f;
        }
        distance = distance_xz(c->target[0], c->target[1], c->pos[0], c->pos[2]);
        if (distance < 2.0f) {
            f32 angle = eyrie_random() * 4096.0f;
            f32 reach = eyrie_random() * CATERPILLAR_WANDER;

            c->target[0] = c->home[0] + reach * func_global_asm_80612794((s16)angle);
            c->target[1] = c->home[2] + reach * func_global_asm_80612790((s16)angle);
            continue;
        }
        eyrie_turn_to(&c->yaw, c->pos, c->target[0], c->target[1], 0.08f, frames);
        {
            f32 step = CATERPILLAR_SPEED * seconds;
            f32 nx, nz;
            f32 probe;
            f32 floor;

            if (step > distance) {
                step = distance;
            }
            nx = c->pos[0] + (c->target[0] - c->pos[0]) * step / distance;
            nz = c->pos[2] + (c->target[1] - c->pos[2]) * step / distance;
            probe = c->pos[1] + CATERPILLAR_STEP;
            floor = probe;
            func_global_asm_80667110(nx, nz, &floor);
            if (floor == probe || floor < c->pos[1] - CATERPILLAR_STEP) {
                // A wall or a drop: somewhere else.
                c->target[0] = c->home[0];
                c->target[1] = c->home[2];
                continue;
            }
            c->pos[0] = nx;
            c->pos[1] = floor;
            c->pos[2] = nz;
        }
    }
}

// Until DK64 knows he's there, the player is kept out of him.
static void eyrie_push_player(Actor *player) {
    f32 dx = player->x_position - eyrie_pos[0];
    f32 dz = player->z_position - eyrie_pos[2];
    f32 reach = EYRIE_RADIUS * eyrie_scale / EYRIE_SCALE * 0.75f + PLAYER_RADIUS;
    f32 distance = _sqrtf(dx * dx + dz * dz);

    if (distance >= reach || distance < 0.01f || player->y_position > eyrie_pos[1] + EYRIE_TALK_FEET) {
        return;
    }
    player->x_position = eyrie_pos[0] + dx * reach / distance;
    player->z_position = eyrie_pos[2] + dz * reach / distance;
}

// A frame of his behavior (chEyrieBaby_update), `frames` game frames on.
static void eyrie_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = eyrie_progress;

    if (frames == 0) {
        return;
    }
    caterpillars_update(player, frames);
    eyrie_push_player(player);
    if (!talk_open && !eyrie_holding) {
        eyrie_check_talk(player);
    }
    switch (eyrie_action) {
        case EYRIE_WAITING:
            if (eyrie_night && !talk_open) {
                eyrie_start(EYRIE_SLEEPING, 0.0f);
                break;
            }
            eyrie_turn_to(&eyrie_yaw, eyrie_pos, player->x_position, player->z_position, EYRIE_TURN_RATE, frames);
            eyrie_progress += seconds / EYRIE_WAIT_SECONDS;
            if (!eyrie_done()) {
                // His squawks and flaps (chEyrieBabyWaitingSFX).
                if (passed(before, eyrie_progress, 0.11f) || passed(before, eyrie_progress, 0.22f) ||
                    passed(before, eyrie_progress, 0.34f)) {
                    eyrie_sfx(eyrie_pos[0], eyrie_pos[2], player, SFX_CLUCKER_AHH, 25000.0f / 32767.0f,
                              1.05f + eyrie_random() * 0.1f);
                }
                if (passed(before, eyrie_progress, 0.55f) || passed(before, eyrie_progress, 0.77f)) {
                    eyrie_sfx(eyrie_pos[0], eyrie_pos[2], player, SFX_CLAW_SWIPE, 25000.0f / 32767.0f, 1.1f);
                }
            }
            while (eyrie_progress >= 1.0f) {
                eyrie_progress -= 1.0f;
            }
            break;
        case EYRIE_SLEEPING:
            if (!eyrie_night && !talk_open) {
                eyrie_start(EYRIE_WAITING, 0.0f);
                break;
            }
            eyrie_progress += seconds / EYRIE_SLEEP_SECONDS;
            if (passed(before, eyrie_progress, 0.2f)) {
                eyrie_sfx(eyrie_pos[0], eyrie_pos[2], player, SFX_SNORE_IN, 25000.0f / 32767.0f * 0.5f, 1.3f);
            }
            if (passed(before, eyrie_progress, 0.72f)) {
                eyrie_sfx(eyrie_pos[0], eyrie_pos[2], player, SFX_SNORE_OUT, 25000.0f / 32767.0f * 0.5f, 1.3f);
            }
            while (eyrie_progress >= 1.0f) {
                eyrie_progress -= 1.0f;
            }
            break;
        case EYRIE_EATING:
            eyrie_lock_player(TRUE);
            eyrie_turn_to(&eyrie_yaw, eyrie_pos, player->x_position, player->z_position, EYRIE_TURN_RATE, frames);
            eyrie_progress += seconds / EYRIE_EAT_SECONDS;
            if (passed(before, eyrie_progress, 0.95f)) {
                eyrie_sfx(eyrie_pos[0], eyrie_pos[2], player, SFX_EYRIE_LIPSMACK, 25000.0f / 32767.0f, 1.0f);
            }
            if (eyrie_progress < 1.0f) {
                break;
            }
            // One down.
            {
                s32 fed = eyrie_fed() + 1;

                eyrie_saved = (eyrie_saved & ~EYRIE_SAVE_FED_MASK) | ((u32)fed << EYRIE_SAVE_FED_SHIFT);
                eyrie_to_eat--;
                jiggy_bk_jingle_play(JINGLE_DING, 28000.0f / 32767.0f);
                if (fed >= CATERPILLAR_COUNT) {
                    eyrie_saved |= EYRIE_SAVE_DONE;
                    eyrie_save();
                    jiggy_bk_jingle_play(JINGLE_PUZZLE_SOLVED, 1.0f);
                    jiggy_bk_sfx_play(SFX_EYRIE_YAWN, 25000.0f / 32767.0f, 1.0f);
                    eyrie_start(EYRIE_GROWING, 0.0f);
                    break;
                }
                eyrie_save();
            }
            if (eyrie_to_eat > 0) {
                eyrie_start(EYRIE_EATING, 0.0f);
            } else {
                eyrie_start(EYRIE_WAITING, 0.0f);
                talk_start(TALK_MORE);
            }
            break;
        case EYRIE_GROWING:
            eyrie_lock_player(TRUE);
            eyrie_progress += seconds / EYRIE_YAWN_SECONDS;
            if (passed(before, eyrie_progress, 0.3f) || passed(before, eyrie_progress, 0.6f)) {
                eyrie_sfx(eyrie_pos[0], eyrie_pos[2], player, SFX_EYRIE_YAWN, 25000.0f / 32767.0f, 0.95f);
            }
            if (passed(before, eyrie_progress, 0.85f)) {
                eyrie_sfx(eyrie_pos[0], eyrie_pos[2], player, SFX_EYRIE_MAMA, 25000.0f / 32767.0f, 1.0f);
            }
            // He grows through the yawn.
            {
                f32 t = eyrie_progress < 1.0f ? eyrie_progress : 1.0f;

                eyrie_scale = EYRIE_SCALE + (EYRIE_GROWN_SCALE - EYRIE_SCALE) * t;
            }
            if (eyrie_progress >= 1.0f) {
                eyrie_scale = EYRIE_GROWN_SCALE;
                eyrie_start(EYRIE_WAITING, 0.0f);
                talk_start(TALK_GROWN);
            }
            break;
    }
    if (eyrie_done() && !eyrie_holding && !talk_open && eyrie_action != EYRIE_GROWING) {
        if (!jiggy_reward_update(EYRIE_REWARD_ID, reward_spot[0], reward_spot[1], reward_spot[2], player, frames) &&
            reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        }
        reward_jingle = FALSE;
    }
    if (talk_open) {
        talk_update();
    }
}

// Draws a built model at a spot, turned to `yaw` (12-bit angle units), at `scale`.
static Gfx *eyrie_pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, const f32 pos[3], f32 yaw,
                            f32 scale, Gfx *gfx, u32 gfx_capacity, Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 scaling[4][4];

    guScaleF(scaling, scale, scale, scale);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scaling, rotate, eyrie_pose.world);
    eyrie_pose.world[3][0] = pos[0];
    eyrie_pose.world[3][1] = pos[1];
    eyrie_pose.world[3][2] = pos[2];
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    eyrie_pose.model = model;
    eyrie_pose.block = block;
    eyrie_pose.animation = animation;
    eyrie_pose.progress = progress;
    eyrie_pose.gfx = gfx;
    eyrie_pose.gfx_capacity = gfx_capacity;
    eyrie_pose.mtx = mtx;
    eyrie_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&eyrie_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// Eyrie and the caterpillars once a frame, with the player.
Gfx *jiggy_eyrie_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    s32 animation;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != eyrie_last_map || object_timer < eyrie_last_timer) {
        if (talk_open || eyrie_holding) {
            eyrie_lock_player(FALSE);
            talk_open = FALSE;
            eyrie_holding = FALSE;
        }
        eyrie_last_map = current_map;
        eyrie_placed = FALSE;
        eyrie_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(eyrie_spots); i++) {
            if (eyrie_spots[i].map == current_map) {
                eyrie_spot = &eyrie_spots[i];
            }
        }
        eyrie_saved = eyrie_spot != NULL ? jiggy_save_get(current_file, EYRIE_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (eyrie_spot == NULL) {
        eyrie_last_timer = object_timer;
        return dl;
    }
    if (eyrie_state == EYRIE_UNLOADED) {
        eyrie_load();
    }
    if (eyrie_state != EYRIE_READY) {
        return dl;
    }
    eyrie_night = isFlagSet(FLAG_NIGHTTIME, FLAG_TYPE_PERMANENT) != 0;
    if (!eyrie_placed) {
        eyrie_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        eyrie_place(actor);
        eyrie_armed = !eyrie_player_near(actor, EYRIE_REARM_RADIUS);
        eyrie_last_talk = object_timer - EYRIE_REPEAT_FRAMES;
    }
    frames = object_timer - eyrie_last_timer;
    eyrie_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && !eyrie_holding) {
        frames = 0;
    }
    eyrie_update(actor, frames);

    switch (eyrie_action) {
        case EYRIE_SLEEPING:
            animation = EYRIE_ANIM_SLEEP;
            break;
        case EYRIE_EATING:
            animation = EYRIE_ANIM_EAT;
            break;
        case EYRIE_GROWING:
            animation = EYRIE_ANIM_YAWN;
            break;
        default:
            animation = eyrie_done() ? EYRIE_ANIM_WAIT_GROWN : EYRIE_ANIM_WAIT;
            break;
    }
    // His eyes: shut asleep (chEyrieBaby_draw's selector 3).
    eyrie_pose.selectors[3] = eyrie_action == EYRIE_SLEEPING ? 2 : 1;
    dl = eyrie_pose_draw(dl, EYRIE_MODEL, eyrie_block, animation, eyrie_progress, eyrie_pos, eyrie_yaw, eyrie_scale,
                         eyrie_gfx[buffer], EYRIE_GFX_MAX, eyrie_mtx[buffer], EYRIE_MTX_MAX);
    eyrie_pose.selectors[3] = 0;
    if (!eyrie_night && !eyrie_done()) {
        for (i = 0; i < CATERPILLAR_COUNT; i++) {
            Caterpillar *c = &caterpillars[i];

            if (caterpillar_taken(i)) {
                continue;
            }
            dl = eyrie_pose_draw(dl, CATERPILLAR_MODEL, caterpillar_block, CATERPILLAR_ANIM_CRAWL, c->progress, c->pos,
                                 c->yaw, CATERPILLAR_SCALE, caterpillar_gfx[buffer][i], CATERPILLAR_GFX_MAX,
                                 caterpillar_mtx[buffer][i], CATERPILLAR_MTX_MAX);
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

// The caterpillars carried, in the HUD: the caterpillar's head and "X n", fading in and out.
static Gfx *hud_count_draw(Gfx *dl) {
    s32 carried = eyrie_spot != NULL && !eyrie_done() ? eyrie_carried() : 0;
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
    if (hud_icon_width > 0) {
        dl = alignHUDGameplay(dl, ALIGN_RIGHT);
        gDPPipeSync(dl++);
        gDPSetCycleType(dl++, G_CYC_1CYCLE);
        gDPSetTexturePersp(dl++, G_TP_NONE);
        gDPSetTextureLUT(dl++, G_TT_NONE);
        gDPSetTextureFilter(dl++, G_TF_POINT);
        gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, hud_alpha);
        gDPLoadTextureTile(dl++, hud_icon_texels, G_IM_FMT_RGBA, G_IM_SIZ_16b, hud_icon_width, hud_icon_height, 0, 0,
                           hud_icon_width - 1, hud_icon_height - 1, 0, G_TX_CLAMP, G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(dl++, x << 2, HUD_Y << 2, (x + HUD_ICON_SIZE) << 2, (HUD_Y + HUD_ICON_SIZE) << 2,
                            G_TX_RENDERTILE, 0, 0, (hud_icon_width << 10) / HUD_ICON_SIZE,
                            (hud_icon_height << 10) / HUD_ICON_SIZE);
        gDPPipeSync(dl++);
        dl = popHUD(dl, ALIGN_RIGHT);
    }
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

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the
// caterpillars carried, and his text box while he talks (drawn as jiggy_bottles_hud_draw draws
// Bottles').
Gfx *jiggy_eyrie_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (eyrie_state != EYRIE_READY || cc_number_of_players != 1) {
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

    // His portrait, its beak moving while the text types out (still, asleep).
    frame = 0;
    if (talk_shown < talk_length && talk_kind != TALK_SLEEP) {
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
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"EYRIE", 1);
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
