// Feathers: Banjo-Kazooie's Red and Gold Feathers, read from the player's Banjo-Kazooie ROM,
// as collectibles of their own beside DK64's own items. Red Feathers are what Banjo flies on
// (jiggy_banjo_fly.c) and Gold Feathers what the Wonderwing burns (jiggy_banjo_wonderwing.c).
//
// Both are sprites in Banjo-Kazooie (src/core2/ch/collectible.c: chRedFeather,
// ASSET_580_SPRITE_RED_FEATHER, and chGoldFeather, ASSET_6D1_SPRITE_GOLDFEATHER; the same
// sprites are their HUD icons, fx/common2score.c). Each has 10 frames of a 64x64 half turn and
// a header size of 175 Banjo-Kazooie units, drawn at the actor's scale of 0.5. Here they are
// halved to 32x32 when loaded (one TMEM load each, and the size the HUD draws them at) and
// spun and stood facing the camera like jiggy_notes.c's notes.
//
// For now they only exist in the Training Grounds (feather_spots below), on the grass around
// the treehouse, between the notes, Jinjos and Honeycombs. They are drawn right after the
// player's own model (func_global_asm_80630DCC in patches_draw.c).
//
// Touching one collects it: it disappears, Banjo-Kazooie's feather jingle plays
// (chCollectible_collectRedFeather and chCollectible_collectGoldFeather: COMUSIC_B and
// COMUSIC_14 at 32000) and its counter comes up for a moment in the HUD pass
// (jiggy_feathers_hud_draw, called from patches_ui.c). Counts stop at Banjo-Kazooie's caps
// before the Cheato codes (item_adjustByDiffWithHud: 50 red, 10 gold). Collected feathers and
// both counts are kept with the DK64 save file (jiggy_save_get/set).
//
// The moves spend them through jiggy_feathers_spend, which works like Banjo-Kazooie's
// func_80346C10: one is taken and the counter comes up, or, with none, the counter comes up
// and Banjo-Kazooie's buzzer plays (COMUSIC_2C_BUZZER at 22000).
// TODO: the collect sparkles (fxSparkle_redFeather, fxSparkle_goldFeather), the first
// pickup's text, the spent-feather actors (func_802D8BE4) and placements on more maps.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

// The two kinds, as jiggy_feathers_spend and jiggy_feathers_hud_hold take them.
#define FEATHER_RED 0
#define FEATHER_GOLD 1
#define FEATHER_KINDS 2
static const u16 feather_sprites[FEATHER_KINDS] = {
    0x580, // ASSET_580_SPRITE_RED_FEATHER
    0x6D1, // ASSET_6D1_SPRITE_GOLDFEATHER
};
// Banjo-Kazooie's caps (item_adjustByDiffWithHud, without FILEPROG_BF/C0 Cheato codes).
static const s32 feather_caps[FEATHER_KINDS] = {50, 10};
// Their pickup jingles, at 32000 (chCollectible_collectItem).
static const u8 feather_jingles[FEATHER_KINDS] = {JIGGY_BK_JINGLE_RED_FEATHER, JIGGY_BK_JINGLE_GOLD_FEATHER};

