// Jinjos: Banjo-Kazooie's five Jinjos, read from the player's Banjo-Kazooie ROM, as a
// collectible of their own beside DK64's colored bananas.
//
// Each color is its own model in Banjo-Kazooie (src/core2/ch/jinjo.c: chJinjoBlue and the
// rest): the same mesh and skeleton, with the color in the vertex colors and the textures'
// palettes. They share one set of animations, and behave like chJinjo_update: stand still,
// now and then wave (the idle animation, played once) or wave both hands for help when the
// player looks at them, and hop round to face the player when they've walked off to the
// side. Their eyes blink through the model's eye selector, as actors spawned with
// ACTOR_FLAG_UNKNOWN_8 do (func_803255FC).
//
// For now the Jinjos only exist in the Training Grounds and Jungle Japes (jinjo_maps below).
// They are drawn right after the player's own model (func_global_asm_80630DCC in
// patches_draw.c).
//
// Touching a Jinjo collects it: it disappears, a pickup sound plays, and Banjo-Kazooie's
// Jinjo counter (fxjinjoscore in src/core2/fx/jinjoscore.c: a row of five Jinjo heads,
// ASSET_7DF, recolored per Jinjo, the collected ones turning to face front) comes up for a
// moment in the HUD pass (jiggy_jinjos_hud_draw, called from patches_ui.c).
// The fifth Jinjo of a map gives a Jiggy (jiggy_jinjo_jiggy.c).
// TODO: the collected Jinjo's fly-away (ASSET_130/131 animations, sparkles) and Banjo-Kazooie's
// own sounds. Collected Jinjos are kept with the DK64 save file (jiggy_save_get/set): a bit per
// Jinjo per map, as Banjo-Kazooie's ITEM_12_JINJOS keeps 5 bits a level.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

// The models, in Banjo-Kazooie's Jinjo order (its markers, MARKER_5A_JINJO_BLUE to
// MARKER_5E_JINJO_YELLOW, and the HUD's heads left to right).
#define JINJO_COLORS 5
static const u16 jinjo_models[JINJO_COLORS] = {
    0x3C0, // ASSET_3C0_MODEL_JINJO_BLUE
    0x3C2, // ASSET_3C2_MODEL_JINJO_GREEN
    0x3BC, // ASSET_3BC_MODEL_JINJO_ORANGE
    0x3C1, // ASSET_3C1_MODEL_JINJO_PINK
    0x3BB, // ASSET_3BB_MODEL_JINJO_YELLOW
};
// A converted Jinjo is about 10 KiB; its pose is under 70 commands and loads a root matrix
// and 31 bone matrices.
#define JINJO_BLOCK_MAX 0x4000
#define JINJO_GFX_MAX 128
#define JINJO_MTX_MAX 40

// Banjo-Kazooie animation assets (chJinjoAnimations) and how long each plays, in seconds.
#define JINJO_ANIM_IDLE 0x2D  // ASSET_2D_ANIM_JINJO_IDLE: held at its start, or one wave.
#define JINJO_ANIM_HELP 0x2F  // ASSET_2F_ANIM_JINJO_HELP: both hands.
#define JINJO_ANIM_JUMP 0x31  // ASSET_31_ANIM_JINJO_JUMP: the hop that turns it round.
#define JINJO_WAVE_SECONDS 1.5f
#define JINJO_JUMP_SECONDS 0.75f
// Chance a frame that a standing Jinjo waves, and how close (in degrees either side) the
// player has to be looking at it for a wave to be the two-handed one.
#define JINJO_WAVE_CHANCE 0.015f
#define JINJO_LOOKED_AT_DEGREES 55.0f
// A Jinjo more than this far (degrees) from facing the player hops round, turning this fast.
#define JINJO_FACE_DEGREES 22.5f
#define JINJO_TURN_DEGREES_PER_SECOND 80.0f
// The eye selector: 1 is open, 4 closed. A blink starts with this chance a frame and steps
// one eye a frame, closed and open again.
#define SEL_EYES 1
#define JINJO_EYES_OPEN 1
#define JINJO_EYES_CLOSED 4
#define JINJO_BLINK_CHANCE 0.03f
// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo: a Jinjo stands 104
// units tall in Banjo-Kazooie, 26 here.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
#define JINJO_AIRBORNE_HEIGHT 6.0f
// Placing a Jinjo on the floor under its spot: looked for from this far above the spot, and
// only trusted this close to the spot's height.
#define JINJO_FLOOR_PROBE 20.0f
#define JINJO_FLOOR_MAX_STEP 40.0f
// How close the player has to come to collect a Jinjo: across, and how far the player's feet
// can be above the Jinjo's (about its height) or below them (about the player's).
#define JINJO_TOUCH_RADIUS 26.0f
#define JINJO_TOUCH_FEET_ABOVE 30.0f
#define JINJO_TOUCH_FEET_BELOW 40.0f
// DK64's pickup jingle (as jiggy_notes.c plays for a note) until Banjo-Kazooie's sounds can
// be played. Banjo-Kazooie plays COMUSIC_A_JINJO_COLLECTED here (COMUSIC_30_5TH_JINJO_COLLECTED
// for the fifth), SFX_17_JINJO_WHISTLE as a wave starts, SFX_27_JINJO_HI as a help wave
// starts, and SFX_8_BANJO_LANDING_04 as a hop lands: they hook in at jinjo_play_sound.
#define JINJO_PICKUP_SOUND 0x331

