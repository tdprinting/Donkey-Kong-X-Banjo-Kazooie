// Conga: Mumbo's Mountain's orange-throwing ape, read from the player's Banjo-Kazooie ROM, as
// a mini boss at the edge of Jungle Japes' first area, where the two worlds have collided.
//
// Like chConga (src/MM/ch/conga.c), Conga (ASSET_35C_MODEL_CONGA) idles with his idle
// animation, turning to face the player, grunting now and then and beating his chest; when the
// player comes within range he throws oranges (ASSET_2D2_MODEL_ORANGE, chorange.c) in an arc
// at them, aimed as __chConga_sendOrangeProjectile aims them (a fixed upward speed, the
// sideways speed worked out from how long the orange takes to fall to the player's height; close
// in, the oranges fly wild). An orange that hits the player hurts them through DK64's own path
// for an enemy's hit, setAction(0xE), as jiggy_grublins.c does; an orange that misses splats
// on the floor (SFX_2F_ORANGE_SPLAT).
//
// The first time the player comes near, he taunts them in a text box. He's beaten by three
// hits, each one his "ow" animation and noise (func_8038708C) and a short invulnerability; a
// hit is one of the player's attacks within reach (the same control states as the Grublins:
// punches, rolls, aerial attacks and the Simian Slam) or one of the player's own shots (DK64's
// gun projectiles, and so Banjo's eggs, which are Coconut Shooter shots: jiggy_banjo_eggs.c).
// Beaten, he plays his "ow" again with Banjo-Kazooie's crying (chConga's state 8), mopes
// (ASSET_53_ANIM_CONGA_DEFEAT) and says a line in the text box; then a Golden Banana (drawn as
// a Jiggy by jiggy_collectibles.c) appears beside him, as his Jiggy does in Banjo-Kazooie
// (func_80387100). He mopes until it's collected, then idles, harmless.
//
// The Golden Banana is spawned as jiggy_jinjo_jiggy.c spawns the Jinjos' one: an
// ACTOR_GOLDEN_BANANA from func_global_asm_806A5DF0, tied to a DK64 permanent flag that DK64
// sets (and saves) when the player collects it, spawned again on every visit until then. The
// flag is 0x31D: past the last one the game uses (PERMFLAG_PROGRESS_K_LUMSY_FREE, 0x315), inside
// the save's permanent block (bits 0 to 0x31F), counted by no global flag
// (D_global_asm_80756024's ranges), and next to the Jinjo Jiggies' 0x31E and 0x31F.
//
// The taunt and the defeat are kept with the DK64 save file (jiggy_save_get/set, key
// 0x43000000 | map), so a beaten Conga stays beaten.
//
// The text box is Bottles' (jiggy_bottles.c): Banjo-Kazooie's Conga portrait
// (ASSET_7F2_SPRITE_CONGA, its mouth moving as the text types out) beside DK64's dialogue font,
// with Conga's talking sounds (SFX_E3/E4/E5_KONGA_TALKING, as gczoombox plays them), and the
// game's cutscene lock holding the player while it's up.
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his
// text box in the HUD pass (jiggy_conga_hud_draw, patches_ui.c). Without a Banjo-Kazooie ROM
// nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

#define CONGA_MODEL 0x35C  // ASSET_35C_MODEL_CONGA
#define ORANGE_MODEL 0x2D2 // ASSET_2D2_MODEL_ORANGE
#define CONGA_PORTRAIT 0x7F2 // ASSET_7F2_SPRITE_CONGA
// Conga converts to about 20 KiB, with 32 bones and about 50 pose commands at rest; the orange
// to about 1.6 KiB, with no bones and 6 commands.
#define CONGA_BLOCK_MAX 0x6000
#define CONGA_GFX_MAX 160
#define CONGA_MTX_MAX 40
#define ORANGE_BLOCK_MAX 0x1000
#define ORANGE_GFX_MAX 16
#define ORANGE_MTX_MAX 4
#define ORANGE_MAX 4

// Banjo-Kazooie animations (chCongaAnimations) and how long each plays, in seconds.
#define CONGA_ANIM_IDLE 0x51       // ASSET_51_ANIM_CONGA_IDLE
#define CONGA_ANIM_OW 0x52         // ASSET_52_ANIM_CONGA_OW: hit, and beaten
#define CONGA_ANIM_DEFEAT 0x53     // ASSET_53_ANIM_CONGA_DEFEAT: moping after
#define CONGA_ANIM_THROW 0x54      // ASSET_54_ANIM_CONGA_THROW
#define CONGA_ANIM_BEAT_CHEST 0x55 // ASSET_55_ANIM_CONGA_BEAT_CHEST
#define CONGA_ANIM_RAISE_ARMS 0x56 // ASSET_56_ANIM_CONGA_RAISE_ARMS: into (and, backward, out of)
                                   // beating his chest
