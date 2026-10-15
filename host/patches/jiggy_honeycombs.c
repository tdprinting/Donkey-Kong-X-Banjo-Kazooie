// Honeycombs: Banjo-Kazooie's empty Honeycomb pieces and Honeycombs, read from the player's
// Banjo-Kazooie ROM, as collectibles of their own beside DK64's colored bananas and melons.
//
// Both are models in Banjo-Kazooie (src/core2/code_42CB0.c: chEmptyHoneycomb,
// ASSET_361_MODEL_EMPTY_HONEYCOMB, and chHoneycomb, ASSET_363_MODEL_HONEYCOMB), with no
// skeleton and no animation, so they are drawn in their rest pose (jiggy_bk_model_pose with
// animation -1). Like chHoneycomb_update, each spins about its upright axis at 200 degrees
// a second, some one way and some the other.
//
// For now they only exist in the Training Grounds and Jungle Japes (honeycomb_maps below).
// They are drawn right after the player's own model (func_global_asm_80630DCC in
// patches_draw.c).
//
// Touching an empty Honeycomb piece collects it: it disappears, Banjo-Kazooie's empty
// Honeycomb jingle plays, and Banjo-Kazooie's piece counter (fxhoneycarrierscore in
// src/core2/fx/honeycarrierscore.c: a ring of six piece sprites, ASSET_7DC, the collected
// ones solid) comes up for a moment in the HUD pass (jiggy_honeycombs_hud_draw, called from
// patches_ui.c). Collected pieces and their total are kept with the DK64 save file
// (jiggy_save_get/set), as Banjo-Kazooie's honeycombscore keeps a bit per piece.
//
// Touching a Honeycomb collects it the way a melon slice is (func_global_asm_806C9974 with
// 1, as func_global_asm_806F54E0 does for a slice, behaviour 0x57): one slice of health back, up
// to the player's melons. As in Banjo-Kazooie they aren't saved, and are back each time the
// map is entered.
// TODO: the collect sparkles (fxSparkle_emptyHoneycomb and fxSparkle_honeycomb), the first
// pickup's text, and what six pieces give (Banjo-Kazooie's health upgrade,
// COMUSIC_18_HEALTH_UPGRADE; here, perhaps a melon).

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define HONEYCOMB_EMPTY_MODEL 0x361 // ASSET_361_MODEL_EMPTY_HONEYCOMB
#define HONEYCOMB_MODEL 0x363       // ASSET_363_MODEL_HONEYCOMB
// A converted Honeycomb is under 5 KiB; its pose is a handful of commands and loads one
// matrix (the root: the models have no bones).
#define HONEYCOMB_BLOCK_MAX 0x2000
#define HONEYCOMB_GFX_MAX 16
#define HONEYCOMB_MTX_MAX 4
// How fast they spin (chHoneycomb_update), in degrees a second.
#define HONEYCOMB_SPIN_DEGREES_PER_SECOND 200.0f
// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo: an empty Honeycomb
// piece is 90 units across and 78 tall in Banjo-Kazooie, 22 and 20 here.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
#define HONEYCOMB_AIRBORNE_HEIGHT 6.0f
// Height of a Honeycomb's base above the ground it was placed on.
#define HONEYCOMB_HOVER 6.0f
// Placing a Honeycomb on the floor under its spot: looked for from this far above the spot,
// and only trusted this close to the spot's height.
#define HONEYCOMB_FLOOR_PROBE 20.0f
#define HONEYCOMB_FLOOR_MAX_STEP 40.0f
// How close the player has to come to collect a Honeycomb: across, and how far the player's
// feet can be above its base (about its height) or below it (about the player's).
#define HONEYCOMB_TOUCH_RADIUS 24.0f
#define HONEYCOMB_TOUCH_FEET_ABOVE 26.0f
#define HONEYCOMB_TOUCH_FEET_BELOW 36.0f
// DK64's pickup jingle (as jiggy_notes.c plays for a note) without a Banjo-Kazooie ROM.
// Banjo-Kazooie plays COMUSIC_17_EMPTY_HONEYCOMB_COLLECTED for a piece and
// COMUSIC_16_HONEYCOMB_COLLECTED for a Honeycomb, both at 28000 (baMarker's collision
// handler, src/core2/ba/marker.c).
#define HONEYCOMB_PICKUP_SOUND 0x331

