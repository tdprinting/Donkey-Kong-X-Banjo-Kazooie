// Music Notes: Banjo-Kazooie's own collectible note (ASSET_6D6_SPRITE_MUSIC_NOTE, the sprite
// src/core2/ch/musicnote.c and the levels' sprite props use), read from the player's
// Banjo-Kazooie ROM, as a collectible of its own beside DK64's colored bananas.
//
// The sprite has 10 frames of a half turn. Like Banjo-Kazooie (cube prop animation in
// src/core2/code_A5BC0.c, which the sprite's header selects), the note plays them forward
// and then back mirrored, two ticks a frame, for a full spin, and always faces the camera.
//
// For now the notes only exist as short trails in the Training Grounds and Jungle Japes
// (note_trails below). They are drawn right after the player's own model
// (func_global_asm_80630DCC in patches_draw.c).
//
// Touching a note collects it: it disappears, a pickup sound plays, and Banjo-Kazooie's note
// counter (its spinning HUD note, ASSET_7D9_SPRITE_NOTE, and its bold HUD numbers) shows the
// total for a moment in the HUD pass (jiggy_notes_hud_draw, called from patches_ui.c).
// Collected notes and the total are kept with the DK64 save file (jiggy_save_get/set).
// TODO: the collect sparkle and Banjo-Kazooie's own note sound.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define JIGGY_MUSIC_NOTE 0x6D6
#define NOTE_MAX_FRAMES 10
#define NOTE_MAX_TEXELS (64 * 64)
// RGBA16 texels that fit in TMEM (4 KiB) at once; bigger frames are drawn in strips.
#define TMEM_RGBA16_TEXELS 2048
#define NOTE_MAX_STRIPS (NOTE_MAX_TEXELS / TMEM_RGBA16_TEXELS)
// Ticks (object_timer, 30 Hz) per frame, from the sprite's header.
#define NOTE_TICKS_PER_FRAME 2
// How big a note is in the world: the sprite's size in Banjo-Kazooie units (its header,
// 210 for the 64-texel frame), the note actor's scale (musicNote_update), and Banjo-Kazooie
// units to DK64 units (as jiggy_banjo3d.c scales Banjo).
#define NOTE_BK_SIZE 210.0f
#define NOTE_BK_SCALE 0.42857143f
#define BK_TO_DK64 0.25f
#define NOTE_SIZE (NOTE_BK_SIZE * NOTE_BK_SCALE * BK_TO_DK64)
// Height of a note's center above the ground it was placed on.
#define NOTE_HOVER 16.0f
#define NOTE_AIRBORNE_HEIGHT 6.0f
// How close the player has to come to collect a note: across, and up from their feet.
#define NOTE_TOUCH_RADIUS 24.0f
#define NOTE_TOUCH_BELOW 10.0f
#define NOTE_TOUCH_ABOVE 50.0f
// DK64's pickup jingle (the one ammo and feathers play in func_global_asm_806F54E0) until
// Banjo-Kazooie's sounds can be played.
#define NOTE_PICKUP_SOUND 0x331

// The HUD counter: Banjo-Kazooie's spinning HUD note and its bold numbers.
#define JIGGY_HUD_NOTE 0x7D9
#define HUD_NOTE_MAX_FRAMES 10
#define HUD_NOTE_MAX_TEXELS (32 * 32)
#define HUD_DIGIT_MAX_TEXELS (32 * 32)
// Ticks are 1/60 s (the engine's lag counter, as in jiggy_cameo.c).
#define HUD_SHOW_TICKS 150
#define HUD_FADE_TICKS 20
#define HUD_TICKS_PER_FRAME 4
#define HUD_MAX_DIGITS 3
// Where the counter sits, bottom left like Banjo-Kazooie's. The left-aligned HUD pulls in by
// the UI pillar, so this keeps it clear of the edge the way DK64's own counters are.
#define HUD_X 56
#define HUD_BOTTOM_MARGIN 36
// The HUD note's color: the sprite is intensity only, tinted the way the game's notes are.
#define HUD_NOTE_R 0xFF
#define HUD_NOTE_G 0xC8
#define HUD_NOTE_B 0x00

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

// Where the notes go, map by map, each spot found by walking it so all can be reached on
// foot. Each note hovers over the floor found under its spot (or over the spot's own height,
// if none is found near it), at a step through the spin so the trail doesn't turn in lockstep.
typedef struct {
    f32 x, y, z;
    u8 phase;
} NotePlacement;

