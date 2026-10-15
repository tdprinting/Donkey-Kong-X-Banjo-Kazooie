// Boss Boom Box: Rusty Bucket Bay's boss, the crate that splits, read from the player's
// Banjo-Kazooie ROM, as the boss of Frantic Factory's Foyer, where the two worlds have collided.
//
// Like chBossBoomBox (src/RBB/ch/bossboombox.c), Boss Boom Box (ASSET_428_MODEL_BOSS_BOOM_BOX)
// waits shut in the middle of the Foyer; when the player comes near he bursts open
// (ASSET_146_ANIM_BOSS_BOOMBOX_APPEAR and its sounds) and, the first time, taunts them in a text
// box while the game's cutscene lock holds them. Then he hops at the player
// (ASSET_147_ANIM_BOOMBOX_MOVE, a boing as he takes off and a clunk as he lands), turning a
// quarter turn at a time toward them as he does; landing on the player hurts them through DK64's
// own path for an enemy's hit, setAction(0xE), as jiggy_grublins.c does, and he bounces back off
// them. A hit from the player (their punches, rolls, aerial attacks and the Simian Slam within
// reach, or one of their shots) makes him say ow (SFX_3F5, the death squeak and a hit thud) and
// hop back; enough hits break him (ASSET_148_ANIM_BOOMBOX_DIE, wood breaking) into two smaller
// boxes, which split again in turn: the biggest takes three hits, the two middle ones two each,
// and the four smallest one each, as chBossBoomBoxTable's sizes and health step down (scaled
// from Banjo-Kazooie's four sizes to three). Each box keeps to the Foyer, as
// func_8038CA70 keeps them in their room.
//
// When the last box breaks, he gives in in the text box and his Jiggy appears (a Golden Banana
// drawn as a Jiggy by jiggy_collectibles.c, reward 0x31 from jiggy_rewards.c, with
// COMUSIC_3D_JIGGY_SPAWN). The meeting and the defeat are kept with the DK64 save file
// (key 0x52000000 | map: Frantic Factory's characters, index 0), so a beaten Boss Boom Box stays
// broken; leaving the Foyer mid-fight (or losing a life) puts him back together for next time.
//
// The text box is Bottles' (jiggy_bottles.c): Banjo-Kazooie's Boss Boom Box portrait
// (ASSET_847_SPRITE_BOSS_BOOM_BOX, its mouth moving as the text types out) beside DK64's
// dialogue font, with his voice (Cheato's talking sounds, low, as gczoombox plays them).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his
// text box in the HUD pass (jiggy_boss_boom_box_hud_draw, patches_ui.c). Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

#define BOOM_BOX_MODEL 0x428    // ASSET_428_MODEL_BOSS_BOOM_BOX
#define BOOM_BOX_PORTRAIT 0x847 // ASSET_847_SPRITE_BOSS_BOOM_BOX
// The box converts to about 19.5 KiB, with 16 bones and about 12 pose commands.
#define BOOM_BOX_BLOCK_MAX 0x8000
#define BOOM_BOX_GFX_MAX 48
#define BOOM_BOX_MTX_MAX 20

// Banjo-Kazooie animations (chBossBoomBox) and how long each plays, in seconds.
#define BOOM_BOX_ANIM_APPEAR 0x146 // ASSET_146_ANIM_BOSS_BOOMBOX_APPEAR: bursting open
#define BOOM_BOX_ANIM_MOVE 0x147   // ASSET_147_ANIM_BOOMBOX_MOVE: a hop
#define BOOM_BOX_ANIM_DIE 0x148    // ASSET_148_ANIM_BOOMBOX_DIE: breaking up
#define BOOM_BOX_APPEAR_SECONDS 2.4f
#define BOOM_BOX_DIE_SECONDS 1.0f
// A hop moves him between these points through its animation (chBossBoomBox_update).
#define HOP_MOVE_FROM 0.1f
#define HOP_MOVE_TO 0.6f
#define HOP_TAKEOFF_PUFF_AT 0.1f
#define HOP_LAND_AT 0.6f
// Breaking, he slides back over this much of the animation.
#define DIE_SLIDE_TO 0.3f

// Banjo-Kazooie sounds (sfx_e) and how chBossBoomBox plays them.
#define BOOM_BOX_SFX_BOING 0x3F2   // SFX_3F2_BOING: each hop, 28000/32767, pitch the size's
#define BOOM_BOX_SFX_CLUNK 0x6C    // SFX_6C_LOCKUP_CLOSING: landing, 20000/32767, the size's pitch
                                   // +-0.05; opening, at 1.25 (1.05), 1.35 and 1.8 s, 0x7D00/32767