// The HUD counter: Banjo-Kazooie's empty Honeycomb piece (ASSET_7DC, one 32x32 frame), six
// round a ring.
#define JIGGY_HUD_PIECE 0x7DC
#define HUD_PIECE_MAX_TEXELS (32 * 32)
#define HUD_PIECES 6
// The ring (fxhoneycarrierscore_draw): pieces 24.5 pixels from its center, the first at
// -272 degrees (just about straight up) and each next 60 degrees on clockwise. The ring turns
// so the newest piece comes to the top: a third of the way a second (at least 2 degrees a
// frame), snapping the last 2 degrees.
#define HUD_RING_RADIUS 24.5f
#define HUD_RING_FIRST_DEGREES -272.0f
#define HUD_RING_STEP_DEGREES 60.0f
#define HUD_RING_TURN_RATE 3.0f
#define HUD_RING_TURN_MIN 2.0f
// Missing pieces are see-through; the newest is too until the ring has turned it to the top,
// then fades in by 0x10 a frame.
#define HUD_PIECE_MISSING_ALPHA 0x50
#define HUD_PIECE_FADE_STEP 0x10
// Ticks are 1/60 s (the engine's lag counter, as in jiggy_notes.c).
#define HUD_SHOW_TICKS 210
#define HUD_FADE_TICKS 20
// Where the ring sits: left, above jiggy_jinjos.c's row of heads (32-pixel heads, HUD_X 56,
// 36 + 32 + 6 pixels off the bottom), with a gap between, so the left-aligned counters
// stack: notes, Jinjos, then pieces.
#define HUD_X 56
#define HUD_RING_EXTENT ((s32)HUD_RING_RADIUS + 16)
#define HUD_BOTTOM_MARGIN (36 + 32 + 6 + 32 + 6)

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u32 D_global_asm_80744478;
extern s16 D_global_asm_80744494;
s16 playSound(s16 arg0, s32 arg1, f32 arg2, f32 arg3, u8 arg4, u8 arg5);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
// The floor under (x, z), as the game looks for one (func_global_asm_80667180): writes its
// height to *y, leaving it as it was if there's none. The return value isn't reliable.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// Changes a player's health by `amount` slices, applied (and capped at their melons) on the
// next frame; what a melon slice does when collected. Returns TRUE if it leaves them none.
s32 func_global_asm_806C9974(u8 playerIndex, s8 amount);

// Where the Honeycombs go, map by map, each spot clear of the Jinjos' and notes' and
// between spots found by walking them, so all can be reached on foot. Heights are the
// ground's; each is snapped to the floor found just above it when placed.
typedef struct {
    f32 x, y, z;
} HoneycombPlacement;

// The Training Grounds: on the grass around the treehouse (the player starts on its deck, at
// about (1346, 227, 1098)).
static const HoneycombPlacement pieces_training_grounds[] = {
    {1250.0f, 30.0f, 930.0f},
    {1490.0f, 35.0f, 1015.0f},
    {1300.0f, 30.0f, 1050.0f},
};
static const HoneycombPlacement combs_training_grounds[] = {
    {1200.0f, 30.0f, 960.0f},
    {1310.0f, 30.0f, 880.0f},
};
// Jungle Japes: by the entrance path and in the first clearing.
static const HoneycombPlacement pieces_japes[] = {
    {1462.0f, 280.0f, 259.0f},
    {950.0f, 280.0f, 505.0f},
};
static const HoneycombPlacement combs_japes[] = {
    {1116.0f, 280.0f, 250.0f},
};
// Gloomy Galleon: one piece on the lighthouse's island, one on the open floor by the entrance;
// a Honeycomb on the east side, by Nipper (jiggy_nipper.c), for after his claws.
static const HoneycombPlacement pieces_galleon[] = {
    {2050.0f, 1450.0f, 760.0f},
    {2200.0f, 1610.0f, 3360.0f},
};
static const HoneycombPlacement combs_galleon[] = {
    {3220.0f, 1670.0f, 3470.0f},
};