// The HUD counter: Banjo-Kazooie's Jinjo head (13 frames of a CI4 sprite: a turn from the
// side to the front, then a wink), one per Jinjo.
#define JIGGY_HUD_JINJO 0x7DF
#define HUD_JINJO_FRAMES 13
#define HUD_JINJO_MAX_TEXELS (32 * 32)
#define HUD_JINJO_FRONT_FRAME 9
#define HUD_JINJO_LAST_FRAME 12
// Frames a second of the turn and of the wink, and the chance a frame that a collected head
// winks (fxjinjoscore_update).
#define HUD_JINJO_TURN_FPS 15.0f
#define HUD_JINJO_WINK_FPS 30.0f
#define HUD_JINJO_WINK_CHANCE 0.05f
// Uncollected heads are see-through, collected ones solid over a drop shadow 2 pixels down
// and right (fxjinjoscore_draw).
#define HUD_JINJO_MISSING_ALPHA 0x6E
#define HUD_JINJO_SHADOW_ALPHA 0x8C
#define HUD_JINJO_SHADOW_OFFSET 2
// The heads rise from this far below, each once the one before is nearly up, closing 40% of
// the way a frame.
#define HUD_JINJO_RISE 64.0f
#define HUD_JINJO_RISE_NEXT 15.0f
#define HUD_JINJO_RISE_KEEP 0.6f
// Ticks are 1/60 s (the engine's lag counter, as in jiggy_notes.c).
#define HUD_SHOW_TICKS 180
#define HUD_FADE_TICKS 20
// Where the row sits: bottom left, over jiggy_notes.c's note counter (HUD_X 56, a 32-pixel
// note 36 pixels off the bottom), with a gap between. Heads are 32 pixels apart, as in
// Banjo-Kazooie.
#define HUD_X 56
#define HUD_BOTTOM_MARGIN (36 + 32 + 6)
#define HUD_JINJO_SPACING 32

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u32 D_global_asm_80744478;
extern s16 D_global_asm_80744494;
s16 playSound(s16 arg0, s32 arg1, f32 arg2, f32 arg3, u8 arg4, u8 arg5);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
// The 12-bit angle (y_rotation) that faces from (x, z) toward (target_x, target_z).
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z);
// The floor under (x, z), as the game looks for one (func_global_asm_80667180): writes its
// height to *y, leaving it as it was if there's none. The return value isn't reliable.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// The fifth Jinjo's Jiggy (jiggy_jinjo_jiggy.c).
s32 jiggy_jinjo_jiggy_earn(f32 pos[3], s32 fanfare);
void jiggy_jinjo_jiggy_update(Actor *player);

// Where the Jinjos go, map by map, each spot found by walking there so all five can be
// reached on foot. Heights are the ground's; each is snapped to the floor found just above it
// when placed.
typedef struct {
    f32 x, y, z;
} JinjoPlacement;