#define FEATHER_MAX_FRAMES 10
// A frame as loaded (64x64 in the ROM), and as kept: halved to fit TMEM in one load.
#define FEATHER_LOAD_MAX_TEXELS (64 * 64)
#define FEATHER_MAX_TEXELS (32 * 32)
// Ticks (object_timer, 30 Hz) per frame of the spin in the world, as jiggy_notes.c's notes.
#define FEATHER_TICKS_PER_FRAME 2
// How big a feather is in the world: the sprites' header size in Banjo-Kazooie units, the
// actors' scale (chRedFeather, chGoldFeather) and Banjo-Kazooie units to DK64 units (as
// jiggy_banjo3d.c scales Banjo).
#define FEATHER_BK_SIZE 175.0f
#define FEATHER_BK_SCALE 0.5f
#define BK_TO_DK64 0.25f
#define FEATHER_SIZE (FEATHER_BK_SIZE * FEATHER_BK_SCALE * BK_TO_DK64)
// Height of a feather's center above the ground it was placed on.
#define FEATHER_HOVER 16.0f
#define FEATHER_AIRBORNE_HEIGHT 6.0f
// How close the player has to come to collect one: across, and up from their feet.
#define FEATHER_TOUCH_RADIUS 24.0f
#define FEATHER_TOUCH_BELOW 10.0f
#define FEATHER_TOUCH_ABOVE 50.0f
// Looking for the floor under a feather: from this far above its spot, trusted this close.
#define FEATHER_FLOOR_PROBE 20.0f
#define FEATHER_FLOOR_MAX_STEP 40.0f
// DK64's pickup jingle (as jiggy_notes.c plays for a note) without a Banjo-Kazooie ROM.
#define FEATHER_PICKUP_SOUND 0x331
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// The HUD counters (fxcommon2score): the feather sprite at half size (32x32 here) with the
// count in Banjo-Kazooie's bold numbers 25 pixels right of its center (9 past its edge).
// The red one turns forward at 0.18 x 60 frames a second, the gold one backward at
// 0.22 x 60, each wrapping round its 10 frames.
#define HUD_DIGIT_MAX_TEXELS (32 * 32)
#define HUD_TEXT_GAP 9
#define HUD_MAX_DIGITS 2
static const f32 hud_frames_per_second[FEATHER_KINDS] = {0.18f * 60.0f, -0.22f * 60.0f};
// Ticks are 1/60 s (the engine's lag counter, as in jiggy_notes.c).
#define HUD_SHOW_TICKS 150
#define HUD_FADE_TICKS 20
// Where they sit: right side, mirroring the left-aligned counters' 56 pixels in from the
// edge, the red one over the gold one, clear of DK64's own counters along the top and bottom.
// Banjo-Kazooie shows both at the top right.
#define HUD_RIGHT_MARGIN 56
#define HUD_RED_Y 96
#define HUD_ROW_SPACING 36

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u32 D_global_asm_80744478;
extern s16 D_global_asm_80744490;
s16 playSound(s16 arg0, s32 arg1, f32 arg2, f32 arg3, u8 arg4, u8 arg5);
// The floor under (x, z), as the game looks for one (func_global_asm_80667180): writes its
// height to *y, leaving it as it was if there's none. The return value isn't reliable.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);

// Where the feathers go in the Training Grounds: spots on the grass around the treehouse
// (the player starts on its deck, at about (1346, 227, 1098)), each between notes, Jinjos and
// Honeycomb pieces that are reached on foot, and 50 or more units from all of them and from
// the Shock Spring Jump and Flight pads. Heights are the ground's; each is snapped to the
// floor found just above it when placed.
typedef struct {
    f32 x, y, z;
    u8 kind;
    u8 phase; // Step through the spin, so they don't turn in lockstep.
} FeatherPlacement;

#define FEATHER_MAP MAP_TRAINING_GROUNDS
static const FeatherPlacement feather_spots[] = {
    {1300.0f, 32.0f, 980.0f, FEATHER_RED, 0},   // Between a note and two Honeycomb pieces.
    {1420.0f, 33.0f, 960.0f, FEATHER_RED, 5},   // Between two notes and a Jinjo.
    {1430.0f, 35.0f, 1040.0f, FEATHER_RED, 10}, // Between a note and a Honeycomb piece.
    {1145.0f, 30.0f, 960.0f, FEATHER_GOLD, 3},  // Between two Jinjos and a Honeycomb.
    {1600.0f, 35.0f, 960.0f, FEATHER_GOLD, 8},  // Between a note and a Jinjo, by the Flight pad.
};
#define FEATHER_COUNT (sizeof(feather_spots) / sizeof(feather_spots[0]))