// Creepy Castle: the pieces on the south-west walk and up the path near the top, a Honeycomb
// on the grass past the drawbridge.
static const HoneycombPlacement pieces_castle[] = {
    {346.0f, 646.0f, 1669.0f},
    {1015.0f, 1592.0f, 1626.0f},
};
static const HoneycombPlacement combs_castle[] = {
    {1065.0f, 472.0f, 354.0f},
};
#define HONEYCOMB_MAP(map, pieces, combs) \
    {map, pieces, sizeof(pieces) / sizeof(pieces[0]), combs, sizeof(combs) / sizeof(combs[0])}
static const struct {
    Maps map;
    const HoneycombPlacement *pieces;
    u32 piece_count;
    const HoneycombPlacement *combs;
    u32 comb_count;
} honeycomb_maps[] = {
    HONEYCOMB_MAP(MAP_TRAINING_GROUNDS, pieces_training_grounds, combs_training_grounds),
    HONEYCOMB_MAP(MAP_JAPES, pieces_japes, combs_japes),
    HONEYCOMB_MAP(MAP_GALLEON, pieces_galleon, combs_galleon),
    HONEYCOMB_MAP(MAP_CASTLE, pieces_castle, combs_castle),
};
// The most of each on one map (collected bits are one u32).
#define PIECE_COUNT_MAX 8
#define COMB_COUNT_MAX 8

typedef enum {
    HONEYCOMBS_UNLOADED,
    HONEYCOMBS_READY,
    HONEYCOMBS_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or sprite didn't convert.
} HoneycombsLoadState;

typedef struct {
    f32 pos[3];
    f32 yaw; // Degrees.
} Honeycomb;

static u8 piece_block[HONEYCOMB_BLOCK_MAX] __attribute__((aligned(16)));
static u8 comb_block[HONEYCOMB_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx piece_gfx[2][PIECE_COUNT_MAX][HONEYCOMB_GFX_MAX];
static Mtx piece_mtx[2][PIECE_COUNT_MAX][HONEYCOMB_MTX_MAX];
static Gfx comb_gfx[2][COMB_COUNT_MAX][HONEYCOMB_GFX_MAX];
static Mtx comb_mtx[2][COMB_COUNT_MAX][HONEYCOMB_MTX_MAX];
static JiggyBkPose honeycomb_pose;
static HoneycombsLoadState honeycomb_state = HONEYCOMBS_UNLOADED;
static s32 honeycomb_last_map = -1;
static s32 honeycomb_placed;
static u32 honeycomb_last_timer;
static Honeycomb pieces[PIECE_COUNT_MAX];
static Honeycomb combs[COMB_COUNT_MAX];
// This map's pieces and Honeycombs (none if it has none), picked when the map changes.
static const HoneycombPlacement *piece_spots;
static u32 piece_count;
static const HoneycombPlacement *comb_spots;
static u32 comb_count;
// Bit i: piece_spots[i] has been collected, on the map the pieces were last loaded for.
static u32 piece_collected;
static s32 pieces_total;
// Bit i: comb_spots[i] has been collected since the map was entered.
static u32 comb_collected;
// Save keys (jiggy_save_get): the collected bits of a map's pieces, and the total.
#define PIECE_SAVE_MAP_KEY(map) (0x48000000 | (u32)(map)) // 'H'
#define PIECE_SAVE_TOTAL_KEY 0x48FFFFFF

static HoneycombsLoadState hud_state = HONEYCOMBS_UNLOADED;
static u16 hud_piece_texels[HUD_PIECE_MAX_TEXELS] __attribute__((aligned(8)));
static s16 hud_piece_width;
static s16 hud_piece_height;
// Ticks since the counter last came up; it's hidden once it reaches the show time.
static u32 hud_ticks = HUD_SHOW_TICKS;
static u32 hud_last_timer;
// Pieces the ring shows (1 to 6), how far it has turned (degrees), and the newest piece's
// alpha (HUD_PIECE_MISSING_ALPHA until the ring has turned it to the top).
static s32 hud_pieces;
static f32 hud_ring_turn;
static s32 hud_newest_alpha = 0xFF;

// Pieces in the ring for a total: 1 to 6 once any are collected, as Banjo-Kazooie's
// ITEM_13_EMPTY_HONEYCOMB counts up to its sixth.
static s32 hud_pieces_for(s32 total) {
    return total <= 0 ? 0 : total - HUD_PIECES * ((total - 1) / HUD_PIECES);
}

static void honeycombs_load(void) {
    s32 size;
    s32 i;

    honeycomb_state = HONEYCOMBS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(HONEYCOMB_EMPTY_MODEL, piece_block, sizeof(piece_block));
    if (size <= 0 || size > (s32)sizeof(piece_block)) {
        return;
    }
    size = jiggy_bk_model_build(HONEYCOMB_MODEL, comb_block, sizeof(comb_block));
    if (size <= 0 || size > (s32)sizeof(comb_block)) {
        return;
    }
    // Neither model has appendages to pick.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        honeycomb_pose.selectors[i] = 0;
    }
    honeycomb_state = HONEYCOMBS_READY;
}

