// Chimpy: Mumbo's Mountain's little monkey, read from the player's Banjo-Kazooie ROM, as an NPC
// in Jungle Japes' first area, where the two worlds have collided; with his orange quest.
//
// Like chlmonkey (src/MM/ch/lmonkey.c), Chimpy (ASSET_35D_MODEL_CHIMPY) idles with his idle
// animation (holding out his hand), turning to face the player, now and then hopping in place
// (his jump animation) and chattering (SFX_58/59_CHIMPY_NOISE, as __chlmonkey_playRandomNoise
// plays them, quieter further off). He stands on his tree stump (ASSET_3C8_MODEL_CHIMPY_STUMP),
// sunk most of the way into the ground.
//
// Conga (jiggy_conga.c) hogs the oranges: once he's beaten (his save bits, read here), an orange
// (ASSET_2D2_MODEL_ORANGE, as chOrangeCollectible draws it) waits on the ground beside him.
// Touching it takes it, with Banjo-Kazooie's orange pickup sound (SFX_B3_ORANGE_TALKING, as
// __chLevelCollectible_collide plays it), and Banjo-Kazooie's 2D orange (sprite 0x648) shows in
// the HUD until it's delivered. DK64 has no way to carry it in the player's hands, so it's kept as
// an item, as Banjo-Kazooie's own item counter (ITEM_19_ORANGE) does.
//
// When the player comes near, standing or walking (only standing, for a repeat: see below), he
// talks in a text box. Without the orange, he asks for one; with it, he thanks them,
// Banjo-Kazooie's puzzle solved fanfare plays (COMUSIC_2D, as the orange reaching him plays it),
// he hops off his stump to the ground beside it, and the stump rises out of the ground as
// chchimpystump does (2.5 units a frame, scaled, with its rumble: lookup 0x6A, SFX 0x3EC at
// pitch 0.2) with a Golden Banana (drawn as a Jiggy by jiggy_collectibles.c) appearing on top,
// with Banjo-Kazooie's Jiggy jingle. The player is held in the game's cutscene lock from his
// thanks until the Golden Banana appears.
//
// The Golden Banana is spawned as jiggy_conga.c spawns his: an ACTOR_GOLDEN_BANANA from
// func_global_asm_806A5DF0, tied to DK64 permanent flag 0x31C (next to Conga's 0x31D, past the
// last one the game uses), spawned again on every visit until it's collected.
//
// His repeats (asking again, before the orange, and his thanks, after it) follow Bottles'
// reminder rule (jiggy_bottles.c): only when the player stands by him (not walking past), only
// after the player has been 250 units off since, and at most once a minute.
//
// The orange taken, the orange delivered and his first talk are kept with the DK64 save file
// (jiggy_save_get/set, key 0x41000000 | map).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his
// text box and the orange in the HUD pass (jiggy_chimpy_hud_draw, patches_ui.c). Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define CHIMPY_MODEL 0x35D    // ASSET_35D_MODEL_CHIMPY
#define STUMP_MODEL 0x3C8     // ASSET_3C8_MODEL_CHIMPY_STUMP
#define ORANGE_MODEL 0x2D2    // ASSET_2D2_MODEL_ORANGE
#define CHIMPY_PORTRAIT 0x7F5 // ASSET_7F5_SPRITE_CHIMPY
#define ORANGE_SPRITE 0x648   // Unnamed in asset_e ("Orange (2D)"): a 50x50 orange.
// Chimpy converts to about 20 KiB (596 vertices, 5 small textures); the stump to about 9 KiB
// (18 vertices, two 64x64 textures); the orange to about 1.6 KiB.
#define CHIMPY_BLOCK_MAX 0x8000
#define CHIMPY_GFX_MAX 160
#define CHIMPY_MTX_MAX 40
#define STUMP_BLOCK_MAX 0x4000
#define STUMP_GFX_MAX 32
#define STUMP_MTX_MAX 4
#define ORANGE_BLOCK_MAX 0x1000
#define ORANGE_GFX_MAX 16
#define ORANGE_MTX_MAX 4

// Banjo-Kazooie animations (chlmonkeyAnimations) and how long each plays, in seconds.
#define CHIMPY_ANIM_JUMP 0x5B // ASSET_5B_ANIM_CHIMPY_JUMP
#define CHIMPY_ANIM_IDLE 0x5C // ASSET_5C_ANIM_CHIMPY_IDLE: holding out his hand
#define CHIMPY_IDLE_SECONDS 2.3f
#define CHIMPY_JUMP_SECONDS 0.67f
// The chance he hops in place at the end of an idle loop.
#define CHIMPY_HOP_CHANCE 0.15f