typedef enum {
    FEATHERS_UNLOADED,
    FEATHERS_READY,
    FEATHERS_UNAVAILABLE, // No Banjo-Kazooie ROM, or a sprite didn't decode.
} FeathersLoadState;

static u16 feather_load_texels[FEATHER_LOAD_MAX_TEXELS] __attribute__((aligned(8)));
static u16 feather_texels[FEATHER_KINDS][FEATHER_MAX_FRAMES][FEATHER_MAX_TEXELS] __attribute__((aligned(8)));
static s16 feather_width[FEATHER_KINDS][FEATHER_MAX_FRAMES];
static s16 feather_height[FEATHER_KINDS][FEATHER_MAX_FRAMES];
static s32 feather_frames[FEATHER_KINDS];
// A quad per frame, plain and mirrored, centered on the feather.
static Vtx feather_vtx[FEATHER_KINDS][FEATHER_MAX_FRAMES][2][4] __attribute__((aligned(8)));
static Mtx feather_mtx[2][FEATHER_COUNT];
static FeathersLoadState feather_state = FEATHERS_UNLOADED;
static s32 feather_last_map = -1;
static s32 feather_last_file = -1;
static s32 feather_placed;
static f32 feather_pos[FEATHER_COUNT][3];
// Bit i: feather_spots[i] has been collected, for the map the feathers were last loaded on.
static u32 feather_collected;
static s32 feather_count[FEATHER_KINDS];
// Save keys (jiggy_save_get): the collected bits of a map's feathers, and the two counts.
#define FEATHER_SAVE_MAP_KEY(map) (0x46000000 | (u32)(map)) // 'F'
static const u32 feather_save_count_keys[FEATHER_KINDS] = {0x46FFFFFE, 0x46FFFFFF};

static FeathersLoadState hud_state = FEATHERS_UNLOADED;
static u16 hud_digit_texels[10][HUD_DIGIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 hud_digit_width[10];
static s16 hud_digit_height[10];
// Ticks since each counter last came up; it's hidden once it reaches the show time.
static u32 hud_ticks[FEATHER_KINDS] = {HUD_SHOW_TICKS, HUD_SHOW_TICKS};
static f32 hud_frame[FEATHER_KINDS];
static u32 hud_last_timer;

// Reads the counts and the map's collected feathers back from the save file: on every map
// (which also catches a different save file being loaded) and when the save file changes.
static void feathers_sync(void) {
    s32 k;

    if ((s32)current_map == feather_last_map && (s32)current_file == feather_last_file) {
        return;
    }
    feather_last_map = current_map;
    feather_last_file = current_file;
    feather_placed = FALSE;
    feather_collected = jiggy_save_get(current_file, FEATHER_SAVE_MAP_KEY(current_map));
    for (k = 0; k < FEATHER_KINDS; k++) {
        feather_count[k] = jiggy_save_get(current_file, feather_save_count_keys[k]);
        if (feather_count[k] > feather_caps[k]) {
            feather_count[k] = feather_caps[k];
        }
    }
}

static void feather_set_vtx(Vtx *v, s32 x, s32 y, s32 s, s32 t) {
    v->v.ob[0] = x;
    v->v.ob[1] = y;
    v->v.ob[2] = 0;
    v->v.flag = 0;
    v->v.tc[0] = s << 5; // 10.5 fixed point
    v->v.tc[1] = t << 5;
    v->v.cn[0] = 0xFF;
    v->v.cn[1] = 0xFF;
    v->v.cn[2] = 0xFF;
    v->v.cn[3] = 0xFF;
}

// Halves an RGBA16 image into dst: each texel the average color of the opaque ones of a 2x2
// block, opaque if at least two of them are.
static void feather_halve(u16 *dst, const u16 *src, s32 width, s32 height) {
    s32 x, y, i;

    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            const u16 *texel = &src[y * 2 * width + x * 2];
            u16 block[4] = {texel[0], texel[1], texel[width], texel[width + 1]};
            s32 r = 0, g = 0, b = 0, opaque = 0;

            for (i = 0; i < 4; i++) {
                if (block[i] & 1) {
                    r += (block[i] >> 11) & 0x1F;
                    g += (block[i] >> 6) & 0x1F;
                    b += (block[i] >> 1) & 0x1F;
                    opaque++;
                }
            }
            if (opaque < 2) {
                dst[y * (width / 2) + x] = 0;
                continue;
            }
            dst[y * (width / 2) + x] = ((r / opaque) << 11) | ((g / opaque) << 6) | ((b / opaque) << 1) | 1;
        }
    }
}