// The Training Grounds: over the grass below the treehouse deck (where the player starts),
// from the foot of the deck past the pond's edge.
static const NotePlacement notes_training_grounds[] = {
    {1360.0f, 37.0f, 1010.0f, 0},
    {1366.0f, 33.0f, 940.0f, 3},
    {1372.0f, 29.0f, 870.0f, 6},
    {1480.0f, 34.0f, 940.0f, 9},
    {1550.0f, 36.0f, 990.0f, 12},
};
// Jungle Japes: along the path from the entrance through the first clearing.
static const NotePlacement notes_japes[] = {
    {945.0f, 280.0f, 660.0f, 0},
    {962.0f, 280.0f, 560.0f, 3},
    {985.0f, 280.0f, 450.0f, 6},
    {1060.0f, 280.0f, 395.0f, 9},
    {1150.0f, 280.0f, 385.0f, 12},
    {1225.0f, 280.0f, 350.0f, 15},
    {1300.0f, 280.0f, 300.0f, 2},
    {1380.0f, 280.0f, 240.0f, 5},
};
#define NOTE_TRAIL(map, notes) {map, notes, sizeof(notes) / sizeof(notes[0])}
static const struct {
    Maps map;
    const NotePlacement *notes;
    u32 count;
} note_trails[] = {
    NOTE_TRAIL(MAP_TRAINING_GROUNDS, notes_training_grounds),
    NOTE_TRAIL(MAP_JAPES, notes_japes),
};
// The most notes on one map (collected bits are one u32).
#define NOTE_COUNT_MAX 16
// Looking for the floor under a note: from this far above its spot, trusted this close to it.
#define NOTE_FLOOR_PROBE 20.0f
#define NOTE_FLOOR_MAX_STEP 40.0f

typedef enum {
    NOTES_UNLOADED,
    NOTES_READY,
    NOTES_UNAVAILABLE, // No Banjo-Kazooie ROM, or the sprite didn't decode.
} NotesLoadState;

static u16 note_texels[NOTE_MAX_FRAMES][NOTE_MAX_TEXELS] __attribute__((aligned(8)));
static s16 note_width[NOTE_MAX_FRAMES];
static s16 note_height[NOTE_MAX_FRAMES];
// A quad per strip of each frame, plain and mirrored, centered on the note.
static Vtx note_vtx[NOTE_MAX_FRAMES][2][NOTE_MAX_STRIPS][4] __attribute__((aligned(8)));
static Mtx note_mtx[2][NOTE_COUNT_MAX];
static s32 note_frames;
static NotesLoadState note_state = NOTES_UNLOADED;
static s32 note_last_map = -1;
static s32 note_placed;
static f32 note_pos[NOTE_COUNT_MAX][3];
// This map's notes (none if it has none), picked when the map changes.
static const NotePlacement *note_trail;
static u32 note_count;
// Bit i: note_trail[i] has been collected, for the map the notes were last loaded on.
static u32 note_collected;
static s32 notes_total;
// Save keys (jiggy_save_get): the collected bits of a map's notes, and the total.
#define NOTE_SAVE_MAP_KEY(map) (0x4E000000 | (u32)(map)) // 'N'
#define NOTE_SAVE_TOTAL_KEY 0x4EFFFFFF