// Banjo-Kazooie sounds (sfx_e) and how chlmonkey and its stump play them.
#define CHIMPY_SFX_NOISE_1 0x58 // SFX_58_CHIMPY_NOISE_1: chatter, up to 18000/32767, 0.85-1.1
#define CHIMPY_SFX_NOISE_2 0x59 // SFX_59_CHIMPY_NOISE_2: the same
#define CHIMPY_NOISE_CHANCE 0.2f
#define CHIMPY_NOISE_COOLDOWN 6  // Frames between chatters, at least.
#define CHIMPY_NOISE_RADIUS 175.0f // He chatters with the player this close (700 units, scaled).
#define ORANGE_SFX_PICKUP 0xB3  // SFX_B3_ORANGE_TALKING: the orange taken, 0x7FF8/32767, 1.0
#define STUMP_SFX_RUMBLE 0x3EC  // Lookup 0x6A: the stump rising, 0x7FF8/32767, pitch 0.2
// Chimpy's voice in the text box (gczoombox), one of these at 20000/32767, about pitch 1.3.
static const u16 chimpy_talk_sfx[] = {0x58, 0x59, 0x5A};
#define CHIMPY_TALK_SFX_EVERY 5 // Characters typed out between sounds.
// Banjo-Kazooie's puzzle solved fanfare (COMUSIC_2D, 0x7FFF/32767) and Jiggy appearing
// (COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767).
#define JINGLE_PUZZLE_SOLVED 0x2D
#define JINGLE_JIGGY_SPAWN 0x3D

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// How fast he turns to face the player (as Bottles turns).
#define CHIMPY_TURN_RATE 0.25f
// He talks when the player comes this close to his stump (Banjo-Kazooie's 345 units, scaled), on
// the ground; a repeat needs the player to have been this far off since, and comes at most this
// often (jiggy_bottles.c's reminder rule).
#define CHIMPY_TALK_RADIUS 86.0f
#define CHIMPY_REARM_RADIUS 250.0f
#define CHIMPY_REPEAT_FRAMES (60 * 30) // A minute.
#define CHIMPY_TALK_FEET_ABOVE 40.0f
#define CHIMPY_TALK_FEET_BELOW 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing him on the floor under his spot: looked for from this far above the spot, and only
// trusted this close to the spot's height.
#define CHIMPY_FLOOR_PROBE 40.0f
#define CHIMPY_FLOOR_MAX_STEP 60.0f
// His hop off the stump: this high an arc on top of the animation's own.
#define CHIMPY_HOP_ARC 10.0f

// The stump (127 units tall, about 165 round, scaled): sunk this far into the ground until it
// rises (chchimpystump sinks it all the way, 134 units; here a stub shows for him to stand on), at
// chchimpystump's 2.5 units a frame. Until it rises the player can't walk into it.
#define STUMP_HEIGHT (127.0f * BK_TO_DK64)
#define STUMP_SUNK 24.0f
#define STUMP_RISE_SPEED (2.5f * BK_TO_DK64)
#define STUMP_RADIUS (165.0f * BK_TO_DK64)
#define PLAYER_RADIUS 10.0f

// The orange on the ground: it turns slowly, and is taken within this distance and height.
#define ORANGE_SPIN_PER_FRAME 40.0f // 12-bit angle units.
#define ORANGE_TAKE_RADIUS 22.0f
#define ORANGE_TAKE_HEIGHT 40.0f

// DK64 player states (Actor control_state) he talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

// The Golden Banana (see the top of this file), as jiggy_conga.c spawns his.
#define REWARD_GOLDEN_BANANA 0x2D // ACTOR_GOLDEN_BANANA
#define REWARD_SPAWNER_TYPE (REWARD_GOLDEN_BANANA - 0x10)
#define FLAG_TYPE_PERMANENT 0
#define REWARD_HEIGHT 6.0f // Above the raised stump's top.
#define REWARD_CLEARANCE 30.0f
#define REWARD_RESPAWN_FRAMES 90

// The text box (as jiggy_bottles.c draws it), in screen pixels.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 10
#define PORTRAIT_MAX_TEXELS (32 * 32)
// Frames 0 to 6 move his mouth (gczoombox's 7 talking frames for Chimpy).
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

// The orange in the HUD while it's carried: the 50x50 sprite shown this tall, in the right-hand
// column below the feather and Mumbo Token counters (jiggy_feathers.c, jiggy_mumbo_tokens.c).
// It's loaded in two halves, each fitting in TMEM.
#define HUD_ORANGE_MAX_TEXELS (52 * 52)
#define HUD_ORANGE_SIZE 32
#define HUD_RIGHT_MARGIN 56
#define HUD_Y (96 + 3 * 36)
#define HUD_FADE_STEP 0x20