// Every frame of a feather sprite, halved if it's bigger than one TMEM load. Returns FALSE if
// one didn't decode.
static s32 feather_load_kind(s32 kind) {
    s32 count = jiggy_bk_sprite_frame_count(feather_sprites[kind]);
    s32 i;

    if (count <= 0) {
        return FALSE;
    }
    if (count > FEATHER_MAX_FRAMES) {
        count = FEATHER_MAX_FRAMES;
    }
    for (i = 0; i < count; i++) {
        s32 size = jiggy_bk_sprite_frame_rgba16(feather_sprites[kind], i, feather_load_texels, sizeof(feather_load_texels));
        s32 width = (size >> 16) & 0xFFFF;
        s32 height = size & 0xFFFF;
        Vtx *v;

        if (size <= 0 || width == 0 || height == 0 || width * height > FEATHER_LOAD_MAX_TEXELS) {
            return FALSE;
        }
        if (width * height > FEATHER_MAX_TEXELS) {
            feather_halve(feather_texels[kind][i], feather_load_texels, width, height);
            width /= 2;
            height /= 2;
        } else {
            s32 t;

            for (t = 0; t < width * height; t++) {
                feather_texels[kind][i][t] = feather_load_texels[t];
            }
        }
        feather_width[kind][i] = width;
        feather_height[kind][i] = height;
        // In texels, centered, before the feather's matrix scales it; the second mirrored.
        v = feather_vtx[kind][i][0];
        feather_set_vtx(&v[0], -width / 2, height / 2, 0, 0);
        feather_set_vtx(&v[1], width / 2, height / 2, width, 0);
        feather_set_vtx(&v[2], width / 2, -height / 2, width, height);
        feather_set_vtx(&v[3], -width / 2, -height / 2, 0, height);
        v = feather_vtx[kind][i][1];
        feather_set_vtx(&v[0], -width / 2, height / 2, width, 0);
        feather_set_vtx(&v[1], width / 2, height / 2, 0, 0);
        feather_set_vtx(&v[2], width / 2, -height / 2, 0, height);
        feather_set_vtx(&v[3], -width / 2, -height / 2, width, height);
    }
    feather_frames[kind] = count;
    return TRUE;
}

static void feathers_load(void) {
    feather_state = FEATHERS_UNAVAILABLE;
    if (!jiggy_bk_available() || !feather_load_kind(FEATHER_RED) || !feather_load_kind(FEATHER_GOLD)) {
        return;
    }
    feather_state = FEATHERS_READY;
}

// Lays the feathers out, each over the floor at its spot.
static void feathers_place(void) {
    u32 i;

    for (i = 0; i < FEATHER_COUNT; i++) {
        f32 probe = feather_spots[i].y + FEATHER_FLOOR_PROBE;
        f32 floor = probe;
        f32 ground = feather_spots[i].y;

        func_global_asm_80667110(feather_spots[i].x, feather_spots[i].z, &floor);
        if (floor != probe && floor - feather_spots[i].y < FEATHER_FLOOR_MAX_STEP &&
            feather_spots[i].y - floor < FEATHER_FLOOR_MAX_STEP) {
            ground = floor;
        }
        feather_pos[i][0] = feather_spots[i].x;
        feather_pos[i][1] = ground + FEATHER_HOVER;
        feather_pos[i][2] = feather_spots[i].z;
    }
    feather_placed = TRUE;
}