#define CONGA_IDLE_SECONDS 3.8f
#define CONGA_OW_SECONDS 1.0f
#define CONGA_DEFEAT_SECONDS 2.0f
#define CONGA_THROW_SECONDS 1.4f
#define CONGA_BEAT_CHEST_SECONDS 0.4f
#define CONGA_RAISE_ARMS_SECONDS 0.9f
// Where through the throw the orange leaves his hand, and his swipe sound.
#define CONGA_THROW_RELEASE_AT 0.56f
#define CONGA_THROW_SWIPE_AT 0.6f
// Where through his idle he may beat his chest (subaddie_maybe_set_state_position_direction's
// 0.38 chance at 0.66), and where he comes back into it after (0.76).
#define CONGA_IDLE_BEAT_AT 0.66f
#define CONGA_IDLE_BEAT_CHANCE 0.38f
#define CONGA_IDLE_RETURN_AT 0.76f
// Between throws, the chance he beats his chest first.
#define CONGA_THROW_BEAT_CHANCE 0.25f

// Banjo-Kazooie sounds (sfx_e) and how chConga plays them.
#define CONGA_SFX_NOISE_1 0x22  // SFX_22_KONGA_NOISE_1: grunts while idle, 22000/32767, 1.0
#define CONGA_SFX_NOISE_2 0x23  // SFX_23_KONGA_NOISE_2: the same
#define CONGA_SFX_OW 0x24       // SFX_24_KONGA_NOISE_3: hit, 22000/32767, 0.9-1.1
#define CONGA_SFX_CHEST 0x3FB   // SFX_3FB_UNKNOWN: each chest beat (0.4 and 0.9), 0x7FF8/32767, 1.0
#define CONGA_SFX_SWIPE 0x02    // SFX_2_CLAW_SWIPE: throwing, 22000/32767, 0.7
#define CONGA_SFX_SPLAT 0x2F    // SFX_2F_ORANGE_SPLAT: an orange landing, 0x7FF8/32767, 1.0
#define CONGA_SFX_CRYING 0x84   // SFX_84_GOBI_CRYING: beaten, twice, 32750/32767, 0.8
// Conga's voice in the text box (gczoombox), one of these at 28000/32767, about pitch 1.1.
static const u16 conga_talk_sfx[] = {0xE3, 0xE4, 0xE5};
#define CONGA_TALK_SFX_EVERY 5 // Characters typed out between sounds.
// Banjo-Kazooie's Jiggy appearing (COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767).
#define JINGLE_JIGGY_SPAWN 0x3D
// Banjo-Kazooie fades these out between 1250 and 2500 units; scaled to DK64 units.
#define SFX_NEAR 310.0f
#define SFX_FAR 625.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// How fast he turns to face the player (subaddie_turnToYaw(this, 3.0f)).
#define CONGA_TURN_RATE 0.19f
// He throws when the player is this close (chConga's 1000 units, scaled) and this far above or
// below him; closer than this (200 units), his oranges fly wild.
#define CONGA_THROW_RADIUS 250.0f
#define CONGA_THROW_HEIGHT 120.0f
#define CONGA_WILD_RADIUS 50.0f
// His body, for the player's attacks and shots: this wide and tall.
#define CONGA_BODY_RADIUS 28.0f
#define CONGA_BODY_HEIGHT 75.0f
// How long after a hit before another counts (his "ow", and a little more), and how many beat him.
#define CONGA_INVULNERABLE_SECONDS 1.5f
#define CONGA_HITS 3
// The player's attacks reach this far (the Simian Slam a little further), within this height;
// punches only hit what the player faces (within this many 12-bit angle units either side).
#define ATTACK_REACH 60.0f
#define SLAM_REACH 76.0f
#define ATTACK_HEIGHT 60.0f
#define PUNCH_ARC 0x300
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing him on the floor under his spot: looked for from this far above the spot, and only
// trusted this close to the spot's height.
#define CONGA_FLOOR_PROBE 40.0f
#define CONGA_FLOOR_MAX_STEP 60.0f

// The oranges, in DK64 units and game frames: they leave his hand this high and this far in
// front of him, going up at chConga's 60 units a frame (scaled), falling at chorange's 5 a frame
// each frame (scaled); they hit the player within this distance of the player's middle (from
// their feet to this high), and give up after this many frames.
#define ORANGE_HAND_HEIGHT 55.0f
#define ORANGE_HAND_FORWARD 12.0f
#define ORANGE_UP_SPEED (60.0f * BK_TO_DK64)
#define ORANGE_GRAVITY (5.0f * BK_TO_DK64)
#define ORANGE_HIT_RADIUS 16.0f
#define ORANGE_HIT_HEIGHT 35.0f
#define ORANGE_FLOOR_PROBE 20.0f
#define ORANGE_MAX_FRAMES 150
#define ORANGE_AIM_MAX_FRAMES 200

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

