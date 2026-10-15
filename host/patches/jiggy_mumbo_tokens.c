// Mumbo Tokens: Banjo-Kazooie's silver skull tokens, read from the player's Banjo-Kazooie
// ROM, as a collectible of their own beside DK64's own items.
//
// A Mumbo Token is a sprite in Banjo-Kazooie (src/core2/ch/mumbotoken.c: chMumboToken,
// ASSET_41A_SPRITE_MUMBO_TOKEN, drawn by fxTouchSparkle_draw; the same sprite is its HUD icon,
// fx/common2score.c). It has 12 frames of a full turn, 32x32, and a header size of 90
// Banjo-Kazooie units. Its header asks for the frames played backward, two ticks each, with
// no mirrored half (the cube prop animation in src/core2/code_A5BC0.c, type 3 reversed), and
// here it is played that way, standing facing the camera like jiggy_feathers.c's feathers.
//
// For now they only exist in the Training Grounds and Jungle Japes (token_maps below), two on
// each, between the notes, Jinjos, Honeycombs and feathers. They are drawn right after the
// player's own model (func_global_asm_80630DCC in patches_draw.c).
//
// Touching one collects it: it disappears, Banjo-Kazooie's token sound plays
// (chMumboToken_collect: SFX 0x401 at pitch 1.0 and volume 0x7FFF) and Banjo-Kazooie's token
// counter (fxcommon2score with ITEM_1C_MUMBO_TOKEN: the turning token and its count, shown up
// to 99) comes up for a moment in the HUD pass (jiggy_mumbo_tokens_hud_draw, called from
// patches_ui.c). Collected tokens and the total are kept with the DK64 save file
// (jiggy_save_get/set), as Banjo-Kazooie's mumboscore keeps a bit per token.
// TODO: the collect sparkle (fxSparkle_mumboToken), the twinkles round an uncollected token,
// the first pickup's text (FILEPROG_4_MUMBO_TOKEN_TEXT). Mumbo (jiggy_mumbo.c) spends them,
// taking them off the saved total and calling jiggy_mumbo_tokens_refresh.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define JIGGY_MUMBO_TOKEN 0x41A // ASSET_41A_SPRITE_MUMBO_TOKEN
#define TOKEN_MAX_FRAMES 12
// 32x32 in the ROM: one TMEM load a frame, and the size the HUD draws it at.
#define TOKEN_MAX_TEXELS (32 * 32)
// Ticks (object_timer, 30 Hz) per frame of the turn in the world, from the sprite's header.
#define TOKEN_TICKS_PER_FRAME 2
// How big a token is in the world: the sprite's header size in Banjo-Kazooie units (the
// actor is drawn at scale 1) and Banjo-Kazooie units to DK64 units (as jiggy_banjo3d.c scales
// Banjo).
#define TOKEN_BK_SIZE 90.0f
#define BK_TO_DK64 0.25f
#define TOKEN_SIZE (TOKEN_BK_SIZE * BK_TO_DK64)
// Height of a token's center above the ground it was placed on.
#define TOKEN_HOVER 16.0f
#define TOKEN_AIRBORNE_HEIGHT 6.0f
// How close the player has to come to collect one: across, and up from their feet.
#define TOKEN_TOUCH_RADIUS 24.0f
#define TOKEN_TOUCH_BELOW 10.0f
#define TOKEN_TOUCH_ABOVE 50.0f
// Looking for the floor under a token: from this far above its spot, trusted this close.
#define TOKEN_FLOOR_PROBE 20.0f
#define TOKEN_FLOOR_MAX_STEP 40.0f
// Banjo-Kazooie's token sound (chMumboToken_collect), or DK64's pickup jingle (as
// jiggy_notes.c plays for a note) without a Banjo-Kazooie ROM.
#define TOKEN_PICKUP_SOUND 0x331
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// The HUD counter (fxcommon2score, ITEM_1C_MUMBO_TOKEN): the token sprite turning forward at
// 0.3 x 60 frames a second, with the count in Banjo-Kazooie's bold numbers 32 pixels right of
// its center (16 past its edge), shown up to 99. Banjo-Kazooie draws the sprite at 1.1 times
// its size; here it is drawn at its own 32x32.
#define HUD_DIGIT_MAX_TEXELS (32 * 32)
#define HUD_TEXT_GAP 16
#define HUD_MAX_DIGITS 2
#define HUD_MAX_VALUE 99
#define HUD_FRAMES_PER_SECOND (0.3f * 60.0f)
// Ticks are 1/60 s (the engine's lag counter, as in jiggy_notes.c).
#define HUD_SHOW_TICKS 150
#define HUD_FADE_TICKS 20
// Where it sits: right side, under jiggy_feathers.c's red and gold counters (y 96 and 132,
// 36 apart), as Banjo-Kazooie shows it at the lower right.
#define HUD_RIGHT_MARGIN 56
#define HUD_Y (96 + 2 * 36)

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