// Stands a Honeycomb over the floor at its spot (or over the spot's height, if no floor is
// found near it).
static void honeycomb_place(Honeycomb *comb, const HoneycombPlacement *spot) {
    f32 probe = spot->y + HONEYCOMB_FLOOR_PROBE;
    f32 floor = probe;
    f32 ground = spot->y;

    func_global_asm_80667110(spot->x, spot->z, &floor);
    if (floor != probe && floor - spot->y < HONEYCOMB_FLOOR_MAX_STEP && spot->y - floor < HONEYCOMB_FLOOR_MAX_STEP) {
        ground = floor;
    }
    comb->pos[0] = spot->x;
    comb->pos[1] = ground + HONEYCOMB_HOVER;
    comb->pos[2] = spot->z;
    comb->yaw = 0.0f;
}

static void honeycombs_place(void) {
    u32 i;

    for (i = 0; i < piece_count; i++) {
        honeycomb_place(&pieces[i], &piece_spots[i]);
    }
    for (i = 0; i < comb_count; i++) {
        honeycomb_place(&combs[i], &comb_spots[i]);
    }
    honeycomb_placed = TRUE;
}

static s32 honeycomb_touching(Honeycomb *comb, Actor *player) {
    f32 dx = comb->pos[0] - player->x_position;
    f32 dy = comb->pos[1] - player->y_position;
    f32 dz = comb->pos[2] - player->z_position;

    return dx * dx + dz * dz <= HONEYCOMB_TOUCH_RADIUS * HONEYCOMB_TOUCH_RADIUS &&
           dy >= -HONEYCOMB_TOUCH_FEET_ABOVE && dy <= HONEYCOMB_TOUCH_FEET_BELOW;
}

// Collects the pieces and Honeycombs the player is touching.
static void honeycombs_collect(Actor *player) {
    u32 i;

    for (i = 0; i < piece_count; i++) {
        if ((piece_collected & (1 << i)) || !honeycomb_touching(&pieces[i], player)) {
            continue;
        }
        piece_collected |= 1 << i;
        pieces_total++;
        jiggy_save_set(current_file, PIECE_SAVE_MAP_KEY(current_map), piece_collected);
        jiggy_save_set(current_file, PIECE_SAVE_TOTAL_KEY, pieces_total);
        // The counter comes up with the ring where it was before this piece (where it ended
        // last time, if it's still up), and turns the new one to the top.
        if (hud_ticks >= HUD_SHOW_TICKS || hud_pieces_for(pieces_total) == 1) {
            hud_ring_turn = (hud_pieces_for(pieces_total) - 2) * HUD_RING_STEP_DEGREES;
            if (hud_ring_turn < 0.0f) {
                hud_ring_turn = 0.0f;
            }
        }
        hud_pieces = hud_pieces_for(pieces_total);
        hud_newest_alpha = HUD_PIECE_MISSING_ALPHA;
        hud_ticks = 0;
        // Banjo-Kazooie's empty Honeycomb jingle, or DK64's pickup sound without a
        // Banjo-Kazooie ROM.
        if (jiggy_bk_jingle_play(JIGGY_BK_JINGLE_EMPTY_HONEYCOMB, 28000.0f / 32767.0f) <= 0) {
            playSound(HONEYCOMB_PICKUP_SOUND, 0x7FFF, 63.0f, 1.0f, 5, 0);
        }
    }
    for (i = 0; i < comb_count; i++) {
        if ((comb_collected & (1 << i)) || !honeycomb_touching(&combs[i], player)) {
            continue;
        }
        comb_collected |= 1 << i;
        // A slice of health back, as a melon slice gives.
        func_global_asm_806C9974(cc_player_index, 1);
        if (jiggy_bk_jingle_play(JIGGY_BK_JINGLE_HONEYCOMB, 28000.0f / 32767.0f) <= 0) {
            playSound(HONEYCOMB_PICKUP_SOUND, 0x7FFF, 63.0f, 1.0f, 5, 0);
        }
    }
}