// The Training Grounds: on the grass around the treehouse (the player starts on its deck, at
// about (1346, 227, 1098)).
static const JinjoPlacement jinjos_training_grounds[JINJO_COLORS] = {
    {1128.0f, 28.0f, 1053.0f}, // Blue
    {1165.0f, 34.0f, 851.0f},  // Green
    {1252.0f, 24.0f, 814.0f},  // Orange
    {1408.0f, 34.0f, 893.0f},  // Pink
    {1619.0f, 37.0f, 1037.0f}, // Yellow
};
// Jungle Japes: around the first clearing past the entrance path.
static const JinjoPlacement jinjos_japes[JINJO_COLORS] = {
    {1108.0f, 280.0f, 199.0f}, // Blue
    {1256.0f, 288.0f, 152.0f}, // Green
    {1460.0f, 280.0f, 214.0f}, // Orange
    {1180.0f, 283.0f, 172.0f}, // Pink
    {1000.0f, 280.0f, 400.0f}, // Yellow
};
// Gloomy Galleon: one each by the entrance, on the east side, on the lighthouse's island and
// by Cranky's (two).
static const JinjoPlacement jinjos_galleon[JINJO_COLORS] = {
    {2450.0f, 1610.0f, 3330.0f}, // Blue: by the entrance, inside the bananas' loop.
    {3000.0f, 1670.0f, 3600.0f}, // Green: the east side, west of Nipper.
    {1800.0f, 1460.0f, 500.0f},  // Orange: the lighthouse's island, by the door.
    {2000.0f, 1610.0f, 4640.0f}, // Pink: by Cranky's, by the north-east door.
    {1560.0f, 1610.0f, 4300.0f}, // Yellow: by Cranky's, past the coins.
};

// Creepy Castle: one on each tier of the hill, beside DK64's own bananas: by the tree past the
// drawbridge, on the east path up, on the east end of the middle path, on the south walk below
// it, and on the open top beside the castle door.
static const JinjoPlacement jinjos_castle[JINJO_COLORS] = {
    {1150.0f, 472.0f, 330.0f},  // Blue
    {1884.0f, 491.0f, 929.0f},  // Green
    {1870.0f, 904.0f, 1650.0f}, // Orange
    {900.0f, 648.0f, 1680.0f},  // Pink
    {1480.0f, 1731.0f, 1450.0f}, // Yellow
};

// Frantic Factory: one in each area joined to the Foyer (where the player comes in), each over
// the floor in the map's own floor file and clear of DK64's bananas and objects.
static const JinjoPlacement jinjos_factory[JINJO_COLORS] = {
    {1420.0f, 831.0f, 2420.0f}, // Blue: the Foyer, north-east.
    {1395.0f, 840.0f, 2165.0f}, // Green: the north room, inside the notes' U.
    {2040.0f, 862.0f, 2480.0f}, // Orange: the round room at the end of the east corridor.
    {500.0f, 804.0f, 1905.0f},  // Pink: the round platform at the end of the west walkway.
    {1700.0f, 836.0f, 2460.0f}, // Yellow: the east corridor's bend.
};

// Fungi Forest: one in each of its main areas, on DK64's floor (its floor triangles), flat
// round about and 60 or more units from the notes and everything else.
static const JinjoPlacement jinjos_fungi[JINJO_COLORS] = {
    {2900.0f, 223.0f, 2050.0f}, // Blue: on the clock tower's hill, east of the clock.
    {2150.0f, 226.0f, 350.0f},  // Green: on the apple area's west plateau, past the beanstalk.
    {620.0f, 99.0f, 760.0f},    // Orange: on the low ground round the giant mushroom.
    {450.0f, 189.0f, 3900.0f},  // Pink: on the barn's plateau, west of the mill.
    {4530.0f, 83.0f, 4001.0f},  // Yellow: on the owl tree's ring path, south side.
};

// DK Isles: one on each of the main island's beaches the player can walk round (the side
// below the mountain and the Japes lobby's side), Krem Isle's shore, K. Lumsy's island and the
// Banana Fairy's island (the last three reached by swimming), all on sand at about y 500.
static const JinjoPlacement jinjos_isles[JINJO_COLORS] = {
    {2500.0f, 494.0f, 950.0f},  // Blue
    {3480.0f, 501.0f, 1500.0f}, // Green
    {2500.0f, 500.0f, 3170.0f}, // Orange
    {3300.0f, 500.0f, 3200.0f}, // Pink
    {1095.0f, 494.0f, 2281.0f}, // Yellow
};
static const struct {
    Maps map;
    const JinjoPlacement *spots;
} jinjo_maps[] = {
    {MAP_TRAINING_GROUNDS, jinjos_training_grounds},
    {MAP_JAPES, jinjos_japes},
    {MAP_GALLEON, jinjos_galleon},
    {MAP_CASTLE, jinjos_castle},
    {MAP_FACTORY, jinjos_factory},
    {MAP_FUNGI, jinjos_fungi},
    {MAP_DK_ISLES_OVERWORLD, jinjos_isles},
};