static void feather_hud_show(s32 kind) {
    if (hud_ticks[kind] >= HUD_SHOW_TICKS) {
        hud_frame[kind] = 0.0f;
    }
    hud_ticks[kind] = 0;
}

// Collects the feathers the player is touching.
static void feathers_collect(Actor *player) {
    u32 i;

    for (i = 0; i < FEATHER_COUNT; i++) {
        s32 kind = feather_spots[i].kind;
        f32 dx, dy, dz;

        if (feather_collected & (1 << i)) {
            continue;
        }
        dx = feather_pos[i][0] - player->x_position;
        dy = feather_pos[i][1] - player->y_position;
        dz = feather_pos[i][2] - player->z_position;
        if (dx * dx + dz * dz > FEATHER_TOUCH_RADIUS * FEATHER_TOUCH_RADIUS || dy < -FEATHER_TOUCH_BELOW ||
            dy > FEATHER_TOUCH_ABOVE) {
            continue;
        }
        feather_collected |= 1 << i;
        if (feather_count[kind] < feather_caps[kind]) {
            feather_count[kind]++;
        }
        jiggy_save_set(current_file, FEATHER_SAVE_MAP_KEY(current_map), feather_collected);
        jiggy_save_set(current_file, feather_save_count_keys[kind], feather_count[kind]);
        feather_hud_show(kind);
        // Banjo-Kazooie's feather jingle, or DK64's pickup sound without a Banjo-Kazooie ROM.
        if (jiggy_bk_jingle_play(feather_jingles[kind], 32000.0f / 32767.0f) <= 0) {
            playSound(FEATHER_PICKUP_SOUND, 0x7FFF, 63.0f, 1.0f, 5, 0);
        }
    }
}

// Spends a feather of `kind` (0 red, 1 gold) for a move, as Banjo-Kazooie's func_80346C10
// does: takes one and brings its counter up, returning TRUE, or, with none left, brings the
// counter up and plays the buzzer, returning FALSE.
s32 jiggy_feathers_spend(s32 kind) {
    feathers_sync();
    feather_hud_show(kind);
    if (feather_count[kind] <= 0) {
        jiggy_bk_jingle_play(JIGGY_BK_JINGLE_BUZZER, 22000.0f / 32767.0f);
        return FALSE;
    }
    feather_count[kind]--;
    jiggy_save_set(current_file, feather_save_count_keys[kind], feather_count[kind]);
    return TRUE;
}

// Keeps a counter up while a move uses it (Banjo-Kazooie shows the red one all through a
// flight); it fades as usual once this stops being called.
void jiggy_feathers_hud_hold(s32 kind) {
    if (hud_ticks[kind] >= HUD_SHOW_TICKS) {
        feather_hud_show(kind);
    }
    hud_ticks[kind] = 0;
}