// Spins a Honeycomb (chHoneycomb_update) `frames` game frames on. Banjo-Kazooie picks the
// way each turns at random; here they alternate.
static void honeycomb_spin(Honeycomb *comb, s32 index, u32 frames) {
    f32 turn = HONEYCOMB_SPIN_DEGREES_PER_SECOND * (frames / GAME_FPS);

    comb->yaw += (index & 1) ? -turn : turn;
    if (comb->yaw >= 360.0f) {
        comb->yaw -= 360.0f;
    }
    if (comb->yaw < 0.0f) {
        comb->yaw += 360.0f;
    }
}

static Gfx *honeycomb_draw(Gfx *dl, Honeycomb *comb, s32 model, void *block, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, comb->yaw, 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, honeycomb_pose.world);
    honeycomb_pose.world[3][0] = comb->pos[0];
    honeycomb_pose.world[3][1] = comb->pos[1];
    honeycomb_pose.world[3][2] = comb->pos[2];

    honeycomb_pose.model = model;
    honeycomb_pose.block = block;
    honeycomb_pose.animation = -1;
    honeycomb_pose.progress = 0.0f;
    honeycomb_pose.gfx = gfx;
    honeycomb_pose.gfx_capacity = HONEYCOMB_GFX_MAX;
    honeycomb_pose.mtx = mtx;
    honeycomb_pose.mtx_capacity = HONEYCOMB_MTX_MAX;
    if (jiggy_bk_model_pose(&honeycomb_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); spins and
// draws the map's Honeycombs once a frame, with the player.
Gfx *jiggy_honeycombs_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != honeycomb_last_map) {
        honeycomb_last_map = current_map;
        honeycomb_placed = FALSE;
        // Read back on every map, which also catches a different save file being loaded.
        piece_collected = jiggy_save_get(current_file, PIECE_SAVE_MAP_KEY(current_map));
        pieces_total = jiggy_save_get(current_file, PIECE_SAVE_TOTAL_KEY);
        comb_collected = 0;
        piece_count = 0;
        comb_count = 0;
        for (i = 0; i < (s32)(sizeof(honeycomb_maps) / sizeof(honeycomb_maps[0])); i++) {
            if (honeycomb_maps[i].map == current_map) {
                piece_spots = honeycomb_maps[i].pieces;
                piece_count = honeycomb_maps[i].piece_count;
                comb_spots = honeycomb_maps[i].combs;
                comb_count = honeycomb_maps[i].comb_count;
            }
        }
    }
    if (piece_count == 0 && comb_count == 0) {
        return dl;
    }
    if (honeycomb_state == HONEYCOMBS_UNLOADED) {
        honeycombs_load();
    }
    if (honeycomb_state != HONEYCOMBS_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!honeycomb_placed) {
        if (actor->y_position - actor->floor > HONEYCOMB_AIRBORNE_HEIGHT) {
            return dl;
        }
        honeycombs_place();
        honeycomb_last_timer = object_timer;
    }
    honeycombs_collect(actor);

    frames = object_timer - honeycomb_last_timer;
    honeycomb_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    for (i = 0; i < piece_count; i++) {
        if (piece_collected & (1 << i)) {
            continue;
        }
        honeycomb_spin(&pieces[i], i, frames);
        dl = honeycomb_draw(dl, &pieces[i], HONEYCOMB_EMPTY_MODEL, piece_block, piece_gfx[buffer][i], piece_mtx[buffer][i]);
    }
    for (i = 0; i < comb_count; i++) {
        if (comb_collected & (1 << i)) {
            continue;
        }
        honeycomb_spin(&combs[i], i, frames);
        dl = honeycomb_draw(dl, &combs[i], HONEYCOMB_MODEL, comb_block, comb_gfx[buffer][i], comb_mtx[buffer][i]);
    }
    return dl;
}