typedef enum {
    JINJOS_UNLOADED,
    JINJOS_READY,
    JINJOS_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or sprite didn't convert.
} JinjosLoadState;

// What a Jinjo is doing (chJinjo_update's states, before it's collected).
typedef enum {
    JINJO_STANDING,
    JINJO_WAVING,
    JINJO_HELP,
    JINJO_HOPPING,
} JinjoAction;

typedef struct {
    f32 pos[3];
    s16 yaw;      // 12-bit angle, like an actor's y_rotation.
    u8 action;    // JinjoAction.
    u8 eyes;      // Eye selector, JINJO_EYES_OPEN to JINJO_EYES_CLOSED.
    s8 blink;     // 0 no blink, 1 closing, -1 opening.
    f32 progress; // Through the action's animation, 0 to 1.
} Jinjo;

static u8 jinjo_blocks[JINJO_COLORS][JINJO_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx jinjo_gfx[2][JINJO_COLORS][JINJO_GFX_MAX];
static Mtx jinjo_mtx[2][JINJO_COLORS][JINJO_MTX_MAX];
static JiggyBkPose jinjo_pose;
static JinjosLoadState jinjo_state = JINJOS_UNLOADED;
static s32 jinjo_last_map = -1;
static s32 jinjo_placed;
static u32 jinjo_last_timer;
static Jinjo jinjos[JINJO_COLORS];
// This map's Jinjos (NULL if it has none), picked when the map changes.
static const JinjoPlacement *jinjo_spots;
// Bit i: jinjo_spots[i] has been collected, on the map the Jinjos were last loaded for.
static u32 jinjo_collected;
// Save key (jiggy_save_get) of a map's collected bits.
#define JINJO_SAVE_MAP_KEY(map) (0x4A000000 | (u32)(map)) // 'J'
static u32 jinjo_random_state = 0x4A494E4A;

// The HUD's heads: per head and frame, the sprite recolored for that Jinjo.
static JinjosLoadState hud_state = JINJOS_UNLOADED;
static u16 hud_jinjo_texels[JINJO_COLORS][HUD_JINJO_FRAMES][HUD_JINJO_MAX_TEXELS] __attribute__((aligned(8)));
static s16 hud_jinjo_width[HUD_JINJO_FRAMES];
static s16 hud_jinjo_height[HUD_JINJO_FRAMES];
// Ticks since the counter last came up; it's hidden once it reaches the show time.
static u32 hud_ticks = HUD_SHOW_TICKS;
static u32 hud_last_timer;
// What each head is doing (fxjinjoscore_update's states).
typedef enum {
    HEAD_MISSING,  // Not collected: side on, see-through.
    HEAD_TURNING,  // Just collected: turning to the front.
    HEAD_FRONT,    // Collected, facing front.
    HEAD_WINKING,  // Collected, through the wink frames and back.
    HEAD_UNWINKING,
} HeadState;
static u8 hud_head_state[JINJO_COLORS];
static f32 hud_head_frame[JINJO_COLORS];
static f32 hud_head_rise[JINJO_COLORS];

// 0 to 1. Its own generator, so the Jinjos don't shift DK64's random numbers.
static f32 jinjo_random(void) {
    jinjo_random_state = jinjo_random_state * 1664525 + 1013904223;
    return (jinjo_random_state >> 8) * (1.0f / 16777216.0f);
}

// Degrees from one 12-bit angle to another, -180 to 180.
static f32 jinjo_angle_between(s16 from, s16 to) {
    s32 diff = ((to - from) & 0xFFF);

    if (diff >= 0x800) {
        diff -= 0x1000;
    }
    return diff * (360.0f / 4096.0f);
}

// Where Banjo-Kazooie's own Jinjo sounds play (see JINJO_PICKUP_SOUND); nothing yet but the
// pickup: Banjo-Kazooie's Jinjo jingle, or DK64's pickup sound without a Banjo-Kazooie ROM.
static void jinjo_play_sound(s32 pickup) {
    if (pickup && jiggy_bk_jingle_play(JIGGY_BK_JINGLE_JINJO, 28000.0f / 32767.0f) <= 0) {
        playSound(JINJO_PICKUP_SOUND, 0x7FFF, 63.0f, 1.0f, 5, 0);
    }
}

static void jinjos_load(void) {
    s32 i;

    jinjo_state = JINJOS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    for (i = 0; i < JINJO_COLORS; i++) {
        s32 size = jiggy_bk_model_build(jinjo_models[i], jinjo_blocks[i], sizeof(jinjo_blocks[i]));

        if (size <= 0 || size > (s32)sizeof(jinjo_blocks[i])) {
            return;
        }
    }
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        jinjo_pose.selectors[i] = 0;
    }
    jinjo_state = JINJOS_READY;
}