// Where the tokens go, map by map: on the ground reached on foot, 50 or more units from the
// notes, Jinjos, Honeycombs, feathers and pads. Heights are the ground's; each is snapped to
// the floor found just above it when placed.
typedef struct {
    f32 x, y, z;
    u8 phase; // Step through the turn, so they don't turn in lockstep.
} TokenPlacement;

// The Training Grounds: on the grass around the treehouse (the player starts on its deck, at
// about (1346, 227, 1098)).
static const TokenPlacement tokens_training_grounds[] = {
    {1220.0f, 30.0f, 1035.0f, 0}, // Between a Jinjo, a Honeycomb and a Honeycomb piece.
    {1480.0f, 32.0f, 860.0f, 6},  // Between a note, a Jinjo and the Flight pad.
};
// Jungle Japes' first area, off the note trail that runs from the entrance up to the far end.
static const TokenPlacement tokens_japes[] = {
    {1050.0f, 280.0f, 300.0f, 0}, // Above the trail's bend, between two Jinjos and the Honeycomb.
    {1400.0f, 280.0f, 330.0f, 6}, // Past the trail's end, by a Honeycomb piece.
};
// Gloomy Galleon: by the entrance, by Cranky's and on the east side.
static const TokenPlacement tokens_galleon[] = {
    {2000.0f, 1612.0f, 2850.0f, 0}, // West of where the player comes in.
    {1750.0f, 1610.0f, 4250.0f, 6}, // Past the door by Cranky's.
    {3150.0f, 1670.0f, 3420.0f, 3}, // The east side, between its bananas and coins.
};

// Creepy Castle: the middle path's east end, the west path up from the south walk, and the top.
static const TokenPlacement tokens_castle[] = {
    {1910.0f, 904.0f, 1600.0f, 0},
    {350.0f, 728.0f, 1250.0f, 4},
    {1250.0f, 1731.0f, 1250.0f, 8},
};

// Frantic Factory: the Foyer's south-east, the west walkway and the north room's north-west.
static const TokenPlacement tokens_factory[] = {
    {1420.0f, 831.0f, 2625.0f, 0}, // The Foyer, between the notes' ring and the corner.
    {790.0f, 802.0f, 2440.0f, 4},  // The west walkway, between two of DK64's bananas.
    {1110.0f, 840.0f, 2090.0f, 8}, // The north room, by the whistle switches.
};

// Fungi Forest: on DK64's floor (its floor triangles), flat round about, 60 or more units from
// the notes and everything else.
static const TokenPlacement tokens_fungi[] = {
    {1300.0f, 99.0f, 700.0f, 0},   // On the low ground east of the giant mushroom.
    {650.0f, 191.0f, 3700.0f, 4},  // On the barn's plateau, west of the mill.
    {4100.0f, 115.0f, 2050.0f, 8}, // In the far north-east field.
};

// DK Isles: on the main island's beach, by the tunnel, past the Japes lobby and below the
// mountain, on sand at about y 500.
static const TokenPlacement tokens_isles[] = {
    {2460.0f, 495.0f, 2420.0f, 0},
    {3675.0f, 494.0f, 1909.0f, 4},
    {3000.0f, 500.0f, 950.0f, 8},
};

// Angry Aztec (with Gobi's Valley).
static const TokenPlacement tokens_aztec[] = {
    {2700.0f, 120.0f, 1320.0f, 0}, // Behind Trunker, at the foot of the tunnel.
    {3280.0f, 120.0f, 2050.0f, 4}, // Beside the way down from the tunnel.
    {3700.0f, 120.0f, 4200.0f, 8}, // The main area's south sand, above the note trail.
};