static void hud_load(void) {
    s32 size;
    s32 width;
    s32 height;

    hud_state = HONEYCOMBS_UNAVAILABLE;
    if (!jiggy_bk_available() || jiggy_bk_sprite_frame_count(JIGGY_HUD_PIECE) <= 0) {
        return;
    }
    size = jiggy_bk_sprite_frame_rgba16(JIGGY_HUD_PIECE, 0, hud_piece_texels, sizeof(hud_piece_texels));
    width = (size >> 16) & 0xFFFF;
    height = size & 0xFFFF;
    if (size <= 0 || width == 0 || height == 0 || width * height > HUD_PIECE_MAX_TEXELS) {
        return;
    }
    hud_piece_width = width;
    hud_piece_height = height;
    hud_state = HONEYCOMBS_READY;
}

// The ring's turn toward the newest piece, and that piece's fade-in once it's at the top
// (fxhoneycarrierscore_update), `frames` game frames on.
static void hud_animate(u32 frames) {
    f32 target = (hud_pieces - 1) * HUD_RING_STEP_DEGREES;
    u32 f;

    for (f = 0; f < frames; f++) {
        if (hud_ring_turn < target) {
            f32 step = (target - hud_ring_turn) * HUD_RING_TURN_RATE / GAME_FPS;

            hud_ring_turn += step > HUD_RING_TURN_MIN ? step : HUD_RING_TURN_MIN;
            if (hud_ring_turn > target - HUD_RING_TURN_MIN) {
                hud_ring_turn = target;
                // The piece clicks into place.
                if (hud_pieces != 1) {
                    jiggy_bk_sfx_play(JIGGY_BK_SFX_SWITCH_PRESS, 0x7FF8 / 32767.0f, 1.3f);
                }
            }
        } else if (hud_newest_alpha < 0xFF) {
            hud_newest_alpha += HUD_PIECE_FADE_STEP;
            if (hud_newest_alpha > 0xFF) {
                hud_newest_alpha = 0xFF;
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

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the empty
// Honeycomb counter, for a moment after each piece. Banjo-Kazooie turns the pieces' quads
// with the ring; here they keep upright (a hexagon a sixth of a turn round looks the same
// once the ring stops), and only move round it.
Gfx *jiggy_honeycombs_hud_draw(Gfx *dl) {
    u32 frames;
    s32 alpha;
    s32 center_x, center_y;
    s32 k;

    if (hud_ticks >= HUD_SHOW_TICKS || cc_number_of_players != 1) {
        return dl;
    }
    if (hud_state == HONEYCOMBS_UNLOADED) {
        hud_load();
    }
    if (hud_state != HONEYCOMBS_READY) {
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
    center_x = HUD_X + HUD_RING_EXTENT;
    center_y = D_global_asm_80744494 - HUD_BOTTOM_MARGIN - HUD_RING_EXTENT;

    dl = alignHUDGameplay(dl, ALIGN_LEFT);
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    // The sprite's own colors, its alpha scaled by the primitive's.
    gDPSetCombineLERP(dl++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0);
    // Piece k is the (k + 1)th collected, in Banjo-Kazooie's y-up degrees round the ring.
    for (k = 0; k < HUD_PIECES; k++) {
        f32 degrees = HUD_RING_FIRST_DEGREES - k * HUD_RING_STEP_DEGREES + hud_ring_turn;
        s16 angle = (s16)(degrees * (4096.0f / 360.0f)) & 0xFFF;
        s32 x = center_x + (s32)(func_global_asm_80612790(angle) * HUD_RING_RADIUS) - hud_piece_width / 2;
        s32 y = center_y - (s32)(func_global_asm_80612794(angle) * HUD_RING_RADIUS) - hud_piece_height / 2;
        s32 piece_alpha = 0xFF;

        if (k >= hud_pieces) {
            piece_alpha = HUD_PIECE_MISSING_ALPHA;
        } else if (k == hud_pieces - 1) {
            piece_alpha = hud_newest_alpha;
        }
        gDPPipeSync(dl++);
        gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, piece_alpha * alpha / 0xFF);
        dl = hud_draw_image(dl, hud_piece_texels, hud_piece_width, hud_piece_height, x, y);
    }
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_LEFT);
    return dl;
}