// Stands each Jinjo on the floor at its spot (or at the spot's height, if no floor is found
// near it), facing the player.
static void jinjos_place(Actor *player) {
    s32 i;

    for (i = 0; i < JINJO_COLORS; i++) {
        Jinjo *jinjo = &jinjos[i];
        f32 probe = jinjo_spots[i].y + JINJO_FLOOR_PROBE;
        f32 floor = probe;

        jinjo->pos[0] = jinjo_spots[i].x;
        jinjo->pos[1] = jinjo_spots[i].y;
        jinjo->pos[2] = jinjo_spots[i].z;
        func_global_asm_80667110(jinjo->pos[0], jinjo->pos[2], &floor);
        if (floor != probe && floor - jinjo_spots[i].y < JINJO_FLOOR_MAX_STEP &&
            jinjo_spots[i].y - floor < JINJO_FLOOR_MAX_STEP) {
            jinjo->pos[1] = floor;
        }
        jinjo->yaw = func_global_asm_80665DE0(player->x_position, player->z_position, jinjo->pos[0], jinjo->pos[2]) & 0xFFF;
        jinjo->action = JINJO_STANDING;
        jinjo->progress = 0.0f;
        jinjo->eyes = JINJO_EYES_OPEN;
        jinjo->blink = 0;
    }
    jinjo_placed = TRUE;
}

// Collects the Jinjos the player is touching.
static void jinjos_collect(Actor *player) {
    s32 i;

    for (i = 0; i < JINJO_COLORS; i++) {
        f32 dx, dy, dz;

        if (jinjo_collected & (1 << i)) {
            continue;
        }
        dx = jinjos[i].pos[0] - player->x_position;
        dy = jinjos[i].pos[1] - player->y_position;
        dz = jinjos[i].pos[2] - player->z_position;
        if (dx * dx + dz * dz > JINJO_TOUCH_RADIUS * JINJO_TOUCH_RADIUS || dy < -JINJO_TOUCH_FEET_ABOVE ||
            dy > JINJO_TOUCH_FEET_BELOW) {
            continue;
        }
        jinjo_collected |= 1 << i;
        jiggy_save_set(current_file, JINJO_SAVE_MAP_KEY(current_map), jinjo_collected);
        // The counter comes up (the heads rise again if it had gone).
        if (hud_ticks >= HUD_SHOW_TICKS) {
            s32 j;

            for (j = 0; j < JINJO_COLORS; j++) {
                hud_head_rise[j] = HUD_JINJO_RISE;
            }
        }
        hud_ticks = 0;
        // The fifth plays Banjo-Kazooie's fifth-Jinjo fanfare in place of the jingle.
        if (jinjo_collected != (1 << JINJO_COLORS) - 1 || !jiggy_jinjo_jiggy_earn(jinjos[i].pos, TRUE)) {
            jinjo_play_sound(TRUE);
        }
    }
}