#define BOOM_BOX_SFX_OW 0x3F5      // SFX_3F5_UNKNOWN: opening (at 0.5 s, 0x7FC6/32767, 1.0); hit,
                                   // 20000/32767, the size's ow pitch
#define BOOM_BOX_SFX_THUD 0x1E     // SFX_1E_HITTING_AN_ENEMY_2: hit, 20000/32767, 1.0
#define BOOM_BOX_SFX_SQUEAK 0xD7   // SFX_D7_GRABBA_DEATH: hit, 0x7530/32767, the size's ow pitch
#define BOOM_BOX_SFX_BREAK 0xD9    // SFX_D9_WOODEN_CRATE_BREAKING_1: at 0.2 s into breaking,
                                   // 30000/32767, 0.9
// His voice in the text box (gczoombox: Cheato's talking sounds at 30000/32767, pitch 0.6).
static const u16 boom_box_talk_sfx[] = {0x149, 0x14A};
#define BOOM_BOX_TALK_SFX_EVERY 5 // Characters typed out between sounds.
// Banjo-Kazooie's Jiggy appearing (COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767).
#define JINGLE_JIGGY_SPAWN 0x3D
// Banjo-Kazooie fades these out between 500 and 1000 units (func_8030E878); scaled.
#define SFX_NEAR 125.0f
#define SFX_FAR 300.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// The sizes, biggest first (chBossBoomBoxTable, three of its four): the scale (Banjo-Kazooie's,
// before BK_TO_DK64), hits to break, how long a hop takes and how far it goes (DK64 units),
// how far back he hops when hit or after landing on the player, his body (DK64 units: the
// model is about 470 units across and 500 tall at scale 1), and the pitches of his boing
// and his ow.
typedef struct {
    f32 scale;
    s32 hits;
    f32 hop_seconds;
    f32 hop_distance;
    f32 push_distance;
    f32 radius;
    f32 height;
    f32 boing_pitch;
    f32 ow_pitch;
} BoomBoxSize;
static const BoomBoxSize boom_box_sizes[] = {
    {1.1f, 3, 1.0f, 75.0f, 34.0f, 58.0f, 130.0f, 0.8f, 0.9f},
    {0.75f, 2, 1.0f, 75.0f, 30.0f, 40.0f, 90.0f, 0.9f, 1.1f},
    {0.5f, 1, 0.5f, 38.0f, 26.0f, 27.0f, 60.0f, 1.0f, 1.3f},
};
#define BOOM_BOX_SIZES 3
// One box, then two, then four: the tree of boxes, each box i splitting into 2i+1 and 2i+2.
#define BOOM_BOX_MAX 7
#define BOOM_BOX_DRAWN_MAX 4 // At most this many at once (the four smallest).
// Where the two halves of a broken box land either side of it (200 units at its scale, scaled).
#define SPLIT_OFFSET (200.0f * BK_TO_DK64)

// When the player comes this close (on the ground) he opens up.
#define BOOM_BOX_WAKE_RADIUS 150.0f
#define BOOM_BOX_WAKE_HEIGHT 80.0f
// After a hit, he can't be hit again until his hop back is done and this long after.
#define BOOM_BOX_INVULNERABLE_SECONDS 0.3f
// The player's attacks reach this far past his side (the Simian Slam a little further), within
// this height above his base or below it; punches only hit what the player faces (within this
// many 12-bit angle units either side).
#define ATTACK_REACH 30.0f
#define SLAM_REACH 46.0f
#define ATTACK_BELOW 40.0f
#define PUNCH_ARC 0x300
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing him: the floor is looked for from this far above a spot, and only trusted this close.
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f

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

// The text box (as jiggy_bottles.c draws it), in screen pixels.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 8
#define PORTRAIT_MAX_TEXELS (32 * 32)
// His portrait is 64x64 in the ROM; it's read whole and halved to fit the text box.
#define PORTRAIT_READ_MAX (64 * 64)
// Frames 0 to 6 move his mouth (gczoombox's 7 talking frames for Boss Boom Box).
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

// Where he is, map by map. Frantic Factory's Foyer, where the player comes in from the lobby
// (at about (1263, 827, 2810)): he waits a little north of its middle, clear of the two raised
// objects either side (1153, 2503) and (1373, 2504), and every box keeps to the Foyer's floor
// (x 1025 to 1475, z 2350 to 2675, all at 831) less a margin. His Jiggy appears in the middle of
// the Foyer's south half, on the entrance's line.
typedef struct {
    Maps map;
    f32 x, y, z;                    // Where he waits (y is the floor's, if the floor isn't found).
    f32 min_x, max_x, min_z, max_z; // His boxes' middles never leave this.
    f32 reward_x, reward_z;         // Where his Jiggy appears.
} BoomBoxPlacement;
static const BoomBoxPlacement boom_box_spots[] = {
    {MAP_FACTORY, 1263.0f, 831.0f, 2445.0f, 1045.0f, 1455.0f, 2370.0f, 2655.0f, 1263.0f, 2560.0f},
};
// Reward ids (docs/GAME_PLAN.md: Frantic Factory's are 0x30 to 0x3F).
#define BOOM_BOX_REWARD 0x31
#define REWARD_HEIGHT 12.5f
#define REWARD_CLEARANCE 30.0f