// Where he is, map by map. Jungle Japes' first area: on open grass east of the middle, between
// Bottles' molehill (1120, 560; 149 units off) and Conga (1430, 420; 207 off), clear of the notes'
// trail (the nearest note 205 off), the feathers (181 and 183 off), the Mumbo Tokens, Jinjos and
// Honeycombs, south of the Grublins' middle path, and short of the out of bounds north-east
// corner. He hops off to the ground a little south-west of his stump; the orange waits on the
// ground beside Conga, toward the area and clear of his Golden Banana (1395, 425).
typedef struct {
    Maps map;
    f32 x, y, z;              // The stump (y is the area's floor, if the floor isn't found).
    f32 ground_x, ground_z;   // Where he stands once he's hopped off it.
    f32 orange_x, orange_z;   // Where the orange waits once Conga's beaten.
    s16 flag;                 // The Golden Banana's DK64 permanent flag.
} ChimpyPlacement;
static const ChimpyPlacement chimpy_spots[] = {
    {MAP_JAPES, 1269.0f, 280.0f, 550.0f, 1224.0f, 505.0f, 1385.0f, 480.0f, 0x31C},
};

// What he says. Each page is one press of A; lines are broken to fit the box when shown.
static const char *const chimpy_ask[] = {
    "OOH! I'M CHIMPY. CONGA HOGS ALL THE ORANGES.",
    "BRING ME ONE AND I'LL SHOW YOU SOMETHING SHINY!",
};
static const char *const chimpy_deliver[] = {
    "AN ORANGE! THANKS, BANJO AND KONG!",
};
static const char *const chimpy_thanks[] = {
    "YUM! THAT ORANGE WAS THE BEST, YOU TWO!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save keys (jiggy_save_get): CHIMPY_SAVE_* bits, and Conga's (jiggy_conga.c's
// CONGA_SAVE_DEFEATED).
#define CHIMPY_SAVE_MAP_KEY(map) (0x41000000 | (u32)(map)) // 'A'
#define CHIMPY_SAVE_ORANGE_TAKEN 0x1
#define CHIMPY_SAVE_DELIVERED 0x2
#define CHIMPY_SAVE_MET 0x4
#define CONGA_SAVE_MAP_KEY(map) (0x43000000 | (u32)(map))
#define CONGA_SAVE_DEFEATED 0x2
// How often Conga's save bits are read back while he's not yet beaten.
#define CONGA_CHECK_FRAMES 15

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
u8 isFlagSet(s16 flagIndex, u8 flagType);
void func_global_asm_806A5DF0(s16 actor, f32 x, f32 y, f32 z, s16 angle, u8 control_state, s16 flag, s32 arg7);

typedef enum {
    CHIMPY_UNLOADED,
    CHIMPY_READY,
    CHIMPY_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or the portrait didn't convert.
} ChimpyLoadState;

typedef enum {
    CHIMPY_IDLE,       // On his stump (or, after, beside it), holding out his hand.
    CHIMPY_HOP,        // A hop in place.
    CHIMPY_HOP_OFF,    // Off the stump, after his thanks.
    CHIMPY_STUMP_RISE, // Watching the stump rise.
} ChimpyAction;

typedef enum {
    TALK_ASK,
    TALK_DELIVER,
    TALK_THANKS,
} ChimpyTalk;

static u8 chimpy_block[CHIMPY_BLOCK_MAX] __attribute__((aligned(16)));
static u8 stump_block[STUMP_BLOCK_MAX] __attribute__((aligned(16)));
static u8 orange_block[ORANGE_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx chimpy_gfx[2][CHIMPY_GFX_MAX];
static Mtx chimpy_mtx[2][CHIMPY_MTX_MAX];
static Gfx stump_gfx[2][STUMP_GFX_MAX];
static Mtx stump_mtx[2][STUMP_MTX_MAX];
static Gfx orange_gfx[2][ORANGE_GFX_MAX];
static Mtx orange_mtx[2][ORANGE_MTX_MAX];
static JiggyBkPose chimpy_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static u16 hud_orange_texels[HUD_ORANGE_MAX_TEXELS] __attribute__((aligned(8)));
static s32 hud_orange_width; // 0 when the sprite didn't convert: no HUD icon.
static s32 hud_orange_height;
static s32 hud_orange_alpha;
static ChimpyLoadState chimpy_state = CHIMPY_UNLOADED;

static s32 chimpy_last_map = -1;
static u32 chimpy_last_timer;
static const ChimpyPlacement *chimpy_spot; // This map's, or NULL.
static s32 chimpy_placed;
static f32 chimpy_pos[3];
static f32 chimpy_yaw; // 12-bit angle units, as a float so the turn eases.
static ChimpyAction chimpy_action;
static f32 chimpy_progress; // 0 to 1 through the action's animation.
static f32 chimpy_floor_y;  // The floor under his stump.
static f32 chimpy_ground[3]; // Where he stands once he's hopped off.
static f32 stump_rise;      // How far the stump is out of the ground: 0 sunk, STUMP_SUNK raised.
static s32 stump_rumble;    // The rumble's handle, or 0.
static s32 chimpy_holding;  // The player's held from his thanks until the Golden Banana appears.
static s32 chimpy_armed;    // The player has been away since his last talk.
static u32 chimpy_last_talk; // object_timer when his last talk began.
static s32 chimpy_noise_cooldown;
static u32 chimpy_saved; // CHIMPY_SAVE_* bits.
static s32 conga_beaten;
static u32 conga_checked; // object_timer when Conga's bits were last read.
static f32 orange_pos[3];
static f32 orange_yaw;
static s32 orange_out; // On the ground, to be taken.
static u32 chimpy_random_state = 0x4348494D;

// His Golden Banana: where it appears, whether it's due, its spawner this visit (NULL until
// spawned), frames until it's spawned again, and its jingle.
static f32 reward_spot[3];
static s32 reward_due;
static ActorSpawner *reward_spawner;
static s32 reward_wait;
static s32 reward_jingle;

// The dialogue (as jiggy_bottles.c keeps it).
static ChimpyTalk talk_kind;
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

static f32 chimpy_random(void) {
    chimpy_random_state = chimpy_random_state * 1664525 + 1013904223;
    return (chimpy_random_state >> 8) * (1.0f / 16777216.0f);
}

static s32 chimpy_build(u32 model, u8 *block, u32 capacity) {
    s32 size = jiggy_bk_model_build(model, block, capacity);

    return size > 0 && size <= (s32)capacity;
}

static void chimpy_load(void) {
    s32 count;
    s32 got;
    s32 i;

    chimpy_state = CHIMPY_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    if (!chimpy_build(CHIMPY_MODEL, chimpy_block, sizeof(chimpy_block)) ||
        !chimpy_build(STUMP_MODEL, stump_block, sizeof(stump_block)) ||
        !chimpy_build(ORANGE_MODEL, orange_block, sizeof(orange_block))) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(CHIMPY_PORTRAIT);
    if (count <= 0) {
        return;
    }
    if (count > PORTRAIT_MAX_FRAMES) {
        count = PORTRAIT_MAX_FRAMES;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 width, height;

        got = jiggy_bk_sprite_frame_rgba16(CHIMPY_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
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
    // The orange for the HUD; without it there's no icon, and the rest still works.
    hud_orange_width = 0;
    got = jiggy_bk_sprite_frame_rgba16(ORANGE_SPRITE, 0, hud_orange_texels, sizeof(hud_orange_texels));
    if (got > 0) {
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        if (width > 0 && height >= 2 && width * height <= HUD_ORANGE_MAX_TEXELS) {
            hud_orange_width = width;
            hud_orange_height = height;
        }
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        chimpy_pose.selectors[i] = 0;
    }
    chimpy_pose.selectors[1] = 1;
    chimpy_state = CHIMPY_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

static void chimpy_start(ChimpyAction action, f32 progress) {
    chimpy_action = action;
    chimpy_progress = progress;
}

// The floor under (x, z), looked for from `probe_above` over `y`; `y` if there's none near.
static f32 chimpy_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + CHIMPY_FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < CHIMPY_FLOOR_MAX_STEP &&
        y - floor < CHIMPY_FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

// The top of the stump, as far as it's risen.
static f32 stump_top(void) {
    return chimpy_floor_y + STUMP_HEIGHT - STUMP_SUNK + stump_rise;
}

static s32 chimpy_delivered(void) {
    return (chimpy_saved & CHIMPY_SAVE_DELIVERED) != 0;
}

static s32 chimpy_carrying(void) {
    return (chimpy_saved & CHIMPY_SAVE_ORANGE_TAKEN) && !chimpy_delivered();
}

// Places the stump, him (on it, or beside it once he's had his orange) and the orange.
static void chimpy_place(Actor *player) {
    chimpy_floor_y = chimpy_floor(chimpy_spot->x, chimpy_spot->y, chimpy_spot->z);
    chimpy_ground[0] = chimpy_spot->ground_x;
    chimpy_ground[1] = chimpy_floor(chimpy_spot->ground_x, chimpy_floor_y, chimpy_spot->ground_z);
    chimpy_ground[2] = chimpy_spot->ground_z;
    orange_pos[0] = chimpy_spot->orange_x;
    orange_pos[1] = chimpy_floor(chimpy_spot->orange_x, chimpy_floor_y, chimpy_spot->orange_z);
    orange_pos[2] = chimpy_spot->orange_z;
    orange_out = FALSE;
    reward_spawner = NULL;
    reward_wait = 0;
    reward_jingle = FALSE;
    chimpy_holding = FALSE;
    chimpy_noise_cooldown = 0;
    if (chimpy_delivered()) {
        // Had his orange before: the stump's up, he's beside it, and the Golden Banana's back
        // on top until it's collected.
        stump_rise = STUMP_SUNK;
        chimpy_pos[0] = chimpy_ground[0];
        chimpy_pos[1] = chimpy_ground[1];
        chimpy_pos[2] = chimpy_ground[2];
        reward_due = !isFlagSet(chimpy_spot->flag, FLAG_TYPE_PERMANENT);
    } else {
        stump_rise = 0.0f;
        chimpy_pos[0] = chimpy_spot->x;
        chimpy_pos[1] = stump_top();
        chimpy_pos[2] = chimpy_spot->z;
        reward_due = FALSE;
    }
    reward_spot[0] = chimpy_spot->x;
    reward_spot[1] = chimpy_floor_y + STUMP_HEIGHT;
    reward_spot[2] = chimpy_spot->z;
    chimpy_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, chimpy_pos[0], chimpy_pos[2]) & 0xFFF;
    chimpy_start(CHIMPY_IDLE, chimpy_random());
    chimpy_placed = TRUE;
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
static void chimpy_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void chimpy_save(u32 bits) {
    chimpy_saved |= bits;
    jiggy_save_set(current_file, CHIMPY_SAVE_MAP_KEY(current_map), chimpy_saved);
}

static void talk_start(ChimpyTalk kind) {
    talk_kind = kind;
    if (kind == TALK_ASK) {
        talk_pages = chimpy_ask;
        talk_page_count = ARRAY_COUNT_OF(chimpy_ask);
    } else if (kind == TALK_DELIVER) {
        talk_pages = chimpy_deliver;
        talk_page_count = ARRAY_COUNT_OF(chimpy_deliver);
    } else {
        talk_pages = chimpy_thanks;
        talk_page_count = ARRAY_COUNT_OF(chimpy_thanks);
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    chimpy_lock_player(TRUE);
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
        if (talk_shown / CHIMPY_TALK_SFX_EVERY != before / CHIMPY_TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 pick = (u32)(chimpy_random() * ARRAY_COUNT_OF(chimpy_talk_sfx)) % ARRAY_COUNT_OF(chimpy_talk_sfx);

            jiggy_bk_sfx_play(chimpy_talk_sfx[pick], 20000.0f / 32767.0f, 1.2f + chimpy_random() * 0.2f);
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
        // He hops off his stump to let it rise; the player's held until the Golden Banana's out.
        chimpy_holding = TRUE;
        chimpy_start(CHIMPY_HOP_OFF, 0.0f);
        return;
    }
    // The player can move again.
    chimpy_lock_player(FALSE);
}

// Whether the player is close enough to his stump, and on the ground, to talk.
static s32 chimpy_player_near(Actor *player, f32 radius) {
    f32 dy = chimpy_floor_y - player->y_position;

    return distance_xz(chimpy_spot->x, chimpy_spot->z, player->x_position, player->z_position) <= radius &&
           dy >= -CHIMPY_TALK_FEET_ABOVE && dy <= CHIMPY_TALK_FEET_BELOW;
}

// Starts a talk when the player stops by him (see the top of this file).
static void chimpy_check_talk(Actor *player) {
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;
    s32 standing = player->control_state == STATE_IDLE;
    s32 walking = player->control_state == STATE_WALK;
    s16 to_chimpy;

    if (!chimpy_armed) {
        chimpy_armed = !chimpy_player_near(player, CHIMPY_REARM_RADIUS) &&
                       (!(chimpy_saved & CHIMPY_SAVE_MET) || object_timer - chimpy_last_talk >= CHIMPY_REPEAT_FRAMES);
    }
    if (!chimpy_player_near(player, CHIMPY_TALK_RADIUS) || !grounded) {
        return;
    }
    if (chimpy_carrying() && (standing || walking)) {
        // The orange: he takes it, whatever he said last.
        chimpy_save(CHIMPY_SAVE_DELIVERED | CHIMPY_SAVE_MET);
        jiggy_bk_jingle_play(JINGLE_PUZZLE_SOLVED, 1.0f);
        talk_start(TALK_DELIVER);
    } else if (chimpy_armed && (standing || (walking && !(chimpy_saved & CHIMPY_SAVE_MET)))) {
        chimpy_save(CHIMPY_SAVE_MET);
        talk_start(chimpy_delivered() ? TALK_THANKS : TALK_ASK);
    } else {
        return;
    }
    chimpy_armed = FALSE;
    chimpy_last_talk = object_timer;
    // The player turns to him (as Bottles turns them).
    to_chimpy = func_global_asm_80665DE0(chimpy_pos[0], chimpy_pos[2], player->x_position, player->z_position);
    player->y_rotation = to_chimpy & 0xFFF;
    player->unkEE = to_chimpy & 0xFFF;
}

// Whether his Golden Banana spawned this visit is still in the game's spawner list (it goes
// when the map is loaded again, such as after a death).
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
    if (!reward_due) {
        return;
    }
    // Collected: DK64 set the flag when the player picked it up.
    if (isFlagSet(chimpy_spot->flag, FLAG_TYPE_PERMANENT)) {
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
    if (distance_xz(player->x_position, player->z_position, reward_spot[0], reward_spot[2]) < REWARD_CLEARANCE &&
        player->y_position > reward_spot[1] - REWARD_CLEARANCE) {
        return;
    }
    func_global_asm_806A5DF0(REWARD_GOLDEN_BANANA, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], 0, 0,
                             chimpy_spot->flag, 0);
    // func_global_asm_80689250 puts the new spawner at the head of the list.
    reward_spawner = actor_spawner_pointer;
    if (reward_jingle) {
        jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        reward_jingle = FALSE;
    }
}

// Turns a share of the way to face (x, z), over `frames` frames.
static void chimpy_turn_to(f32 x, f32 z, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, z, chimpy_pos[0], chimpy_pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - chimpy_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        chimpy_yaw += turn * CHIMPY_TURN_RATE;
        if (chimpy_yaw < 0.0f) {
            chimpy_yaw += 4096.0f;
        } else if (chimpy_yaw >= 4096.0f) {
            chimpy_yaw -= 4096.0f;
        }
    }
}

// His chatter (__chlmonkey_playRandomNoise: often, quieter further off, not while talking).
static void chimpy_random_noise(Actor *player, u32 frames) {
    f32 distance = distance_xz(chimpy_pos[0], chimpy_pos[2], player->x_position, player->z_position);

    chimpy_noise_cooldown -= frames;
    if (talk_open || distance >= CHIMPY_NOISE_RADIUS || chimpy_noise_cooldown >= 0) {
        return;
    }
    if (chimpy_random() < CHIMPY_NOISE_CHANCE) {
        f32 volume = (18000.0f / 32767.0f) * (CHIMPY_NOISE_RADIUS - distance) / CHIMPY_NOISE_RADIUS;

        chimpy_noise_cooldown = CHIMPY_NOISE_COOLDOWN;
        jiggy_bk_sfx_play(chimpy_random() < 0.5f ? CHIMPY_SFX_NOISE_1 : CHIMPY_SFX_NOISE_2, volume,
                          0.85f + chimpy_random() * 0.25f);
    }
}

// Until it rises, the stump (with him on it) keeps the player out (DK64 doesn't know it's there).
static void stump_push_player(Actor *player) {
    f32 dx = player->x_position - chimpy_spot->x;
    f32 dz = player->z_position - chimpy_spot->z;
    f32 reach = STUMP_RADIUS + PLAYER_RADIUS;
    f32 distance = _sqrtf(dx * dx + dz * dz);

    if (distance >= reach || player->y_position >= stump_top() + CHIMPY_TALK_FEET_ABOVE || distance < 0.01f) {
        return;
    }
    player->x_position = chimpy_spot->x + dx * reach / distance;
    player->z_position = chimpy_spot->z + dz * reach / distance;
}

// The orange beside Conga: out once he's beaten (and done talking), until it's taken.
static void orange_update(Actor *player, u32 frames) {
    f32 dy;

    if (!conga_beaten && object_timer - conga_checked >= CONGA_CHECK_FRAMES) {
        conga_checked = object_timer;
        conga_beaten = (jiggy_save_get(current_file, CONGA_SAVE_MAP_KEY(current_map)) & CONGA_SAVE_DEFEATED) != 0;
    }
    if (!orange_out) {
        orange_out = conga_beaten && !(chimpy_saved & (CHIMPY_SAVE_ORANGE_TAKEN | CHIMPY_SAVE_DELIVERED)) &&
                     !(D_global_asm_8076A0B1 & CUTSCENE_LOCK);
        return;
    }
    orange_yaw += ORANGE_SPIN_PER_FRAME * frames;
    while (orange_yaw >= 4096.0f) {
        orange_yaw -= 4096.0f;
    }
    dy = player->y_position - orange_pos[1];
    if (dy > -ORANGE_TAKE_HEIGHT && dy < ORANGE_TAKE_HEIGHT &&
        distance_xz(orange_pos[0], orange_pos[2], player->x_position, player->z_position) <= ORANGE_TAKE_RADIUS) {
        orange_out = FALSE;
        chimpy_save(CHIMPY_SAVE_ORANGE_TAKEN);
        jiggy_bk_sfx_play(ORANGE_SFX_PICKUP, 0x7FF8 / 32767.0f, 1.0f);
    }
}

// A frame of his behavior (chlmonkey_update and chchimpystump_update), `frames` game frames on.
static void chimpy_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 t;

    if (frames == 0) {
        return;
    }
    orange_update(player, frames);
    if (!chimpy_delivered()) {
        stump_push_player(player);
    }
    if (!talk_open && !chimpy_holding) {
        chimpy_check_talk(player);
    }

    switch (chimpy_action) {
        case CHIMPY_IDLE:
        case CHIMPY_HOP:
            chimpy_turn_to(player->x_position, player->z_position, frames);
            chimpy_random_noise(player, frames);
            if (chimpy_action == CHIMPY_HOP) {
                chimpy_progress += seconds / CHIMPY_JUMP_SECONDS;
                if (chimpy_progress >= 1.0f) {
                    chimpy_start(CHIMPY_IDLE, 0.0f);
                }
                break;
            }
            chimpy_progress += seconds / CHIMPY_IDLE_SECONDS;
            if (chimpy_progress >= 1.0f) {
                chimpy_progress -= 1.0f;
                if (!talk_open && chimpy_random() < CHIMPY_HOP_CHANCE) {
                    chimpy_start(CHIMPY_HOP, 0.0f);
                }
            }
            break;
        case CHIMPY_HOP_OFF:
            // From the stump's top to the ground beside it, in an arc.
            chimpy_lock_player(TRUE);
            chimpy_turn_to(chimpy_ground[0], chimpy_ground[2], frames);
            chimpy_progress += seconds / CHIMPY_JUMP_SECONDS;
            t = chimpy_progress < 1.0f ? chimpy_progress : 1.0f;
            chimpy_pos[0] = chimpy_spot->x + (chimpy_ground[0] - chimpy_spot->x) * t;
            chimpy_pos[1] = stump_top() + (chimpy_ground[1] - stump_top()) * t + 4.0f * CHIMPY_HOP_ARC * t * (1.0f - t);
            chimpy_pos[2] = chimpy_spot->z + (chimpy_ground[2] - chimpy_spot->z) * t;
            if (chimpy_progress >= 1.0f) {
                chimpy_start(CHIMPY_STUMP_RISE, 0.0f);
                stump_rumble = jiggy_bk_sfx_play(STUMP_SFX_RUMBLE, 0x7FF8 / 32767.0f, 0.2f);
            }
            break;
        case CHIMPY_STUMP_RISE:
            chimpy_lock_player(TRUE);
            chimpy_turn_to(chimpy_spot->x, chimpy_spot->z, frames);
            chimpy_progress += seconds / CHIMPY_IDLE_SECONDS;
            while (chimpy_progress >= 1.0f) {
                chimpy_progress -= 1.0f;
            }
            stump_rise += STUMP_RISE_SPEED * frames;
            if (stump_rise >= STUMP_SUNK) {
                stump_rise = STUMP_SUNK;
                if (stump_rumble > 0) {
                    jiggy_bk_sound_stop(stump_rumble);
                    stump_rumble = 0;
                }
                // Up: the Golden Banana appears on top, and the player can move again.
                chimpy_holding = FALSE;
                chimpy_lock_player(FALSE);
                reward_due = TRUE;
                reward_wait = 0;
                reward_jingle = TRUE;
                chimpy_armed = FALSE;
                chimpy_last_talk = object_timer;
                chimpy_start(CHIMPY_IDLE, chimpy_random());
            }
            break;
    }
    if (!chimpy_holding) {
        reward_update(player, frames);
    }
    if (talk_open) {
        talk_update();
    }
}

// Draws a built model at a spot, turned to `yaw` (12-bit angle units).
static Gfx *chimpy_pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, const f32 pos[3], f32 yaw,
                             Gfx *gfx, u32 gfx_capacity, Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 scale[4][4];

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, chimpy_pose.world);
    chimpy_pose.world[3][0] = pos[0];
    chimpy_pose.world[3][1] = pos[1];
    chimpy_pose.world[3][2] = pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    chimpy_pose.model = model;
    chimpy_pose.block = block;
    chimpy_pose.animation = animation;
    chimpy_pose.progress = progress;
    chimpy_pose.gfx = gfx;
    chimpy_pose.gfx_capacity = gfx_capacity;
    chimpy_pose.mtx = mtx;
    chimpy_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&chimpy_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Chimpy, his stump and the orange once a frame, with the player.
Gfx *jiggy_chimpy_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    f32 stump_pos[3];
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): he's back where he was.
    if ((s32)current_map != chimpy_last_map || object_timer < chimpy_last_timer) {
        // Leaving the map mid-talk (or mid-rise) lets the player go.
        if (talk_open || chimpy_holding) {
            chimpy_lock_player(FALSE);
            talk_open = FALSE;
            chimpy_holding = FALSE;
        }
        if (stump_rumble > 0) {
            jiggy_bk_sound_stop(stump_rumble);
            stump_rumble = 0;
        }
        chimpy_last_map = current_map;
        chimpy_placed = FALSE;
        chimpy_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(chimpy_spots); i++) {
            if (chimpy_spots[i].map == current_map) {
                chimpy_spot = &chimpy_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        chimpy_saved = chimpy_spot != NULL ? jiggy_save_get(current_file, CHIMPY_SAVE_MAP_KEY(current_map)) : 0;
        conga_beaten = chimpy_spot != NULL &&
                       (jiggy_save_get(current_file, CONGA_SAVE_MAP_KEY(current_map)) & CONGA_SAVE_DEFEATED) != 0;
        conga_checked = object_timer;
        orange_out = FALSE;
    }
    if (chimpy_spot == NULL) {
        chimpy_last_timer = object_timer;
        return dl;
    }
    if (chimpy_state == CHIMPY_UNLOADED) {
        chimpy_load();
    }
    if (chimpy_state != CHIMPY_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!chimpy_placed) {
        chimpy_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        chimpy_place(actor);
        chimpy_armed = !chimpy_player_near(actor, CHIMPY_REARM_RADIUS);
        chimpy_last_talk = object_timer - CHIMPY_REPEAT_FRAMES;
    }
    frames = object_timer - chimpy_last_timer;
    chimpy_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player (Bottles' or Conga's talk).
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && !chimpy_holding) {
        frames = 0;
    }
    chimpy_update(actor, frames);

    stump_pos[0] = chimpy_spot->x;
    stump_pos[1] = stump_top() - STUMP_HEIGHT;
    stump_pos[2] = chimpy_spot->z;
    dl = chimpy_pose_draw(dl, STUMP_MODEL, stump_block, -1, 0.0f, stump_pos, 0.0f, stump_gfx[buffer], STUMP_GFX_MAX,
                          stump_mtx[buffer], STUMP_MTX_MAX);
    dl = chimpy_pose_draw(dl, CHIMPY_MODEL, chimpy_block,
                          chimpy_action == CHIMPY_HOP || chimpy_action == CHIMPY_HOP_OFF ? CHIMPY_ANIM_JUMP : CHIMPY_ANIM_IDLE,
                          chimpy_progress, chimpy_pos, chimpy_yaw, chimpy_gfx[buffer], CHIMPY_GFX_MAX, chimpy_mtx[buffer],
                          CHIMPY_MTX_MAX);
    if (orange_out) {
        dl = chimpy_pose_draw(dl, ORANGE_MODEL, orange_block, -1, 0.0f, orange_pos, orange_yaw, orange_gfx[buffer],
                              ORANGE_GFX_MAX, orange_mtx[buffer], ORANGE_MTX_MAX);
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

// The carried orange in the HUD, HUD_ORANGE_SIZE tall, its right edge HUD_RIGHT_MARGIN in from the
// screen's; it fades in when taken and out when delivered.
static Gfx *hud_orange_draw(Gfx *dl) {
    s32 carrying = chimpy_spot != NULL && chimpy_carrying();
    s32 display_width;
    s32 half;
    s32 x;
    s32 step;
    s32 h;

    if (hud_orange_width == 0) {
        return dl;
    }
    if (carrying) {
        hud_orange_alpha = hud_orange_alpha + HUD_FADE_STEP > 0xFF ? 0xFF : hud_orange_alpha + HUD_FADE_STEP;
    } else {
        hud_orange_alpha = hud_orange_alpha - HUD_FADE_STEP < 0 ? 0 : hud_orange_alpha - HUD_FADE_STEP;
    }
    if (hud_orange_alpha == 0) {
        return dl;
    }
    display_width = hud_orange_width * HUD_ORANGE_SIZE / hud_orange_height;
    // Texels a pixel, in 5.10 fixed point.
    step = (hud_orange_height << 10) / HUD_ORANGE_SIZE;
    x = D_global_asm_80744490 - HUD_RIGHT_MARGIN - display_width;
    half = hud_orange_height / 2;

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
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, hud_orange_alpha);
    // Two halves, top and bottom, each a TMEM load.
    for (h = 0; h < 2; h++) {
        s32 first = h * half;
        s32 last = h == 0 ? half - 1 : hud_orange_height - 1;
        s32 y0 = HUD_Y + first * HUD_ORANGE_SIZE / hud_orange_height;
        s32 y1 = HUD_Y + (last + 1) * HUD_ORANGE_SIZE / hud_orange_height;

        gDPLoadTextureTile(dl++, hud_orange_texels, G_IM_FMT_RGBA, G_IM_SIZ_16b,
            hud_orange_width, hud_orange_height,
            0, first, hud_orange_width - 1, last,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(dl++,
            x << 2, y0 << 2, (x + display_width) << 2, y1 << 2,
            G_TX_RENDERTILE,
            0, first << 5, step, step);
    }
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_RIGHT);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the carried
// orange, and his text box while he talks (drawn as jiggy_bottles_hud_draw draws Bottles').
Gfx *jiggy_chimpy_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (chimpy_state != CHIMPY_READY || cc_number_of_players != 1) {
        return dl;
    }
    dl = hud_orange_draw(dl);
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
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"CHIMPY", 1);
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