// A frame of one Jinjo's behavior (chJinjo_update), `frames` game frames on.
static void jinjo_animate(Jinjo *jinjo, Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    s16 to_player = func_global_asm_80665DE0(player->x_position, player->z_position, jinjo->pos[0], jinjo->pos[2]);
    s16 to_jinjo = func_global_asm_80665DE0(jinjo->pos[0], jinjo->pos[2], player->x_position, player->z_position);
    f32 off_facing = jinjo_angle_between(jinjo->yaw, to_player);
    u32 f;

    switch (jinjo->action) {
        case JINJO_STANDING:
            for (f = 0; f < frames; f++) {
                if (jinjo_random() < JINJO_WAVE_CHANCE) {
                    f32 looked_at = jinjo_angle_between(player->y_rotation, to_jinjo);

                    if (looked_at < 0.0f) {
                        looked_at = -looked_at;
                    }
                    jinjo->action = looked_at < JINJO_LOOKED_AT_DEGREES ? JINJO_HELP : JINJO_WAVING;
                    jinjo->progress = 0.0f;
                    break;
                }
            }
            break;
        case JINJO_WAVING:
        case JINJO_HELP:
            jinjo->progress += seconds / JINJO_WAVE_SECONDS;
            break;
        case JINJO_HOPPING: {
            f32 turn = JINJO_TURN_DEGREES_PER_SECOND * seconds;

            jinjo->progress += seconds / JINJO_JUMP_SECONDS;
            // Turns toward the player, without passing them.
            if (off_facing < 0.0f) {
                turn = off_facing < -turn ? -turn : off_facing;
            } else {
                turn = off_facing > turn ? turn : off_facing;
            }
            jinjo->yaw = (jinjo->yaw + (s16)(turn * (4096.0f / 360.0f))) & 0xFFF;
            break;
        }
    }
    if (jinjo->action != JINJO_STANDING && jinjo->progress >= 0.99f) {
        jinjo->action = JINJO_STANDING;
        jinjo->progress = 0.0f;
    }
    if (jinjo->action != JINJO_HOPPING && (off_facing > JINJO_FACE_DEGREES || off_facing < -JINJO_FACE_DEGREES)) {
        jinjo->action = JINJO_HOPPING;
        jinjo->progress = 0.0f;
    }

    // Blinks: an eye frame a game frame, closed then open.
    for (f = 0; f < frames; f++) {
        if (jinjo->blink == 0) {
            if (jinjo_random() < JINJO_BLINK_CHANCE) {
                jinjo->blink = 1;
            }
        } else if (jinjo->blink > 0) {
            if (jinjo->eyes < JINJO_EYES_CLOSED) {
                jinjo->eyes++;
            } else {
                jinjo->blink = -1;
            }
        } else {
            jinjo->eyes--;
            if (jinjo->eyes <= JINJO_EYES_OPEN) {
                jinjo->eyes = JINJO_EYES_OPEN;
                jinjo->blink = 0;
            }
        }
    }
}

static Gfx *jinjo_draw(Gfx *dl, s32 color, s32 buffer) {
    Jinjo *jinjo = &jinjos[color];
    f32 rotate[4][4];
    f32 scale[4][4];

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, jinjo->yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, jinjo_pose.world);
    jinjo_pose.world[3][0] = jinjo->pos[0];
    jinjo_pose.world[3][1] = jinjo->pos[1];
    jinjo_pose.world[3][2] = jinjo->pos[2];

    jinjo_pose.model = jinjo_models[color];
    jinjo_pose.block = jinjo_blocks[color];
    switch (jinjo->action) {
        case JINJO_HELP:
            jinjo_pose.animation = JINJO_ANIM_HELP;
            break;
        case JINJO_HOPPING:
            jinjo_pose.animation = JINJO_ANIM_JUMP;
            break;
        default:
            jinjo_pose.animation = JINJO_ANIM_IDLE;
            break;
    }
    jinjo_pose.progress = jinjo->progress;
    jinjo_pose.selectors[SEL_EYES] = jinjo->eyes;
    jinjo_pose.gfx = jinjo_gfx[buffer][color];
    jinjo_pose.gfx_capacity = JINJO_GFX_MAX;
    jinjo_pose.mtx = jinjo_mtx[buffer][color];
    jinjo_pose.mtx_capacity = JINJO_MTX_MAX;
    if (jiggy_bk_model_pose(&jinjo_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, jinjo_gfx[buffer][color]);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws the map's Jinjos once a frame, with the player.
Gfx *jiggy_jinjos_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    s32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != jinjo_last_map) {
        jinjo_last_map = current_map;
        jinjo_placed = FALSE;
        jinjo_collected = jiggy_save_get(current_file, JINJO_SAVE_MAP_KEY(current_map));
        jinjo_spots = NULL;
        for (i = 0; i < (s32)(sizeof(jinjo_maps) / sizeof(jinjo_maps[0])); i++) {
            if (jinjo_maps[i].map == current_map) {
                jinjo_spots = jinjo_maps[i].spots;
            }
        }
    }
    if (jinjo_spots == NULL) {
        return dl;
    }
    if (jinjo_state == JINJOS_UNLOADED) {
        jinjos_load();
    }
    if (jinjo_state != JINJOS_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!jinjo_placed) {
        if (actor->y_position - actor->floor > JINJO_AIRBORNE_HEIGHT) {
            return dl;
        }
        jinjos_place(actor);
        jinjo_last_timer = object_timer;
        // All five found before Jinjos gave a Jiggy: it's given where the last one stood.
        if (jinjo_collected == (1 << JINJO_COLORS) - 1) {
            jiggy_jinjo_jiggy_earn(jinjos[JINJO_COLORS - 1].pos, FALSE);
        }
    }
    jinjos_collect(actor);
    jiggy_jinjo_jiggy_update(actor);

    frames = object_timer - jinjo_last_timer;
    jinjo_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    for (i = 0; i < JINJO_COLORS; i++) {
        if (jinjo_collected & (1 << i)) {
            continue;
        }
        jinjo_animate(&jinjos[i], actor, frames);
        dl = jinjo_draw(dl, i, buffer);
    }
    return dl;
}