// What he says. Each page is one press of A; lines are broken to fit the box when shown.
static const char *const boom_box_meet[] = {
    "OI! I'M BOSS BOOM BOX, AND THIS FACTORY'S MY WAREHOUSE NOW!",
    "WANT MY JIGGY? BREAK ME, AND ALL ME LITTLE BOXES TOO! HAR HAR!",
};
static const char *const boom_box_defeat[] = {
    "ARGH! ME LOVELY BOXES, ALL MATCHWOOD! FINE, TAKE YOUR JIGGY!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): BOOM_BOX_SAVE_* bits. Frantic Factory's characters are 0x52,
// with the character's index (Boss Boom Box is 0) in bits 16 to 23.
#define BOOM_BOX_SAVE_KEY(map) (0x52000000 | (0u << 16) | (u32)(map))
#define BOOM_BOX_SAVE_MET 0x1
#define BOOM_BOX_SAVE_DEFEATED 0x2

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
} BoomBoxActiveActor;
extern BoomBoxActiveActor D_global_asm_807FB930[];
extern u16 D_global_asm_807FBB34; // How many.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y, leaving it as it was if there's none.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// Whether the player can be hurt now (not invulnerable, not already knocked down).
s32 func_global_asm_806EB400(Actor *player);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
s32 jiggy_banjo_air_attack(Actor *player);

typedef enum {
    BOOM_BOX_UNLOADED,
    BOOM_BOX_READY,
    BOOM_BOX_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model or the portrait didn't convert.
} BoomBoxLoadState;

typedef enum {
    BOX_GONE,     // Not out (yet), or broken.
    BOX_WAITING,  // Shut, until the player comes near (the biggest only).
    BOX_OPENING,  // Bursting open.
    BOX_HOPPING,  // At the player.
    BOX_PUSHED,   // Hopping back after a hit, or after landing on the player.
    BOX_BREAKING,
} BoxAction;

typedef struct {
    BoxAction action;
    s32 size; // Into boom_box_sizes.
    s32 hits;
    f32 pos[3];
    f32 from[3], to[3]; // This hop's start and end.
    f32 yaw, from_yaw, to_yaw; // 12-bit angle units.
    f32 progress;  // 0 to 1 through the animation.
    f32 invulnerable; // Seconds left.
    s32 hit_player;   // Landed on the player this hop.
} Box;

typedef enum {
    TALK_MEET,
    TALK_DEFEAT,
} BoomBoxTalk;