// Crystal Caves: in the Kremlings' camp, on the north-east round ledge and on the slope out west.
static const TokenPlacement tokens_caves[] = {
    {2550.0f, 14.0f, 540.0f, 0},
    {3400.0f, 287.0f, 760.0f, 4},
    {850.0f, 141.0f, 2050.0f, 8},
};
#define TOKEN_MAP_SPOTS(map, spots) {map, spots, sizeof(spots) / sizeof(spots[0])}
static const struct {
    Maps map;
    const TokenPlacement *spots;
    u32 count;
} token_maps[] = {
    TOKEN_MAP_SPOTS(MAP_TRAINING_GROUNDS, tokens_training_grounds),
    TOKEN_MAP_SPOTS(MAP_JAPES, tokens_japes),
    TOKEN_MAP_SPOTS(MAP_GALLEON, tokens_galleon),
    TOKEN_MAP_SPOTS(MAP_CASTLE, tokens_castle),
    TOKEN_MAP_SPOTS(MAP_FACTORY, tokens_factory),
    TOKEN_MAP_SPOTS(MAP_FUNGI, tokens_fungi),
    TOKEN_MAP_SPOTS(MAP_DK_ISLES_OVERWORLD, tokens_isles),
    TOKEN_MAP_SPOTS(MAP_AZTEC, tokens_aztec),
    TOKEN_MAP_SPOTS(MAP_CAVES, tokens_caves),
};
// The most tokens on one map (collected bits are one u32).
#define TOKEN_COUNT_MAX 8
// The current map's tokens, or none.
static const TokenPlacement *token_spots;
static u32 token_count_on_map;

typedef enum {
    TOKENS_UNLOADED,
    TOKENS_READY,
    TOKENS_UNAVAILABLE, // No Banjo-Kazooie ROM, or the sprite didn't decode.
} TokensLoadState;

static u16 token_texels[TOKEN_MAX_FRAMES][TOKEN_MAX_TEXELS] __attribute__((aligned(8)));
static s16 token_width[TOKEN_MAX_FRAMES];
static s16 token_height[TOKEN_MAX_FRAMES];
static s32 token_frames;
// A quad per frame, centered on the token.
static Vtx token_vtx[TOKEN_MAX_FRAMES][4] __attribute__((aligned(8)));
static Mtx token_mtx[2][TOKEN_COUNT_MAX];
static TokensLoadState token_state = TOKENS_UNLOADED;
static s32 token_last_map = -1;
static s32 token_last_file = -1;
static s32 token_placed;
static f32 token_pos[TOKEN_COUNT_MAX][3];
// Bit i: token_spots[i] has been collected, for the map the tokens were last loaded on.
static u32 token_collected;
static s32 tokens_total;
// Save keys (jiggy_save_get): the collected bits of a map's tokens, and the total.
#define TOKEN_SAVE_MAP_KEY(map) (0x4D000000 | (u32)(map)) // 'M'
#define TOKEN_SAVE_TOTAL_KEY 0x4DFFFFFF