// Banjo-Kazooie's recolor of the pink head's palette for each Jinjo (fxjinjoscore_new),
// applied to the texels the palette made: the white and near-white highlights stay, the rest
// change. Kept as Banjo-Kazooie writes it, overflow of a scaled-up channel included.
static u16 hud_recolor(u16 rgba, s32 color) {
    s32 red = (rgba >> 11) & 0x1F;
    s32 green = (rgba >> 6) & 0x1F;
    s32 blue = (rgba >> 1) & 0x1F;
    s32 alpha = rgba & 1;
    s32 old_green = green;

    if (red < 0x10 || green < 0x10 || blue < 0x10) {
        switch (color) {
            case 0: // Blue
                red = red * 0.25;
                green = old_green * 1.5;
                break;
            case 1: // Green
                red = red * 0.2;
                green = blue;
                blue = blue * 0.2;
                break;
            case 2: // Orange
                green = blue;
                blue = old_green * 0.4;
                green *= 0.6;
                break;
            case 4: // Yellow
                green = blue;
                blue = old_green * 0.4;
                break;
            default: // Pink, the sprite's own colors.
                break;
        }
    }
    return (red << 11) | (green << 6) | (blue << 1) | alpha;
}

static void hud_load(void) {
    s32 count;
    s32 frame;
    s32 color;

    hud_state = JINJOS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(JIGGY_HUD_JINJO);
    if (count < HUD_JINJO_FRAMES) {
        return;
    }
    for (frame = 0; frame < HUD_JINJO_FRAMES; frame++) {
        u16 *pink = hud_jinjo_texels[3][frame];
        s32 size = jiggy_bk_sprite_frame_rgba16(JIGGY_HUD_JINJO, frame, pink, sizeof(hud_jinjo_texels[0][0]));
        s32 width = (size >> 16) & 0xFFFF;
        s32 height = size & 0xFFFF;
        s32 i;

        if (size <= 0 || width == 0 || height == 0 || width * height > HUD_JINJO_MAX_TEXELS) {
            return;
        }
        hud_jinjo_width[frame] = width;
        hud_jinjo_height[frame] = height;
        for (color = 0; color < JINJO_COLORS; color++) {
            if (color == 3) {
                continue;
            }
            for (i = 0; i < width * height; i++) {
                hud_jinjo_texels[color][frame][i] = hud_recolor(pink[i], color);
            }
        }
    }
    hud_state = JINJOS_READY;
}

// The heads' turns and winks, and their rise into place (fxjinjoscore_update), `frames` game
// frames on.
static void hud_animate(u32 frames) {
    f32 seconds = frames / GAME_FPS;
    u32 f;
    s32 i;

    for (i = 0; i < JINJO_COLORS; i++) {
        s32 collected = (jinjo_collected >> i) & 1;

        switch (hud_head_state[i]) {
            case HEAD_MISSING:
                hud_head_frame[i] = 0.0f;
                // A head turns once it's up.
                if (collected && hud_head_rise[i] < 1.0f) {
                    hud_head_state[i] = HEAD_TURNING;
                    // TODO: Banjo-Kazooie plays SFX_3EE here.
                }
                break;
            case HEAD_TURNING:
                hud_head_frame[i] += seconds * HUD_JINJO_TURN_FPS;
                if (hud_head_frame[i] >= HUD_JINJO_FRONT_FRAME) {
                    hud_head_frame[i] = HUD_JINJO_FRONT_FRAME;
                    hud_head_state[i] = HEAD_FRONT;
                }
                break;
            case HEAD_FRONT:
                for (f = 0; f < frames; f++) {
                    if (jinjo_random() < HUD_JINJO_WINK_CHANCE) {
                        hud_head_state[i] = HEAD_WINKING;
                        break;
                    }
                }
                break;
            case HEAD_WINKING:
                hud_head_frame[i] += seconds * HUD_JINJO_WINK_FPS;
                if (hud_head_frame[i] >= HUD_JINJO_LAST_FRAME) {
                    hud_head_frame[i] = HUD_JINJO_LAST_FRAME;
                    hud_head_state[i] = HEAD_UNWINKING;
                }
                break;
            case HEAD_UNWINKING:
                hud_head_frame[i] -= seconds * HUD_JINJO_WINK_FPS;
                if (hud_head_frame[i] < HUD_JINJO_FRONT_FRAME + 1) {
                    hud_head_frame[i] = HUD_JINJO_FRONT_FRAME;
                    hud_head_state[i] = HEAD_FRONT;
                }
                break;
        }
        // Each head rises once the one before it is nearly up.
        if (i == 0 || hud_head_rise[i - 1] < HUD_JINJO_RISE_NEXT) {
            for (f = 0; f < frames && hud_head_rise[i] >= 1.0f; f++) {
                hud_head_rise[i] *= HUD_JINJO_RISE_KEEP;
            }
        }
    }
}