static u8 boom_box_block[BOOM_BOX_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx boom_box_gfx[2][BOOM_BOX_DRAWN_MAX][BOOM_BOX_GFX_MAX];
static Mtx boom_box_mtx[2][BOOM_BOX_DRAWN_MAX][BOOM_BOX_MTX_MAX];
static JiggyBkPose boom_box_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static u16 portrait_read[PORTRAIT_READ_MAX] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static BoomBoxLoadState boom_box_state = BOOM_BOX_UNLOADED;

static s32 boom_box_last_map = -1;
static u32 boom_box_last_timer;
static const BoomBoxPlacement *boom_box_spot; // This map's, or NULL.
static s32 boom_box_placed;
static f32 boom_box_floor; // The Foyer's floor height.
static Box boxes[BOOM_BOX_MAX];
static s32 boxes_broken_smallest; // Smallest boxes broken this fight.
static u32 boom_box_saved; // BOOM_BOX_SAVE_* bits.
static s32 boom_box_cutscene; // Holding the player while he opens up the first time.
static s32 reward_due;
static s32 reward_jingle;
static u32 boom_box_random_state = 0x424F4F4D;

// The dialogue (as jiggy_bottles.c keeps it).
static BoomBoxTalk talk_kind;
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

static f32 boom_box_random(void) {
    boom_box_random_state = boom_box_random_state * 1664525 + 1013904223;
    return (boom_box_random_state >> 8) * (1.0f / 16777216.0f);
}

static f32 boom_box_random_between(f32 low, f32 high) {
    return low + boom_box_random() * (high - low);
}

static void boom_box_load(void) {
    s32 size;
    s32 count;
    s32 i;

    boom_box_state = BOOM_BOX_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(BOOM_BOX_MODEL, boom_box_block, sizeof(boom_box_block));
    if (size <= 0 || size > (s32)sizeof(boom_box_block)) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(BOOM_BOX_PORTRAIT);
    if (count <= 0) {
        return;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES && portrait_frames < PORTRAIT_MAX_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(BOOM_BOX_PORTRAIT, i, portrait_read, sizeof(portrait_read));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;
        s32 step = 1;
        s32 x, y;
        u16 *out = portrait_texels[portrait_frames];

        if (got <= 0 || width == 0 || height == 0 || width * height > PORTRAIT_READ_MAX) {
            continue;
        }
        while ((width / step) * (height / step) > PORTRAIT_MAX_TEXELS) {
            step++;
        }
        for (y = 0; y < height / step; y++) {
            for (x = 0; x < width / step; x++) {
                out[y * (width / step) + x] = portrait_read[(y * step) * width + x * step];
            }
        }
        portrait_width[portrait_frames] = width / step;
        portrait_height[portrait_frames] = height / step;
        portrait_frames++;
    }
    if (portrait_frames == 0) {
        return;
    }
    boom_box_state = BOOM_BOX_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// A Banjo-Kazooie sound from a box, fading out with distance from the player.
static void box_sfx(Box *b, Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(b->pos[0], b->pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

// True when `at` is passed going from `from` to `to` through an animation.
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

// Keeps a box's middle inside the room, less its own size (func_8038CA70).
static void box_clamp(Box *b, f32 *pos) {
    f32 r = boom_box_sizes[b->size].radius;

    if (pos[0] < boom_box_spot->min_x + r) {
        pos[0] = boom_box_spot->min_x + r;
    }
    if (pos[0] > boom_box_spot->max_x - r) {
        pos[0] = boom_box_spot->max_x - r;
    }
    if (pos[2] < boom_box_spot->min_z + r) {
        pos[2] = boom_box_spot->min_z + r;
    }
    if (pos[2] > boom_box_spot->max_z - r) {
        pos[2] = boom_box_spot->max_z - r;
    }
}

// Starts a hop: at the player (`toward`), or `distance` away from them (`toward` FALSE). He
// turns a quarter turn toward the player when they're off to his side (chBossBoomBox_setState).
static void box_hop(Box *b, Actor *player, s32 toward, f32 distance) {
    s16 to_player = func_global_asm_80665DE0(player->x_position, player->z_position, b->pos[0], b->pos[2]) & 0xFFF;
    f32 off;

    b->from[0] = b->pos[0];
    b->from[1] = b->pos[1];
    b->from[2] = b->pos[2];
    b->to[0] = b->pos[0] + func_global_asm_80612794(to_player) * (toward ? distance : -distance);
    b->to[2] = b->pos[2] + func_global_asm_80612790(to_player) * (toward ? distance : -distance);
    box_clamp(b, b->to);
    b->to[1] = floor_under(b->to[0], boom_box_floor, b->to[2]);
    b->from_yaw = b->yaw;
    b->to_yaw = b->yaw;
    if (toward) {
        off = to_player - b->yaw;
        while (off > 2048.0f) {
            off -= 4096.0f;
        }
        while (off < -2048.0f) {
            off += 4096.0f;
        }
        if (off > 0x200) {
            b->to_yaw = b->yaw + 1024.0f;
        } else if (off < -0x200) {
            b->to_yaw = b->yaw - 1024.0f;
        }
    }
    b->action = toward ? BOX_HOPPING : BOX_PUSHED;
    b->progress = 0.0f;
    b->hit_player = FALSE;
    box_sfx(b, player, BOOM_BOX_SFX_BOING, 28000.0f / 32767.0f, boom_box_sizes[b->size].boing_pitch);
}

// Puts the fight back to its start: the biggest box, shut where he waits.
static void boxes_reset(Actor *player) {
    s32 i;

    for (i = 0; i < BOOM_BOX_MAX; i++) {
        boxes[i].action = BOX_GONE;
    }
    boxes_broken_smallest = 0;
    reward_due = FALSE;
    reward_jingle = FALSE;
    if (boom_box_saved & BOOM_BOX_SAVE_DEFEATED) {
        // Beaten: his Jiggy's due until it's collected.
        reward_due = !jiggy_reward_collected(BOOM_BOX_REWARD);
        return;
    }
    boxes[0].action = BOX_WAITING;
    boxes[0].size = 0;
    boxes[0].hits = 0;
    boxes[0].pos[0] = boom_box_spot->x;
    boxes[0].pos[1] = boom_box_floor;
    boxes[0].pos[2] = boom_box_spot->z;
    // Facing the way the player comes in, a quarter turn at a time from there on.
    boxes[0].yaw = 0.0f;
    boxes[0].progress = 0.0f;
    boxes[0].invulnerable = 0.0f;
}

static void boom_box_place(Actor *player) {
    boom_box_floor = floor_under(boom_box_spot->x, boom_box_spot->y, boom_box_spot->z);
    boom_box_cutscene = FALSE;
    boxes_reset(player);
    boom_box_placed = TRUE;
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

// The player can't move while he opens up and talks (as jiggy_bottles.c holds them).
static void boom_box_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void talk_start(BoomBoxTalk kind) {
    talk_kind = kind;
    if (kind == TALK_MEET) {
        talk_pages = boom_box_meet;
        talk_page_count = ARRAY_COUNT_OF(boom_box_meet);
    } else {
        talk_pages = boom_box_defeat;
        talk_page_count = ARRAY_COUNT_OF(boom_box_defeat);
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    boom_box_lock_player(TRUE);
}

static void boom_box_save(u32 bits) {
    boom_box_saved |= bits;
    jiggy_save_set(current_file, BOOM_BOX_SAVE_KEY(current_map), boom_box_saved);
}

// The text box's frame: A to type out the rest of the page or go on to the next, and the
// talking sounds while it types.
static void talk_update(Actor *player) {
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
        if (talk_shown / BOOM_BOX_TALK_SFX_EVERY != before / BOOM_BOX_TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 pick = (u32)(boom_box_random() * ARRAY_COUNT_OF(boom_box_talk_sfx)) % ARRAY_COUNT_OF(boom_box_talk_sfx);

            jiggy_bk_sfx_play(boom_box_talk_sfx[pick], 30000.0f / 32767.0f, 0.6f);
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
    boom_box_lock_player(FALSE);
    if (talk_kind == TALK_MEET) {
        boom_box_save(BOOM_BOX_SAVE_MET);
        // The fight's on.
        box_hop(&boxes[0], player, TRUE, boom_box_sizes[0].hop_distance);
    } else {
        // His Jiggy appears, with Banjo-Kazooie's Jiggy jingle.
        reward_due = TRUE;
        reward_jingle = TRUE;
    }
}

// Whether the player's attack hits a box this frame (as jiggy_grublins.c checks, for its size).
static s32 box_attacked(Box *b, Actor *player) {
    const BoomBoxSize *size = &boom_box_sizes[b->size];
    f32 distance = distance_xz(b->pos[0], b->pos[2], player->x_position, player->z_position) - size->radius;
    f32 dy = player->y_position - b->pos[1];
    s32 to_box;
    s32 off;

    if (dy > size->height || dy < -ATTACK_BELOW) {
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
            // Up against him, a punch always lands; further out, only in front of the player.
            if (distance <= 0.0f) {
                return TRUE;
            }
            to_box = func_global_asm_80665DE0(b->pos[0], b->pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_box - (player->y_rotation & 0xFFF)) & 0xFFF;
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

// Whether one of the player's own shots (a gun's projectile, or one of Banjo's eggs) is in a box.
static s32 box_shot(Box *b, Actor *player) {
    const BoomBoxSize *size = &boom_box_sizes[b->size];
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
        dy = shot->y_position - b->pos[1];
        if (dy >= 0.0f && dy <= size->height &&
            distance_xz(b->pos[0], b->pos[2], shot->x_position, shot->z_position) <= size->radius) {
            return TRUE;
        }
    }
    return FALSE;
}

// Hit (func_8038D7E8): ow and hop back, or break.
static void box_hurt(Box *b, Actor *player) {
    const BoomBoxSize *size = &boom_box_sizes[b->size];

    box_sfx(b, player, BOOM_BOX_SFX_THUD, 20000.0f / 32767.0f, 1.0f);
    box_sfx(b, player, BOOM_BOX_SFX_SQUEAK, 0x7530 / 32767.0f, size->ow_pitch);
    b->hits++;
    if (b->hits >= size->hits) {
        s16 to_player = func_global_asm_80665DE0(player->x_position, player->z_position, b->pos[0], b->pos[2]) & 0xFFF;

        b->action = BOX_BREAKING;
        b->progress = 0.0f;
        b->from[0] = b->pos[0];
        b->from[1] = b->pos[1];
        b->from[2] = b->pos[2];
        // He slides back from the blow as he breaks (300 units, scaled).
        b->to[0] = b->pos[0] - func_global_asm_80612794(to_player) * 300.0f * BK_TO_DK64 * size->scale;
        b->to[2] = b->pos[2] - func_global_asm_80612790(to_player) * 300.0f * BK_TO_DK64 * size->scale;
        box_clamp(b, b->to);
        b->to[1] = floor_under(b->to[0], boom_box_floor, b->to[2]);
        return;
    }
    box_sfx(b, player, BOOM_BOX_SFX_OW, 20000.0f / 32767.0f, size->ow_pitch);
    box_hop(b, player, FALSE, size->push_distance);
    b->invulnerable = BOOM_BOX_INVULNERABLE_SECONDS;
}

// A box broken: two smaller ones jump out either side of it, or, the smallest, it's gone; the
// last one ends the fight.
static void box_broken(s32 index, Actor *player) {
    Box *b = &boxes[index];
    s32 child;
    s32 side;

    b->action = BOX_GONE;
    if (b->size + 1 >= BOOM_BOX_SIZES) {
        boxes_broken_smallest++;
        if (boxes_broken_smallest >= (1 << (BOOM_BOX_SIZES - 1))) {
            boom_box_save(BOOM_BOX_SAVE_DEFEATED);
            talk_start(TALK_DEFEAT);
        }
        return;
    }
    for (side = 0; side < 2; side++) {
        Box *c;
        f32 offset = SPLIT_OFFSET * boom_box_sizes[b->size].scale;

        child = 2 * index + 1 + side;
        if (child >= BOOM_BOX_MAX) {
            continue;
        }
        c = &boxes[child];
        c->size = b->size + 1;
        c->hits = 0;
        c->pos[0] = b->pos[0] + (side == 0 ? offset : -offset);
        c->pos[2] = b->pos[2];
        box_clamp(c, c->pos);
        c->pos[1] = floor_under(c->pos[0], boom_box_floor, c->pos[2]);
        c->yaw = b->yaw;
        c->invulnerable = BOOM_BOX_INVULNERABLE_SECONDS;
        // They come out with a hop of their own (chBossBoomBox's state 7), then go for the player.
        box_hop(c, player, TRUE, 0.0f);
    }
}

// Hurts the player through DK64's own enemy hit (as jiggy_grublins.c does), knocking them away
// from the box.
static void box_hit_player(Box *b, Actor *player) {
    player->unkEE = func_global_asm_80665DE0(b->pos[0], b->pos[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
}

// A frame of one box (chBossBoomBox_update), `frames` game frames on.
static void box_update(s32 index, Actor *player, u32 frames) {
    Box *b = &boxes[index];
    const BoomBoxSize *size = &boom_box_sizes[b->size];
    f32 seconds = frames / GAME_FPS;
    f32 before = b->progress;
    f32 t;
    f32 dy;

    if (frames == 0) {
        return;
    }
    if (b->action == BOX_HOPPING || b->action == BOX_PUSHED) {
        if (b->invulnerable > 0.0f) {
            if (b->action != BOX_PUSHED) {
                b->invulnerable -= seconds;
            }
        } else if (!talk_open && (box_attacked(b, player) || box_shot(b, player))) {
            box_hurt(b, player);
            return;
        }
    }
    switch (b->action) {
        case BOX_WAITING:
            dy = player->y_position - b->pos[1];
            if (distance_xz(b->pos[0], b->pos[2], player->x_position, player->z_position) < BOOM_BOX_WAKE_RADIUS &&
                dy < BOOM_BOX_WAKE_HEIGHT && dy > -BOOM_BOX_WAKE_HEIGHT &&
                player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT) {
                b->action = BOX_OPENING;
                b->progress = 0.0f;
                // The first time, it's a cutscene: the player's held while he opens and talks.
                boom_box_cutscene = !(boom_box_saved & BOOM_BOX_SAVE_MET);
                if (boom_box_cutscene) {
                    boom_box_lock_player(TRUE);
                }
            }
            break;
        case BOX_OPENING:
            b->progress += seconds / BOOM_BOX_APPEAR_SECONDS;
            if (passed(before * BOOM_BOX_APPEAR_SECONDS, b->progress * BOOM_BOX_APPEAR_SECONDS, 0.5f)) {
                jiggy_bk_sfx_play(BOOM_BOX_SFX_OW, 0x7FC6 / 32767.0f, 1.0f);
            }
            if (passed(before * BOOM_BOX_APPEAR_SECONDS, b->progress * BOOM_BOX_APPEAR_SECONDS, 1.25f)) {
                jiggy_bk_sfx_play(BOOM_BOX_SFX_CLUNK, 0x7D00 / 32767.0f, 1.05f);
            }
            if (passed(before * BOOM_BOX_APPEAR_SECONDS, b->progress * BOOM_BOX_APPEAR_SECONDS, 1.35f) ||
                passed(before * BOOM_BOX_APPEAR_SECONDS, b->progress * BOOM_BOX_APPEAR_SECONDS, 1.8f)) {
                jiggy_bk_sfx_play(BOOM_BOX_SFX_CLUNK, 0x7D00 / 32767.0f, 1.0f);
            }
            if (b->progress >= 1.0f) {
                b->progress = 0.9999f;
                if (boom_box_cutscene) {
                    boom_box_cutscene = FALSE;
                    talk_start(TALK_MEET);
                    // Waits open until the talk's done (talk_update starts his first hop).
                    b->action = BOX_PUSHED;
                    b->from[0] = b->to[0] = b->pos[0];
                    b->from[1] = b->to[1] = b->pos[1];
                    b->from[2] = b->to[2] = b->pos[2];
                    b->from_yaw = b->to_yaw = b->yaw;
                    b->progress = 0.0f;
                    b->invulnerable = 1.0f;
                } else {
                    box_hop(b, player, TRUE, size->hop_distance);
                }
            }
            break;
        case BOX_HOPPING:
        case BOX_PUSHED:
            if (talk_open) {
                // Still while he talks.
                break;
            }
            b->progress += seconds / size->hop_seconds;
            if (passed(before, b->progress, HOP_LAND_AT)) {
                box_sfx(b, player, BOOM_BOX_SFX_CLUNK, 20000.0f / 32767.0f,
                        size->boing_pitch + boom_box_random_between(-0.05f, 0.05f));
            }
            t = (b->progress - HOP_MOVE_FROM) / (HOP_MOVE_TO - HOP_MOVE_FROM);
            if (t < 0.0f) {
                t = 0.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
            }
            b->pos[0] = b->from[0] + (b->to[0] - b->from[0]) * t;
            b->pos[1] = b->from[1] + (b->to[1] - b->from[1]) * t;
            b->pos[2] = b->from[2] + (b->to[2] - b->from[2]) * t;
            b->yaw = b->from_yaw + (b->to_yaw - b->from_yaw) * t;
            if (b->yaw < 0.0f) {
                b->yaw += 4096.0f;
            } else if (b->yaw >= 4096.0f) {
                b->yaw -= 4096.0f;
            }
            // Coming down on the player (or the player running into him) hurts them; he bounces
            // back off them after.
            dy = player->y_position - b->pos[1];
            if (b->action == BOX_HOPPING && !b->hit_player && b->progress >= HOP_MOVE_FROM &&
                distance_xz(b->pos[0], b->pos[2], player->x_position, player->z_position) <= size->radius &&
                dy <= size->height && dy >= -ATTACK_BELOW && func_global_asm_806EB400(player)) {
                box_hit_player(b, player);
                b->hit_player = TRUE;
            }
            if (b->progress >= 1.0f) {
                if (b->action == BOX_PUSHED) {
                    b->invulnerable = BOOM_BOX_INVULNERABLE_SECONDS;
                }
                if (b->hit_player) {
                    box_hop(b, player, FALSE, size->push_distance);
                } else {
                    box_hop(b, player, TRUE, size->hop_distance * boom_box_random_between(1.0f, 1.1f));
                }
            }
            break;
        case BOX_BREAKING:
            b->progress += seconds / BOOM_BOX_DIE_SECONDS;
            if (passed(before * BOOM_BOX_DIE_SECONDS, b->progress * BOOM_BOX_DIE_SECONDS, 0.2f)) {
                box_sfx(b, player, BOOM_BOX_SFX_BREAK, 30000.0f / 32767.0f, 0.9f);
            }
            t = b->progress / DIE_SLIDE_TO;
            if (t > 1.0f) {
                t = 1.0f;
            }
            b->pos[0] = b->from[0] + (b->to[0] - b->from[0]) * t;
            b->pos[1] = b->from[1] + (b->to[1] - b->from[1]) * t;
            b->pos[2] = b->from[2] + (b->to[2] - b->from[2]) * t;
            if (b->progress >= 1.0f) {
                box_broken(index, player);
            }
            break;
        default:
            break;
    }
}

// Draws a box at its spot and size, its look picked as chBossBoomBox_draw picks it.
static Gfx *box_draw(Gfx *dl, Box *b, s32 animation, f32 progress, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 s = BK_TO_DK64 * boom_box_sizes[b->size].scale;
    s32 face = b->action == BOX_PUSHED && !talk_open ? 2 : 1;
    s32 i;

    guScaleF(scale, s, s, s);
    guRotateF(rotate, b->yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, boom_box_pose.world);
    boom_box_pose.world[3][0] = b->pos[0];
    boom_box_pose.world[3][1] = b->pos[1];
    boom_box_pose.world[3][2] = b->pos[2];
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        boom_box_pose.selectors[i] = 0;
    }
    // The biggest is drawn as the boss (selector 1's first branch), the rest as plain boxes; the
    // face (selectors 2, 4 and 5, or 3, 6 and 7) winces while he's knocked back.
    if (b->size == 0) {
        boom_box_pose.selectors[1] = 1;
        boom_box_pose.selectors[2] = face;
        boom_box_pose.selectors[4] = face;
        boom_box_pose.selectors[5] = face;
    } else {
        boom_box_pose.selectors[1] = 2;
        boom_box_pose.selectors[3] = face;
        boom_box_pose.selectors[6] = face;
        boom_box_pose.selectors[7] = face;
    }
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    boom_box_pose.model = BOOM_BOX_MODEL;
    boom_box_pose.block = boom_box_block;
    boom_box_pose.animation = animation;
    boom_box_pose.progress = progress;
    boom_box_pose.gfx = gfx;
    boom_box_pose.gfx_capacity = BOOM_BOX_GFX_MAX;
    boom_box_pose.mtx = mtx;
    boom_box_pose.mtx_capacity = BOOM_BOX_MTX_MAX;
    if (jiggy_bk_model_pose(&boom_box_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Boss Boom Box's boxes once a frame, with the player.
Gfx *jiggy_boss_boom_box_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    s32 drawn = 0;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): the fight starts over.
    if ((s32)current_map != boom_box_last_map || object_timer < boom_box_last_timer) {
        // Leaving the map mid-talk or mid-cutscene lets the player go.
        if (talk_open || boom_box_cutscene) {
            boom_box_lock_player(FALSE);
            talk_open = FALSE;
            boom_box_cutscene = FALSE;
        }
        boom_box_last_map = current_map;
        boom_box_placed = FALSE;
        boom_box_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(boom_box_spots); i++) {
            if (boom_box_spots[i].map == current_map) {
                boom_box_spot = &boom_box_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        boom_box_saved = boom_box_spot != NULL ? jiggy_save_get(current_file, BOOM_BOX_SAVE_KEY(current_map)) : 0;
    }
    if (boom_box_spot == NULL) {
        boom_box_last_timer = object_timer;
        return dl;
    }
    if (boom_box_state == BOOM_BOX_UNLOADED) {
        boom_box_load();
    }
    if (boom_box_state != BOOM_BOX_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!boom_box_placed) {
        boom_box_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        boom_box_place(actor);
    }
    frames = object_timer - boom_box_last_timer;
    boom_box_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && !boom_box_cutscene) {
        frames = 0;
    }
    for (i = 0; i < BOOM_BOX_MAX; i++) {
        if (boxes[i].action != BOX_GONE) {
            box_update(i, actor, frames);
        }
    }
    if (talk_open) {
        talk_update(actor);
    }
    // The first time, his Jiggy waits for the player to step off its spot, so the jingle plays
    // as it appears.
    if (reward_due && frames > 0 && !talk_open &&
        (!reward_jingle || distance_xz(actor->x_position, actor->z_position, boom_box_spot->reward_x,
                                       boom_box_spot->reward_z) >= REWARD_CLEARANCE)) {
        f32 y = floor_under(boom_box_spot->reward_x, boom_box_floor, boom_box_spot->reward_z);

        if (jiggy_reward_update(BOOM_BOX_REWARD, boom_box_spot->reward_x, y + REWARD_HEIGHT, boom_box_spot->reward_z,
                                actor, frames)) {
            reward_due = FALSE;
        } else if (reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
            reward_jingle = FALSE;
        }
    }

    for (i = 0; i < BOOM_BOX_MAX && drawn < BOOM_BOX_DRAWN_MAX; i++) {
        Box *b = &boxes[i];
        s32 animation;
        f32 progress = b->progress;

        switch (b->action) {
            case BOX_WAITING:
            case BOX_OPENING:
                animation = BOOM_BOX_ANIM_APPEAR;
                break;
            case BOX_HOPPING:
            case BOX_PUSHED:
                animation = BOOM_BOX_ANIM_MOVE;
                // Standing open while he taunts the player.
                if (talk_open && talk_kind == TALK_MEET) {
                    animation = BOOM_BOX_ANIM_APPEAR;
                    progress = 1.0f;
                }
                break;
            case BOX_BREAKING:
                animation = BOOM_BOX_ANIM_DIE;
                break;
            default:
                continue;
        }
        dl = box_draw(dl, b, animation, progress, boom_box_gfx[buffer][drawn], boom_box_mtx[buffer][drawn]);
        drawn++;
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
Gfx *jiggy_boss_boom_box_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (!talk_open || boom_box_state != BOOM_BOX_READY || cc_number_of_players != 1) {
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
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"BOSS BOOM BOX", 1);
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