static TokensLoadState hud_state = TOKENS_UNLOADED;
static u16 hud_digit_texels[10][HUD_DIGIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 hud_digit_width[10];
static s16 hud_digit_height[10];
// Ticks since the counter last came up; it's hidden once it reaches the show time.
static u32 hud_ticks = HUD_SHOW_TICKS;
static f32 hud_frame;
static u32 hud_last_timer;

// Reads the total and the map's collected tokens back from the save file: on every map
// (which also catches a different save file being loaded) and when the save file changes.
static void tokens_sync(void) {
    s32 k;

    if ((s32)current_map == token_last_map && (s32)current_file == token_last_file) {
        return;
    }
    token_last_map = current_map;
    token_last_file = current_file;
    token_placed = FALSE;
    token_spots = NULL;
    token_count_on_map = 0;
    for (k = 0; k < (s32)(sizeof(token_maps) / sizeof(token_maps[0])); k++) {
        if (token_maps[k].map == current_map) {
            token_spots = token_maps[k].spots;
            token_count_on_map = token_maps[k].count;
        }
    }
    token_collected = jiggy_save_get(current_file, TOKEN_SAVE_MAP_KEY(current_map));
    tokens_total = jiggy_save_get(current_file, TOKEN_SAVE_TOTAL_KEY);
}

static void token_set_vtx(Vtx *v, s32 x, s32 y, s32 s, s32 t) {
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

static void tokens_load(void) {
    s32 count;
    s32 i;

    token_state = TOKENS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(JIGGY_MUMBO_TOKEN);
    if (count <= 0) {
        return;
    }
    if (count > TOKEN_MAX_FRAMES) {
        count = TOKEN_MAX_FRAMES;
    }
    for (i = 0; i < count; i++) {
        s32 size = jiggy_bk_sprite_frame_rgba16(JIGGY_MUMBO_TOKEN, i, token_texels[i], sizeof(token_texels[0]));
        s32 width = (size >> 16) & 0xFFFF;
        s32 height = size & 0xFFFF;
        Vtx *v = token_vtx[i];

        if (size <= 0 || width == 0 || height == 0 || width * height > TOKEN_MAX_TEXELS) {
            return;
        }
        token_width[i] = width;
        token_height[i] = height;
        // In texels, centered, before the token's matrix scales it.
        token_set_vtx(&v[0], -width / 2, height / 2, 0, 0);
        token_set_vtx(&v[1], width / 2, height / 2, width, 0);
        token_set_vtx(&v[2], width / 2, -height / 2, width, height);
        token_set_vtx(&v[3], -width / 2, -height / 2, 0, height);
    }
    token_frames = count;
    token_state = TOKENS_READY;
}

// Lays the tokens out, each over the floor at its spot.
static void tokens_place(void) {
    u32 i;

    for (i = 0; i < token_count_on_map; i++) {
        f32 probe = token_spots[i].y + TOKEN_FLOOR_PROBE;
        f32 floor = probe;
        f32 ground = token_spots[i].y;

        func_global_asm_80667110(token_spots[i].x, token_spots[i].z, &floor);
        if (floor != probe && floor - token_spots[i].y < TOKEN_FLOOR_MAX_STEP &&
            token_spots[i].y - floor < TOKEN_FLOOR_MAX_STEP) {
            ground = floor;
        }
        token_pos[i][0] = token_spots[i].x;
        token_pos[i][1] = ground + TOKEN_HOVER;
        token_pos[i][2] = token_spots[i].z;
    }
    token_placed = TRUE;
}

// Collects the tokens the player is touching.
static void tokens_collect(Actor *player) {
    u32 i;

    for (i = 0; i < token_count_on_map; i++) {
        f32 dx, dy, dz;

        if (token_collected & (1 << i)) {
            continue;
        }
        dx = token_pos[i][0] - player->x_position;
        dy = token_pos[i][1] - player->y_position;
        dz = token_pos[i][2] - player->z_position;
        if (dx * dx + dz * dz > TOKEN_TOUCH_RADIUS * TOKEN_TOUCH_RADIUS || dy < -TOKEN_TOUCH_BELOW ||
            dy > TOKEN_TOUCH_ABOVE) {
            continue;
        }
        token_collected |= 1 << i;
        tokens_total++;
        jiggy_save_set(current_file, TOKEN_SAVE_MAP_KEY(current_map), token_collected);
        jiggy_save_set(current_file, TOKEN_SAVE_TOTAL_KEY, tokens_total);
        if (hud_ticks >= HUD_SHOW_TICKS) {
            hud_frame = 0.0f;
        }
        hud_ticks = 0;
        // Banjo-Kazooie's token sound, or DK64's pickup sound without a Banjo-Kazooie ROM.
        if (jiggy_bk_sfx_play(JIGGY_BK_SFX_MUMBO_TOKEN, 0x7FFF / 32767.0f, 1.0f) <= 0) {
            playSound(TOKEN_PICKUP_SOUND, 0x7FFF, 63.0f, 1.0f, 5, 0);
        }
    }
}

// Called when tokens are spent (jiggy_mumbo.c) after the total is saved: reads it back, and
// brings the counter up showing it, as Banjo-Kazooie's item_adjustByDiffWithHud does.
void jiggy_mumbo_tokens_refresh(void) {
    tokens_total = jiggy_save_get(current_file, TOKEN_SAVE_TOTAL_KEY);
    if (hud_ticks >= HUD_SHOW_TICKS) {
        hud_frame = 0.0f;
    }
    hud_ticks = 0;
}

// A matrix that stands the token at pos facing the camera, sized for a frame `width` texels
// wide (as jiggy_feathers.c stands its feathers).
static void token_billboard(Mtx *m, f32 pos[3], s32 width) {
    CharacterChange *cc = &character_change_array[cc_player_index];
    f32 mf[4][4];
    f32 f[3], r[3], u[3];
    f32 length;
    f32 scale = TOKEN_SIZE / width;

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
// map's tokens once a frame, with the player.
Gfx *jiggy_mumbo_tokens_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    tokens_sync();
    if (token_count_on_map == 0) {
        return dl;
    }
    if (token_state == TOKENS_UNLOADED) {
        tokens_load();
    }
    if (token_state != TOKENS_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!token_placed) {
        if (actor->y_position - actor->floor > TOKEN_AIRBORNE_HEIGHT) {
            return dl;
        }
        tokens_place();
    }
    tokens_collect(actor);

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

    for (i = 0; i < token_count_on_map; i++) {
        // The frames backward, round and round (the sprite's header: type 3, reversed).
        u32 step = (object_timer / TOKEN_TICKS_PER_FRAME + token_spots[i].phase) % token_frames;
        s32 frame = token_frames - 1 - step;
        s32 width = token_width[frame];
        s32 height = token_height[frame];

        if (token_collected & (1 << i)) {
            continue;
        }
        token_billboard(&token_mtx[buffer][i], token_pos[i], width);
        gSPMatrix(dl++, &token_mtx[buffer][i], G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPLoadTextureTile(dl++, token_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, height,
            0, 0, width - 1, height - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(dl++, token_vtx[frame], 4, 0);
        gSP2Triangles(dl++, 0, 3, 2, 0, 0, 2, 1, 0);
        gSPPopMatrix(dl++, G_MTX_MODELVIEW);
    }

    gDPPipeSync(dl++);
    return dl;
}

// The token's frames are shared with the world's (tokens_load); the HUD adds the numbers.
static void hud_load(void) {
    s32 i;

    hud_state = TOKENS_UNAVAILABLE;
    if (token_state == TOKENS_UNLOADED) {
        tokens_load();
    }
    if (token_state != TOKENS_READY) {
        return;
    }
    for (i = 0; i < 10; i++) {
        s32 size = jiggy_bk_bold_number_rgba16(i, hud_digit_texels[i], sizeof(hud_digit_texels[0]));
        s32 width = (size >> 16) & 0xFFFF;
        s32 height = size & 0xFFFF;

        if (size <= 0 || width == 0 || height == 0 || width * height > HUD_DIGIT_MAX_TEXELS) {
            return;
        }
        hud_digit_width[i] = width;
        hud_digit_height[i] = height;
    }
    hud_state = TOKENS_READY;
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

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the Mumbo
// Token counter, for a moment after each token, its right edge HUD_RIGHT_MARGIN in from the
// screen's (room is kept for two digits, so the token doesn't move as the count changes).
Gfx *jiggy_mumbo_tokens_hud_draw(Gfx *dl) {
    s32 digits[HUD_MAX_DIGITS];
    s32 digit_count = 0;
    s32 value;
    u32 frames;
    s32 frame;
    s32 alpha;
    s32 x;
    s32 i;

    frames = object_timer - hud_last_timer;
    hud_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    if (hud_ticks >= HUD_SHOW_TICKS || cc_number_of_players != 1) {
        return dl;
    }
    if (hud_state == TOKENS_UNLOADED) {
        hud_load();
    }
    if (hud_state != TOKENS_READY) {
        hud_ticks = HUD_SHOW_TICKS;
        return dl;
    }
    hud_ticks += D_global_asm_80744478;
    if (hud_ticks >= HUD_SHOW_TICKS) {
        return dl;
    }
    alpha = 0xFF;
    if (hud_ticks > HUD_SHOW_TICKS - HUD_FADE_TICKS) {
        alpha = 0xFF * (s32)(HUD_SHOW_TICKS - hud_ticks) / HUD_FADE_TICKS;
    }
    hud_frame += HUD_FRAMES_PER_SECOND * (frames / GAME_FPS);
    while (hud_frame >= token_frames) {
        hud_frame -= token_frames;
    }
    frame = (s32)hud_frame;
    if (frame >= token_frames) {
        frame = token_frames - 1;
    }
    value = tokens_total > HUD_MAX_VALUE ? HUD_MAX_VALUE : tokens_total;
    do {
        digits[digit_count++] = value % 10;
        value /= 10;
    } while (value > 0 && digit_count < HUD_MAX_DIGITS);

    dl = alignHUDGameplay(dl, ALIGN_RIGHT);
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    // The sprite's and numbers' own colors, their alpha scaled by the primitive's.
    gDPSetCombineLERP(dl++, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, TEXEL0, TEXEL0, 0, PRIMITIVE, 0);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
    x = D_global_asm_80744490 - HUD_RIGHT_MARGIN - HUD_MAX_DIGITS * hud_digit_width[0] - HUD_TEXT_GAP -
        token_width[frame];
    dl = hud_draw_image(dl, token_texels[frame], token_width[frame], token_height[frame], x, HUD_Y);
    x += token_width[frame] + HUD_TEXT_GAP;
    for (i = digit_count - 1; i >= 0; i--) {
        s32 d = digits[i];
        s32 dy = HUD_Y + (token_height[frame] - hud_digit_height[d]) / 2;

        dl = hud_draw_image(dl, hud_digit_texels[d], hud_digit_width[d], hud_digit_height[d], x, dy);
        x += hud_digit_width[d];
    }
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_RIGHT);
    return dl;
}