// One RGBA16 image as a texture rectangle at (x, y), top left, in screen pixels.
static Gfx *hud_draw_image(Gfx *dl, u16 *texels, s32 width, s32 height, s32 x, s32 y) {
    gDPLoadTextureTile(dl++, texels, G_IM_FMT_RGBA, G_IM_SIZ_16b,
        width, height,
        0, 0, width - 1, height - 1,
        0,
        G_TX_CLAMP, G_TX_CLAMP,
        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(dl++,
        x << 2, y << 2, (x + width) << 2, (y + height) << 2,
        G_TX_RENDERTILE,
        0, 0, 1 << 10, 1 << 10);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the Jinjo
// counter, for a moment after each Jinjo.
Gfx *jiggy_jinjos_hud_draw(Gfx *dl) {
    u32 frames;
    s32 alpha;
    s32 bottom;
    s32 i;

    if (hud_ticks >= HUD_SHOW_TICKS || cc_number_of_players != 1) {
        return dl;
    }
    if (hud_state == JINJOS_UNLOADED) {
        hud_load();
    }
    if (hud_state != JINJOS_READY) {
        hud_ticks = HUD_SHOW_TICKS;
        return dl;
    }
    hud_ticks += D_global_asm_80744478;
    if (hud_ticks >= HUD_SHOW_TICKS) {
        return dl;
    }
    frames = object_timer - hud_last_timer;
    hud_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    hud_animate(frames);
    alpha = 0xFF;
    if (hud_ticks > HUD_SHOW_TICKS - HUD_FADE_TICKS) {
        alpha = 0xFF * (s32)(HUD_SHOW_TICKS - hud_ticks) / HUD_FADE_TICKS;
    }
    bottom = D_global_asm_80744494 - HUD_BOTTOM_MARGIN;

    dl = alignHUDGameplay(dl, ALIGN_LEFT);
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    for (i = 0; i < JINJO_COLORS; i++) {
        s32 frame = (s32)hud_head_frame[i];
        s32 width = hud_jinjo_width[frame];
        s32 height = hud_jinjo_height[frame];
        u16 *texels = hud_jinjo_texels[i][frame];
        s32 x = HUD_X + i * HUD_JINJO_SPACING;
        s32 y = bottom - height + (s32)hud_head_rise[i];

        if (hud_head_state[i] == HEAD_MISSING) {
            gDPPipeSync(dl++);
            gDPSetCombineLERP(dl++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0);
            gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, HUD_JINJO_MISSING_ALPHA * alpha / 0xFF);
            dl = hud_draw_image(dl, texels, width, height, x, y);
            continue;
        }
        // The shadow: the head's shape in translucent black, down and right of it.
        gDPPipeSync(dl++);
        gDPSetCombineLERP(dl++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
        gDPSetPrimColor(dl++, 0, 0, 0x00, 0x00, 0x00, HUD_JINJO_SHADOW_ALPHA * alpha / 0xFF);
        dl = hud_draw_image(dl, texels, width, height, x, y);
        gDPPipeSync(dl++);
        gDPSetCombineLERP(dl++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0);
        gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
        dl = hud_draw_image(dl, texels, width, height, x - HUD_JINJO_SHADOW_OFFSET, y - HUD_JINJO_SHADOW_OFFSET);
    }
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_LEFT);
    return dl;
}