// A matrix that stands the feather at pos facing the camera, sized for a frame `width`
// texels wide (as jiggy_notes.c stands its notes).
static void feather_billboard(Mtx *m, f32 pos[3], s32 width) {
    CharacterChange *cc = &character_change_array[cc_player_index];
    f32 mf[4][4];
    f32 f[3], r[3], u[3];
    f32 length;
    f32 scale = FEATHER_SIZE / width;

    f[0] = cc->look_at_eye_x - pos[0];
    f[1] = cc->look_at_eye_y - pos[1];
    f[2] = cc->look_at_eye_z - pos[2];
    length = _sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (length < 1.0f) {
        length = 1.0f;
    }
    f[0] /= length;
    f[1] /= length;
    f[2] /= length;
    r[0] = f[2];
    r[1] = 0.0f;
    r[2] = -f[0];
    length = _sqrtf(r[0] * r[0] + r[2] * r[2]);
    if (length < 0.001f) {
        r[0] = 1.0f;
        r[2] = 0.0f;
        length = 1.0f;
    }
    r[0] /= length;
    r[2] /= length;
    u[0] = f[1] * r[2] - f[2] * r[1];
    u[1] = f[2] * r[0] - f[0] * r[2];
    u[2] = f[0] * r[1] - f[1] * r[0];

    mf[0][0] = r[0] * scale; mf[0][1] = r[1] * scale; mf[0][2] = r[2] * scale; mf[0][3] = 0.0f;
    mf[1][0] = u[0] * scale; mf[1][1] = u[1] * scale; mf[1][2] = u[2] * scale; mf[1][3] = 0.0f;
    mf[2][0] = f[0] * scale; mf[2][1] = f[1] * scale; mf[2][2] = f[2] * scale; mf[2][3] = 0.0f;
    mf[3][0] = pos[0];       mf[3][1] = pos[1];       mf[3][2] = pos[2];       mf[3][3] = 1.0f;
    guMtxF2L(mf, m);
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); draws the
// map's feathers once a frame, with the player.
Gfx *jiggy_feathers_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    feathers_sync();
    if (current_map != FEATHER_MAP) {
        return dl;
    }
    if (feather_state == FEATHERS_UNLOADED) {
        feathers_load();
    }
    if (feather_state != FEATHERS_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!feather_placed) {
        if (actor->y_position - actor->floor > FEATHER_AIRBORNE_HEIGHT) {
            return dl;
        }
        feathers_place();
    }
    feathers_collect(actor);

    gDPPipeSync(dl++);
    gSPClearGeometryMode(dl++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN);
    gSPSetGeometryMode(dl++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_PERSP);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_BILERP);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(dl++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);

    for (i = 0; i < FEATHER_COUNT; i++) {
        s32 kind = feather_spots[i].kind;
        s32 frames = feather_frames[kind];
        // A spin is the frames forward, then back mirrored without repeating either end.
        u32 steps = frames > 1 ? (frames - 1) * 2 : 1;
        u32 step = (object_timer / FEATHER_TICKS_PER_FRAME + feather_spots[i].phase) % steps;
        s32 mirrored = step >= (u32)frames;
        s32 frame = mirrored ? steps - step : step;
        s32 width = feather_width[kind][frame];
        s32 height = feather_height[kind][frame];

        if (feather_collected & (1 << i)) {
            continue;
        }
        feather_billboard(&feather_mtx[buffer][i], feather_pos[i], width);
        gSPMatrix(dl++, &feather_mtx[buffer][i], G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPLoadTextureTile(dl++, feather_texels[kind][frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, height,
            0, 0, width - 1, height - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(dl++, feather_vtx[kind][frame][mirrored], 4, 0);
        gSP2Triangles(dl++, 0, 3, 2, 0, 0, 2, 1, 0);
        gSPPopMatrix(dl++, G_MTX_MODELVIEW);
    }

    gDPPipeSync(dl++);
    return dl;
}

static s32 hud_load_rgba16(s32 size, s16 *width, s16 *height, s32 max_texels) {
    s32 w = (size >> 16) & 0xFFFF;
    s32 h = size & 0xFFFF;

    if (size <= 0 || w == 0 || h == 0 || w * h > max_texels) {
        return FALSE;
    }
    *width = w;
    *height = h;
    return TRUE;
}

// The feathers' frames are shared with the world's (feathers_load); the HUD adds the numbers.
static void hud_load(void) {
    s32 i;

    hud_state = FEATHERS_UNAVAILABLE;
    if (feather_state == FEATHERS_UNLOADED) {
        feathers_load();
    }
    if (feather_state != FEATHERS_READY) {
        return;
    }
    for (i = 0; i < 10; i++) {
        s32 size = jiggy_bk_bold_number_rgba16(i, hud_digit_texels[i], sizeof(hud_digit_texels[0]));
        if (!hud_load_rgba16(size, &hud_digit_width[i], &hud_digit_height[i], HUD_DIGIT_MAX_TEXELS)) {
            return;
        }
    }
    hud_state = FEATHERS_READY;
}

// One RGBA16 image as a texture rectangle at (x, y), top left, in screen pixels. Every image
// here fits in TMEM in one load.
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

// One counter: the turning feather and its count, its right edge HUD_RIGHT_MARGIN in from the
// screen's (room is kept for two digits, so the feather doesn't move as the count changes).
static Gfx *hud_draw_counter(Gfx *dl, s32 kind, s32 y, u32 frames) {
    s32 digits[HUD_MAX_DIGITS];
    s32 digit_count = 0;
    s32 value = feather_count[kind];
    s32 count = feather_frames[kind];
    s32 frame;
    s32 alpha;
    s32 x;
    s32 i;

    alpha = 0xFF;
    if (hud_ticks[kind] > HUD_SHOW_TICKS - HUD_FADE_TICKS) {
        alpha = 0xFF * (s32)(HUD_SHOW_TICKS - hud_ticks[kind]) / HUD_FADE_TICKS;
    }
    hud_frame[kind] += hud_frames_per_second[kind] * (frames / GAME_FPS);
    while (hud_frame[kind] >= count) {
        hud_frame[kind] -= count;
    }
    while (hud_frame[kind] < 0.0f) {
        hud_frame[kind] += count;
    }
    frame = (s32)hud_frame[kind];
    if (frame >= count) {
        frame = count - 1;
    }
    do {
        digits[digit_count++] = value % 10;
        value /= 10;
    } while (value > 0 && digit_count < HUD_MAX_DIGITS);

    x = D_global_asm_80744490 - HUD_RIGHT_MARGIN - HUD_MAX_DIGITS * hud_digit_width[0] - HUD_TEXT_GAP -
        feather_width[kind][frame];
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
    dl = hud_draw_image(dl, feather_texels[kind][frame], feather_width[kind][frame], feather_height[kind][frame], x,
                        y);
    x += feather_width[kind][frame] + HUD_TEXT_GAP;
    for (i = digit_count - 1; i >= 0; i--) {
        s32 d = digits[i];
        s32 dy = y + (feather_height[kind][frame] - hud_digit_height[d]) / 2;

        dl = hud_draw_image(dl, hud_digit_texels[d], hud_digit_width[d], hud_digit_height[d], x, dy);
        x += hud_digit_width[d];
    }
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the red and
// gold feather counters, each for a moment after a pickup or a spend, and while a move holds
// it up.
Gfx *jiggy_feathers_hud_draw(Gfx *dl) {
    u32 frames;
    s32 shown[FEATHER_KINDS];
    s32 k;

    frames = object_timer - hud_last_timer;
    hud_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    if ((hud_ticks[FEATHER_RED] >= HUD_SHOW_TICKS && hud_ticks[FEATHER_GOLD] >= HUD_SHOW_TICKS) ||
        cc_number_of_players != 1) {
        return dl;
    }
    if (hud_state == FEATHERS_UNLOADED) {
        hud_load();
    }
    if (hud_state != FEATHERS_READY) {
        hud_ticks[FEATHER_RED] = HUD_SHOW_TICKS;
        hud_ticks[FEATHER_GOLD] = HUD_SHOW_TICKS;
        return dl;
    }
    for (k = 0; k < FEATHER_KINDS; k++) {
        if (hud_ticks[k] < HUD_SHOW_TICKS) {
            hud_ticks[k] += D_global_asm_80744478;
        }
        shown[k] = hud_ticks[k] < HUD_SHOW_TICKS;
    }
    if (!shown[FEATHER_RED] && !shown[FEATHER_GOLD]) {
        return dl;
    }

    dl = alignHUDGameplay(dl, ALIGN_RIGHT);
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    // The sprites' and numbers' own colors, their alpha scaled by the primitive's.
    gDPSetCombineLERP(dl++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0);
    for (k = 0; k < FEATHER_KINDS; k++) {
        if (shown[k]) {
            dl = hud_draw_counter(dl, k, HUD_RED_Y + k * HUD_ROW_SPACING, frames);
        }
    }
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_RIGHT);
    return dl;
}