// The Golden Banana (see the top of this file): func_global_asm_806A5DF0's actor id, the
// spawner type func_global_asm_80689250 gives it, and how it's spawned and respawned, as
// jiggy_jinjo_jiggy.c does.
#define REWARD_GOLDEN_BANANA 0x2D // ACTOR_GOLDEN_BANANA
#define REWARD_SPAWNER_TYPE (REWARD_GOLDEN_BANANA - 0x10)
#define FLAG_TYPE_PERMANENT 0
#define REWARD_HEIGHT 12.5f
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
// Frames 0 to 8 move his mouth (gczoombox's 9 talking frames for Conga).
#define PORTRAIT_TALK_FRAMES 9
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

// Where he is, map by map. Jungle Japes' first area: by the bamboo at its east edge (further
// north along that edge is out of bounds), clear of Bottles' molehill (1120, 560), the notes'
// trail and the collectibles (the nearest, a Mumbo Token at (1400, 330) and a Red Feather at
// (1330, 380), are 95 and 108 units off). His Golden Banana appears a little in front of him,
// toward the area.
typedef struct {
    Maps map;
    f32 x, y, z;          // Where he stands (y is the area's floor, if the floor isn't found).
    f32 reward_x, reward_z; // Where his Golden Banana appears.
    s16 flag;             // Its DK64 permanent flag.
} CongaPlacement;
static const CongaPlacement conga_spots[] = {
    {MAP_JAPES, 1430.0f, 280.0f, 420.0f, 1395.0f, 425.0f, 0x31D},
};