static NotesLoadState hud_state = NOTES_UNLOADED;
static u16 hud_note_texels[HUD_NOTE_MAX_FRAMES][HUD_NOTE_MAX_TEXELS] __attribute__((aligned(8)));
static s16 hud_note_width[HUD_NOTE_MAX_FRAMES];
static s16 hud_note_height[HUD_NOTE_MAX_FRAMES];
static s32 hud_note_frames;
static u16 hud_digit_texels[10][HUD_DIGIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 hud_digit_width[10];
static s16 hud_digit_height[10];
// Ticks since the counter last came up; it's hidden once it reaches the show time.
static u32 hud_ticks = HUD_SHOW_TICKS;

static void note_set_vtx(Vtx *v, s32 x, s32 y, s32 s, s32 t) {
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

// Vertices in texels, so a frame's quad is width x height before the note's matrix scales it.
static void note_build_vtx(s32 frame) {
    s32 width = note_width[frame];
    s32 height = note_height[frame];
    s32 rows_per_load = TMEM_RGBA16_TEXELS / width;
    s32 mirrored;
    s32 strip;
    s32 row;

    for (mirrored = 0; mirrored < 2; mirrored++) {
        s32 left = mirrored ? width : 0;
        s32 right = mirrored ? 0 : width;

        for (row = 0, strip = 0; row < height; row += rows_per_load, strip++) {
            s32 rows = row + rows_per_load > height ? height - row : rows_per_load;
            s32 top = height / 2 - row;
            s32 bottom = top - rows;
            Vtx *v = note_vtx[frame][mirrored][strip];

            note_set_vtx(&v[0], -width / 2, top, left, 0);
            note_set_vtx(&v[1], width / 2, top, right, 0);
            note_set_vtx(&v[2], width / 2, bottom, right, rows);
            note_set_vtx(&v[3], -width / 2, bottom, left, rows);
        }
    }
}

static void notes_load(void) {
    s32 count;
    s32 i;

    note_state = NOTES_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(JIGGY_MUSIC_NOTE);
    if (count <= 0) {
        return;
    }
    if (count > NOTE_MAX_FRAMES) {
        count = NOTE_MAX_FRAMES;
    }
    // Every frame is needed for the spin, so one that fails leaves the notes out.
    for (i = 0; i < count; i++) {
        s32 size = jiggy_bk_sprite_frame_rgba16(JIGGY_MUSIC_NOTE, i, note_texels[i], sizeof(note_texels[0]));
        s32 width = (size >> 16) & 0xFFFF;
        s32 height = size & 0xFFFF;

        if (size <= 0 || width == 0 || height == 0 || width * height > NOTE_MAX_TEXELS ||
            width > TMEM_RGBA16_TEXELS) {
            return;
        }
        note_width[i] = width;
        note_height[i] = height;
        note_build_vtx(i);
    }
    note_frames = count;
    note_state = NOTES_READY;
}

// Lays the trail out, each note over the floor at its spot.
static void notes_place(Actor *player) {
    u32 i;

    for (i = 0; i < note_count; i++) {
        f32 probe = note_trail[i].y + NOTE_FLOOR_PROBE;
        f32 floor = probe;
        f32 ground = note_trail[i].y;

        func_global_asm_80667110(note_trail[i].x, note_trail[i].z, &floor);
        if (floor != probe && floor - note_trail[i].y < NOTE_FLOOR_MAX_STEP &&
            note_trail[i].y - floor < NOTE_FLOOR_MAX_STEP) {
            ground = floor;
        }
        note_pos[i][0] = note_trail[i].x;
        note_pos[i][1] = ground + NOTE_HOVER;
        note_pos[i][2] = note_trail[i].z;
    }
    note_placed = TRUE;
}

// Collects the notes the player is touching.
static void notes_collect(Actor *player) {
    u32 i;

    for (i = 0; i < note_count; i++) {
        f32 dx, dy, dz;

        if (note_collected & (1 << i)) {
            continue;
        }
        dx = note_pos[i][0] - player->x_position;
        dy = note_pos[i][1] - player->y_position;
        dz = note_pos[i][2] - player->z_position;
        if (dx * dx + dz * dz > NOTE_TOUCH_RADIUS * NOTE_TOUCH_RADIUS || dy < -NOTE_TOUCH_BELOW ||
            dy > NOTE_TOUCH_ABOVE) {
            continue;
        }
        note_collected |= 1 << i;
        notes_total++;
        jiggy_save_set(current_file, NOTE_SAVE_MAP_KEY(current_map), note_collected);
        jiggy_save_set(current_file, NOTE_SAVE_TOTAL_KEY, notes_total);
        hud_ticks = 0;
        // Banjo-Kazooie's note jingle, or DK64's pickup sound without a Banjo-Kazooie ROM.
        if (jiggy_bk_jingle_play(JIGGY_BK_JINGLE_NOTE, 16000.0f / 32767.0f) <= 0) {
            playSound(NOTE_PICKUP_SOUND, 0x7FFF, 63.0f, 1.0f, 5, 0);
        }
    }
}

// A matrix that stands the note at pos facing the camera, sized for a frame `width` texels
// wide. Rows are the note's right, up and toward-the-camera axes (row vectors, as guMtxF2L
// takes them).
static void note_billboard(Mtx *m, f32 pos[3], s32 width) {
    CharacterChange *cc = &character_change_array[cc_player_index];
    f32 mf[4][4];
    f32 f[3], r[3], u[3];
    f32 length;
    f32 scale = NOTE_SIZE / width;

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
    // right = up x toward-camera; straight down on the note, any right will do.
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

static Gfx *note_draw(Gfx *dl, Mtx *m, s32 frame, s32 mirrored) {
    s32 width = note_width[frame];
    s32 height = note_height[frame];
    s32 rows_per_load = TMEM_RGBA16_TEXELS / width;
    s32 strip;
    s32 row;

    gSPMatrix(dl++, m, G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    for (row = 0, strip = 0; row < height; row += rows_per_load, strip++) {
        s32 rows = row + rows_per_load > height ? height - row : rows_per_load;

        gDPLoadTextureTile(dl++, &note_texels[frame][row * width], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, rows,
            0, 0, width - 1, rows - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(dl++, note_vtx[frame][mirrored][strip], 4, 0);
        gSP2Triangles(dl++, 0, 3, 2, 0, 0, 2, 1, 0);
    }
    gSPPopMatrix(dl++, G_MTX_MODELVIEW);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); draws the
// map's notes once a frame, with the player.
Gfx *jiggy_notes_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 steps;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != note_last_map) {
        note_last_map = current_map;
        note_placed = FALSE;
        // Read back on every map, which also catches a different save file being loaded.
        note_collected = jiggy_save_get(current_file, NOTE_SAVE_MAP_KEY(current_map));
        notes_total = jiggy_save_get(current_file, NOTE_SAVE_TOTAL_KEY);
        note_count = 0;
        for (i = 0; i < sizeof(note_trails) / sizeof(note_trails[0]); i++) {
            if (note_trails[i].map == current_map) {
                note_trail = note_trails[i].notes;
                note_count = note_trails[i].count;
            }
        }
    }
    if (note_count == 0) {
        return dl;
    }
    if (note_state == NOTES_UNLOADED) {
        notes_load();
    }
    if (note_state != NOTES_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!note_placed) {
        if (actor->y_position - actor->floor > NOTE_AIRBORNE_HEIGHT) {
            return dl;
        }
        notes_place(actor);
    }
    notes_collect(actor);

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

    // A spin is the frames forward, then back mirrored without repeating either end.
    steps = (note_frames - 1) * 2;
    if (steps == 0) {
        steps = 1;
    }
    for (i = 0; i < note_count; i++) {
        u32 step = (object_timer / NOTE_TICKS_PER_FRAME + note_trail[i].phase) % steps;
        if (note_collected & (1 << i)) {
            continue;
        }
        s32 mirrored = step >= (u32)note_frames;
        s32 frame = mirrored ? steps - step : step;

        note_billboard(&note_mtx[buffer][i], note_pos[i], note_width[frame]);
        dl = note_draw(dl, &note_mtx[buffer][i], frame, mirrored);
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

static void hud_load(void) {
    s32 count;
    s32 i;

    hud_state = NOTES_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(JIGGY_HUD_NOTE);
    if (count <= 0) {
        return;
    }
    if (count > HUD_NOTE_MAX_FRAMES) {
        count = HUD_NOTE_MAX_FRAMES;
    }
    for (i = 0; i < count; i++) {
        s32 size = jiggy_bk_sprite_frame_rgba16(JIGGY_HUD_NOTE, i, hud_note_texels[i], sizeof(hud_note_texels[0]));
        if (!hud_load_rgba16(size, &hud_note_width[i], &hud_note_height[i], HUD_NOTE_MAX_TEXELS)) {
            return;
        }
    }
    hud_note_frames = count;
    for (i = 0; i < 10; i++) {
        s32 size = jiggy_bk_bold_number_rgba16(i, hud_digit_texels[i], sizeof(hud_digit_texels[0]));
        if (!hud_load_rgba16(size, &hud_digit_width[i], &hud_digit_height[i], HUD_DIGIT_MAX_TEXELS)) {
            return;
        }
    }
    hud_state = NOTES_READY;
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

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the note
// counter, for a moment after each note.
Gfx *jiggy_notes_hud_draw(Gfx *dl) {
    s32 digits[HUD_MAX_DIGITS];
    s32 digit_count = 0;
    s32 value;
    s32 frame;
    s32 alpha;
    s32 x, y;
    s32 i;

    if (hud_ticks >= HUD_SHOW_TICKS || cc_number_of_players != 1) {
        return dl;
    }
    if (hud_state == NOTES_UNLOADED) {
        hud_load();
    }
    if (hud_state != NOTES_READY) {
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

    value = notes_total;
    do {
        digits[digit_count++] = value % 10;
        value /= 10;
    } while (value > 0 && digit_count < HUD_MAX_DIGITS);

    frame = (object_timer / HUD_TICKS_PER_FRAME) % hud_note_frames;
    x = HUD_X;
    y = D_global_asm_80744494 - hud_note_height[frame] - HUD_BOTTOM_MARGIN;

    dl = alignHUDGameplay(dl, ALIGN_LEFT);
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(dl++, 0, 0, HUD_NOTE_R, HUD_NOTE_G, HUD_NOTE_B, alpha);
    dl = hud_draw_image(dl, hud_note_texels[frame], hud_note_width[frame], hud_note_height[frame], x, y);

    // The numbers carry their own colors.
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
    x += hud_note_width[frame] + 4;
    for (i = digit_count - 1; i >= 0; i--) {
        s32 d = digits[i];
        s32 dy = y + (hud_note_height[frame] - hud_digit_height[d]) / 2;

        dl = hud_draw_image(dl, hud_digit_texels[d], hud_digit_width[d], hud_digit_height[d], x, dy);
        x += hud_digit_width[d];
    }
    gDPPipeSync(dl++);
    dl = popHUD(dl, ALIGN_LEFT);
    return dl;
}