// What he says. Each page is one press of A; lines are broken to fit the box when shown.
static const char *const conga_taunt[] = {
    "GRR! NO BEAR OR MONKEY TAKES MY ORANGES!",
};
static const char *const conga_defeat[] = {
    "OOH OOH! YOU WIN, BANJO AND KONG! TAKE THIS SHINY THING!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): CONGA_SAVE_* bits.
#define CONGA_SAVE_MAP_KEY(map) (0x43000000 | (u32)(map)) // 'C'
#define CONGA_SAVE_TAUNTED 0x1
#define CONGA_SAVE_DEFEATED 0x2

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
// The actors that update this frame (as jiggy_banjo_eggs.c reads them).
typedef struct {
    Actor *actor;
    s32 unk4;
} CongaActiveActor;
extern CongaActiveActor D_global_asm_807FB930[];
extern u16 D_global_asm_807FBB34; // How many.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y and returns nonzero, or leaves it as it was.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// Whether the player can be hurt now (not invulnerable, not already knocked down).
s32 func_global_asm_806EB400(Actor *player);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
u8 isFlagSet(s16 flagIndex, u8 flagType);
void func_global_asm_806A5DF0(s16 actor, f32 x, f32 y, f32 z, s16 angle, u8 control_state, s16 flag, s32 arg7);

typedef enum {
    CONGA_UNLOADED,
    CONGA_READY,
    CONGA_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or the portrait didn't convert.
} CongaLoadState;

typedef enum {
    CONGA_IDLE,
    CONGA_RAISE_ARMS, // Into beating his chest.
    CONGA_BEAT_CHEST,
    CONGA_LOWER_ARMS, // Out of it (the raise, backward).
    CONGA_THROW,
    CONGA_HIT,        // Ow.
    CONGA_BEATEN,     // Ow, crying.
    CONGA_MOPING,     // Until his Golden Banana is collected.
    CONGA_CALM,       // After: idle, harmless.
} CongaAction;

typedef enum {
    TALK_TAUNT,
    TALK_DEFEAT,
} CongaTalk;

typedef struct {
    s32 live;
    f32 pos[3];
    f32 vel[3]; // DK64 units a frame.
    s32 frames;
} Orange;

static u8 conga_block[CONGA_BLOCK_MAX] __attribute__((aligned(16)));
static u8 orange_block[ORANGE_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx conga_gfx[2][CONGA_GFX_MAX];
static Mtx conga_mtx[2][CONGA_MTX_MAX];
static Gfx orange_gfx[2][ORANGE_MAX][ORANGE_GFX_MAX];
static Mtx orange_mtx[2][ORANGE_MAX][ORANGE_MTX_MAX];
static JiggyBkPose conga_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static CongaLoadState conga_state = CONGA_UNLOADED;

static s32 conga_last_map = -1;
static u32 conga_last_timer;
static const CongaPlacement *conga_spot; // This map's, or NULL.
static s32 conga_placed;
static f32 conga_pos[3];
static f32 conga_yaw; // 12-bit angle units, as a float so the turn eases.
static CongaAction conga_action;
static f32 conga_progress; // 0 to 1 through the action's animation.
static s32 conga_beats;    // Chest beats so far this time.
static s32 conga_hits;
static f32 conga_invulnerable; // Seconds left.
static u32 conga_saved;        // CONGA_SAVE_* bits.
static Orange oranges[ORANGE_MAX];
static u32 conga_random_state = 0x434F4E47;

// His Golden Banana: where it appears, whether it's due (after his defeat line), its spawner
// this visit (NULL until spawned), frames until it's spawned again, and its jingle.
static f32 reward_spot[3];
static s32 reward_due;
static ActorSpawner *reward_spawner;
static s32 reward_wait;
static s32 reward_jingle;

// The dialogue (as jiggy_bottles.c keeps it).
static CongaTalk talk_kind;
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

static f32 conga_random(void) {
    conga_random_state = conga_random_state * 1664525 + 1013904223;
    return (conga_random_state >> 8) * (1.0f / 16777216.0f);
}

static f32 conga_random_between(f32 low, f32 high) {
    return low + conga_random() * (high - low);
}

static void conga_load(void) {
    s32 size;
    s32 count;
    s32 i;

    conga_state = CONGA_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(CONGA_MODEL, conga_block, sizeof(conga_block));
    if (size <= 0 || size > (s32)sizeof(conga_block)) {
        return;
    }
    size = jiggy_bk_model_build(ORANGE_MODEL, orange_block, sizeof(orange_block));
    if (size <= 0 || size > (s32)sizeof(orange_block)) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(CONGA_PORTRAIT);
    if (count <= 0) {
        return;
    }
    if (count > PORTRAIT_MAX_FRAMES) {
        count = PORTRAIT_MAX_FRAMES;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(CONGA_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
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
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        conga_pose.selectors[i] = 0;
    }
    conga_pose.selectors[1] = 1;
    conga_state = CONGA_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// A Banjo-Kazooie sound from where Conga is, fading out with distance from the player.
static void conga_sfx(Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(conga_pos[0], conga_pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

// True when `at` is passed going from `from` to `to` through an animation.
static s32 conga_passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

static void conga_start(CongaAction action, f32 progress) {
    conga_action = action;
    conga_progress = progress;
}

// The floor under (x, z), looked for from `probe_above` over `y`; `y` if there's none near.
static f32 conga_floor(f32 x, f32 y, f32 z, f32 probe_above, f32 max_step) {
    f32 probe = y + probe_above;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < max_step && y - floor < max_step) {
        return floor;
    }
    return y;
}

// Stands him over the floor at his spot, and finds where his Golden Banana goes.
static void conga_place(Actor *player) {
    conga_pos[0] = conga_spot->x;
    conga_pos[1] = conga_floor(conga_spot->x, conga_spot->y, conga_spot->z, CONGA_FLOOR_PROBE, CONGA_FLOOR_MAX_STEP);
    conga_pos[2] = conga_spot->z;
    reward_spot[0] = conga_spot->reward_x;
    reward_spot[1] = conga_floor(conga_spot->reward_x, conga_pos[1], conga_spot->reward_z, CONGA_FLOOR_PROBE, CONGA_FLOOR_MAX_STEP);
    reward_spot[2] = conga_spot->reward_z;
    conga_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, conga_pos[0], conga_pos[2]) & 0xFFF;
    conga_hits = 0;
    conga_invulnerable = 0.0f;
    conga_beats = 0;
    reward_spawner = NULL;
    reward_wait = 0;
    reward_jingle = FALSE;
    if (!(conga_saved & CONGA_SAVE_DEFEATED)) {
        conga_start(CONGA_IDLE, conga_random());
        reward_due = FALSE;
    } else if (isFlagSet(conga_spot->flag, FLAG_TYPE_PERMANENT)) {
        conga_start(CONGA_CALM, conga_random());
        reward_due = FALSE;
    } else {
        // Beaten before, his Golden Banana not yet collected: it's back beside him.
        conga_start(CONGA_MOPING, conga_random());
        reward_due = TRUE;
    }
    conga_placed = TRUE;
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
static void conga_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void conga_clear_oranges(void) {
    s32 i;

    for (i = 0; i < ORANGE_MAX; i++) {
        oranges[i].live = FALSE;
    }
}

static void talk_start(CongaTalk kind) {
    talk_kind = kind;
    if (kind == TALK_TAUNT) {
        talk_pages = conga_taunt;
        talk_page_count = ARRAY_COUNT_OF(conga_taunt);
    } else {
        talk_pages = conga_defeat;
        talk_page_count = ARRAY_COUNT_OF(conga_defeat);
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    conga_lock_player(TRUE);
    // No orange lands while he talks.
    conga_clear_oranges();
}

static void conga_save(u32 bits) {
    conga_saved |= bits;
    jiggy_save_set(current_file, CONGA_SAVE_MAP_KEY(current_map), conga_saved);
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
        if (talk_shown / CONGA_TALK_SFX_EVERY != before / CONGA_TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 pick = (u32)(conga_random() * ARRAY_COUNT_OF(conga_talk_sfx)) % ARRAY_COUNT_OF(conga_talk_sfx);

            jiggy_bk_sfx_play(conga_talk_sfx[pick], 28000.0f / 32767.0f, 1.0f + conga_random() * 0.2f);
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
    conga_lock_player(FALSE);
    if (talk_kind == TALK_TAUNT) {
        conga_save(CONGA_SAVE_TAUNTED);
        // Done beating his chest at them: the fight's on.
        if (conga_action == CONGA_RAISE_ARMS || conga_action == CONGA_BEAT_CHEST) {
            conga_start(CONGA_LOWER_ARMS, 0.0f);
        }
    } else {
        // His Golden Banana appears, with Banjo-Kazooie's Jiggy jingle.
        reward_due = TRUE;
        reward_wait = 0;
        reward_jingle = TRUE;
    }
}

// Whether the player is where he throws at them.
static s32 conga_in_range(Actor *player) {
    f32 dy = player->y_position - conga_pos[1];

    return distance_xz(conga_pos[0], conga_pos[2], player->x_position, player->z_position) <= CONGA_THROW_RADIUS &&
           dy <= CONGA_THROW_HEIGHT && dy >= -CONGA_THROW_HEIGHT;
}

// Whether the player's attack hits him this frame (as jiggy_grublins.c checks, for his size).
static s32 conga_attacked(Actor *player) {
    f32 distance = distance_xz(conga_pos[0], conga_pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - conga_pos[1];
    s32 to_conga;
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
            // Close in, a punch always lands; further out, only in front of the player.
            if (distance <= CONGA_BODY_RADIUS) {
                return TRUE;
            }
            to_conga = func_global_asm_80665DE0(conga_pos[0], conga_pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_conga - (player->y_rotation & 0xFFF)) & 0xFFF;
            return off <= PUNCH_ARC || off >= 0x1000 - PUNCH_ARC;
        case STATE_RUNNING_ATTACK:
        case STATE_AERIAL_ATTACK:
        case STATE_AERIAL_ATTACK_DIDDY:
        case STATE_AERIAL_ATTACK_CHUNKY:
            return distance <= ATTACK_REACH;
        default:
            return FALSE;
    }
}

// Whether one of the player's own shots (a gun's projectile, or one of Banjo's eggs) is in him.
static s32 conga_shot(Actor *player) {
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
        dy = shot->y_position - conga_pos[1];
        if (dy >= 0.0f && dy <= CONGA_BODY_HEIGHT &&
            distance_xz(conga_pos[0], conga_pos[2], shot->x_position, shot->z_position) <= CONGA_BODY_RADIUS) {
            return TRUE;
        }
    }
    return FALSE;
}

// Hit (func_80387168): ow, or beaten on the third.
static void conga_hurt(Actor *player) {
    conga_hits++;
    conga_invulnerable = CONGA_INVULNERABLE_SECONDS;
    if (conga_hits >= CONGA_HITS) {
        conga_start(CONGA_BEATEN, 0.0f);
        conga_clear_oranges();
        conga_save(CONGA_SAVE_DEFEATED);
        conga_sfx(player, CONGA_SFX_CRYING, 32750.0f / 32767.0f, 0.8f);
        conga_sfx(player, CONGA_SFX_CRYING, 32750.0f / 32767.0f, 0.8f);
        return;
    }
    conga_start(CONGA_HIT, 0.0f);
    conga_sfx(player, CONGA_SFX_OW, 22000.0f / 32767.0f, conga_random_between(0.9f, 1.1f));
}

// Throws an orange at the player (__chConga_sendOrangeProjectile).
static void conga_throw_orange(Actor *player) {
    Orange *o = NULL;
    f32 y, vy;
    s32 frames;
    s32 i;

    for (i = 0; i < ORANGE_MAX; i++) {
        if (!oranges[i].live) {
            o = &oranges[i];
            break;
        }
    }
    if (o == NULL) {
        return;
    }
    o->pos[0] = conga_pos[0] + func_global_asm_80612794((s16)conga_yaw) * ORANGE_HAND_FORWARD;
    o->pos[1] = conga_pos[1] + ORANGE_HAND_HEIGHT;
    o->pos[2] = conga_pos[2] + func_global_asm_80612790((s16)conga_yaw) * ORANGE_HAND_FORWARD;
    o->vel[0] = player->x_position - o->pos[0];
    o->vel[1] = ORANGE_UP_SPEED;
    o->vel[2] = player->z_position - o->pos[2];
    // Right under him, they fly wild.
    if (distance_xz(conga_pos[0], conga_pos[2], player->x_position, player->z_position) < CONGA_WILD_RADIUS) {
        f32 spread_x = conga_random_between(2.4f, 4.4f);
        f32 spread_z = conga_random_between(2.4f, 4.4f);

        o->vel[0] *= conga_random() < 0.5f ? spread_x : -spread_x;
        o->vel[1] = conga_random_between(1.8f, 2.2f) * ORANGE_UP_SPEED;
        o->vel[2] *= conga_random() < 0.5f ? spread_z : -spread_z;
    }
    // How many frames it takes to come down to the player's height, so it's there then.
    y = o->pos[1];
    vy = o->vel[1];
    for (frames = 0; !(y < player->y_position && vy < 0.0f) && frames < ORANGE_AIM_MAX_FRAMES; frames++) {
        vy -= ORANGE_GRAVITY;
        y += vy;
    }
    if (frames < 1) {
        frames = 1;
    }
    o->vel[0] /= frames;
    o->vel[2] /= frames;
    o->frames = 0;
    o->live = TRUE;
}

// Hurts the player through DK64's own enemy hit (as jiggy_grublins.c does), knocking them away
// from Conga.
static void conga_hit_player(Actor *player) {
    // The knockback turns the player's heading round, so it's set toward Conga first.
    player->unkEE = func_global_asm_80665DE0(conga_pos[0], conga_pos[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
}

// The oranges in flight (chorange_update), `frames` game frames on.
static void oranges_update(Actor *player, u32 frames) {
    s32 i;
    u32 f;

    for (i = 0; i < ORANGE_MAX; i++) {
        Orange *o = &oranges[i];

        for (f = 0; f < frames && o->live; f++) {
            f32 dy;
            f32 floor;

            o->pos[0] += o->vel[0];
            o->vel[1] -= ORANGE_GRAVITY;
            o->pos[1] += o->vel[1];
            o->pos[2] += o->vel[2];
            o->frames++;
            dy = o->pos[1] - player->y_position;
            if (dy >= 0.0f && dy <= ORANGE_HIT_HEIGHT &&
                distance_xz(o->pos[0], o->pos[2], player->x_position, player->z_position) <= ORANGE_HIT_RADIUS) {
                o->live = FALSE;
                jiggy_bk_sfx_play(CONGA_SFX_SPLAT, 0x7FF8 / 32767.0f, 1.0f);
                if (func_global_asm_806EB400(player)) {
                    conga_hit_player(player);
                }
                break;
            }
            floor = o->pos[1] + ORANGE_FLOOR_PROBE;
            if (!func_global_asm_80667110(o->pos[0], o->pos[2], &floor) || floor > o->pos[1] + ORANGE_FLOOR_PROBE) {
                floor = conga_spot->y;
            }
            if (o->vel[1] < 0.0f && o->pos[1] <= floor) {
                o->live = FALSE;
                conga_sfx(player, CONGA_SFX_SPLAT, 0x7FF8 / 32767.0f, 1.0f);
                break;
            }
            if (o->frames >= ORANGE_MAX_FRAMES) {
                o->live = FALSE;
            }
        }
    }
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

// Spawns his Golden Banana while it's due and not collected (as jiggy_jinjo_jiggy_update does).
static void reward_update(Actor *player, u32 frames) {
    f32 dx, dz;

    if (!reward_due) {
        return;
    }
    // Collected: DK64 set the flag when the player picked it up.
    if (isFlagSet(conga_spot->flag, FLAG_TYPE_PERMANENT)) {
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
                             conga_spot->flag, 0);
    // func_global_asm_80689250 puts the new spawner at the head of the list.
    reward_spawner = actor_spawner_pointer;
    if (reward_jingle) {
        jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        reward_jingle = FALSE;
    }
}

// Turns a share of the way to face the player, over `frames` frames.
static void conga_turn_to_player(Actor *player, u32 frames) {
    s16 target = func_global_asm_80665DE0(player->x_position, player->z_position, conga_pos[0], conga_pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - conga_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        conga_yaw += turn * CONGA_TURN_RATE;
        if (conga_yaw < 0.0f) {
            conga_yaw += 4096.0f;
        } else if (conga_yaw >= 4096.0f) {
            conga_yaw -= 4096.0f;
        }
    }
}

// His grunts while idle (__chConga_playRandomNoise: now and then, not while talking).
static void conga_random_noise(Actor *player) {
    if ((object_timer & 0xF) == 0xB && conga_random() > 0.85f && !talk_open) {
        conga_sfx(player, conga_random() < 0.5f ? CONGA_SFX_NOISE_1 : CONGA_SFX_NOISE_2, 22000.0f / 32767.0f, 1.0f);
    }
}

// A frame of his behavior (chConga_update), `frames` game frames on.
static void conga_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = conga_progress;
    s32 fighting = conga_action != CONGA_BEATEN && conga_action != CONGA_MOPING && conga_action != CONGA_CALM;
    s32 in_range = conga_in_range(player);
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;

    if (frames == 0) {
        return;
    }
    if (fighting && !talk_open) {
        if (conga_invulnerable > 0.0f) {
            conga_invulnerable -= seconds;
        } else if (conga_attacked(player) || conga_shot(player)) {
            conga_hurt(player);
            before = 0.0f;
        }
    }
    // The first time the player comes near, he taunts them, beating his chest.
    if (fighting && !talk_open && !(conga_saved & CONGA_SAVE_TAUNTED) && in_range && grounded &&
        conga_action != CONGA_HIT) {
        talk_start(TALK_TAUNT);
        if (conga_action != CONGA_BEAT_CHEST) {
            conga_start(CONGA_RAISE_ARMS, 0.0f);
            before = 0.0f;
        }
    }

    switch (conga_action) {
        case CONGA_IDLE:
            conga_turn_to_player(player, frames);
            conga_random_noise(player);
            conga_progress += seconds / CONGA_IDLE_SECONDS;
            if ((conga_saved & CONGA_SAVE_TAUNTED) && in_range && !talk_open) {
                conga_start(CONGA_THROW, 0.0f);
                break;
            }
            if (conga_passed(before, conga_progress, CONGA_IDLE_BEAT_AT) && conga_random() < CONGA_IDLE_BEAT_CHANCE) {
                conga_start(CONGA_RAISE_ARMS, 0.0f);
                break;
            }
            while (conga_progress >= 1.0f) {
                conga_progress -= 1.0f;
            }
            break;
        case CONGA_RAISE_ARMS:
            conga_turn_to_player(player, frames);
            conga_progress += seconds / CONGA_RAISE_ARMS_SECONDS;
            if (conga_progress >= 1.0f) {
                conga_start(CONGA_BEAT_CHEST, 0.0f);
                conga_beats = 0;
            }
            break;
        case CONGA_BEAT_CHEST:
            conga_random_noise(player);
            conga_progress += seconds / CONGA_BEAT_CHEST_SECONDS;
            if (conga_passed(before, conga_progress, 0.4f) || conga_passed(before, conga_progress, 0.9f)) {
                conga_sfx(player, CONGA_SFX_CHEST, 0x7FF8 / 32767.0f, 1.0f);
            }
            if (conga_progress >= 1.0f) {
                conga_progress -= 1.0f;
                conga_beats++;
                // A couple of beats at least; he keeps on while he taunts.
                if (!talk_open && conga_beats >= 2 && conga_random() < (in_range ? 1.0f : 0.4f)) {
                    conga_start(CONGA_LOWER_ARMS, 0.0f);
                }
            }
            break;
        case CONGA_LOWER_ARMS:
            conga_progress += seconds / CONGA_RAISE_ARMS_SECONDS;
            if (conga_progress >= 1.0f) {
                conga_start(CONGA_IDLE, CONGA_IDLE_RETURN_AT);
            }
            break;
        case CONGA_THROW:
            conga_turn_to_player(player, frames);
            conga_progress += seconds / CONGA_THROW_SECONDS;
            if (conga_passed(before, conga_progress, CONGA_THROW_RELEASE_AT)) {
                conga_throw_orange(player);
            }
            if (conga_passed(before, conga_progress, CONGA_THROW_SWIPE_AT)) {
                conga_sfx(player, CONGA_SFX_SWIPE, 22000.0f / 32767.0f, 0.7f);
            }
            if (conga_progress >= 1.0f) {
                if (!in_range) {
                    conga_start(CONGA_IDLE, 0.0f);
                } else if (conga_random() < CONGA_THROW_BEAT_CHANCE) {
                    conga_start(CONGA_RAISE_ARMS, 0.0f);
                } else {
                    conga_start(CONGA_THROW, 0.0f);
                }
            }
            break;
        case CONGA_HIT:
            conga_progress += seconds / CONGA_OW_SECONDS;
            if (conga_progress >= 1.0f) {
                conga_start(CONGA_IDLE, 0.0f);
            }
            break;
        case CONGA_BEATEN:
            conga_progress += seconds / CONGA_OW_SECONDS;
            if (conga_progress >= 1.0f) {
                conga_start(CONGA_MOPING, 0.0f);
                talk_start(TALK_DEFEAT);
            }
            break;
        case CONGA_MOPING:
            conga_progress += seconds / CONGA_DEFEAT_SECONDS;
            while (conga_progress >= 1.0f) {
                conga_progress -= 1.0f;
            }
            // Once the Golden Banana's collected, he gets over it.
            if (!talk_open && isFlagSet(conga_spot->flag, FLAG_TYPE_PERMANENT)) {
                conga_start(CONGA_CALM, 0.0f);
            }
            break;
        case CONGA_CALM:
            conga_turn_to_player(player, frames);
            conga_random_noise(player);
            conga_progress += seconds / CONGA_IDLE_SECONDS;
            while (conga_progress >= 1.0f) {
                conga_progress -= 1.0f;
            }
            break;
    }
    oranges_update(player, frames);
    reward_update(player, frames);
    if (talk_open) {
        talk_update();
    }
}

// Draws a built model at a spot, turned to `yaw` (12-bit angle units).
static Gfx *conga_pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, const f32 pos[3], f32 yaw,
                            Gfx *gfx, u32 gfx_capacity, Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 scale[4][4];

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, conga_pose.world);
    conga_pose.world[3][0] = pos[0];
    conga_pose.world[3][1] = pos[1];
    conga_pose.world[3][2] = pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    conga_pose.model = model;
    conga_pose.block = block;
    conga_pose.animation = animation;
    conga_pose.progress = progress;
    conga_pose.gfx = gfx;
    conga_pose.gfx_capacity = gfx_capacity;
    conga_pose.mtx = mtx;
    conga_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&conga_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Conga and his oranges once a frame, with the player.
Gfx *jiggy_conga_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    s32 animation;
    f32 progress;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): he's back where he was.
    if ((s32)current_map != conga_last_map || object_timer < conga_last_timer) {
        // Leaving the map mid-talk lets the player go.
        if (talk_open) {
            conga_lock_player(FALSE);
            talk_open = FALSE;
        }
        conga_last_map = current_map;
        conga_placed = FALSE;
        conga_spot = NULL;
        conga_clear_oranges();
        for (i = 0; i < ARRAY_COUNT_OF(conga_spots); i++) {
            if (conga_spots[i].map == current_map) {
                conga_spot = &conga_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        conga_saved = conga_spot != NULL ? jiggy_save_get(current_file, CONGA_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (conga_spot == NULL) {
        conga_last_timer = object_timer;
        return dl;
    }
    if (conga_state == CONGA_UNLOADED) {
        conga_load();
    }
    if (conga_state != CONGA_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!conga_placed) {
        conga_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        conga_place(actor);
    }
    frames = object_timer - conga_last_timer;
    conga_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player (Bottles' talk, for one).
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open) {
        frames = 0;
    }
    conga_update(actor, frames);

    progress = conga_progress;
    switch (conga_action) {
        case CONGA_RAISE_ARMS:
            animation = CONGA_ANIM_RAISE_ARMS;
            break;
        case CONGA_LOWER_ARMS:
            animation = CONGA_ANIM_RAISE_ARMS;
            progress = 1.0f - conga_progress;
            break;
        case CONGA_BEAT_CHEST:
            animation = CONGA_ANIM_BEAT_CHEST;
            break;
        case CONGA_THROW:
            animation = CONGA_ANIM_THROW;
            break;
        case CONGA_HIT:
        case CONGA_BEATEN:
            animation = CONGA_ANIM_OW;
            break;
        case CONGA_MOPING:
            animation = CONGA_ANIM_DEFEAT;
            break;
        default:
            animation = CONGA_ANIM_IDLE;
            break;
    }
    dl = conga_pose_draw(dl, CONGA_MODEL, conga_block, animation, progress, conga_pos, conga_yaw, conga_gfx[buffer],
                         CONGA_GFX_MAX, conga_mtx[buffer], CONGA_MTX_MAX);
    for (i = 0; i < ORANGE_MAX; i++) {
        if (oranges[i].live) {
            dl = conga_pose_draw(dl, ORANGE_MODEL, orange_block, -1, 0.0f, oranges[i].pos, 0.0f, orange_gfx[buffer][i],
                                 ORANGE_GFX_MAX, orange_mtx[buffer][i], ORANGE_MTX_MAX);
        }
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
Gfx *jiggy_conga_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (!talk_open || conga_state != CONGA_READY || cc_number_of_players != 1) {
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
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"CONGA", 1);
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
