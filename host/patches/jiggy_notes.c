// Music Notes: Banjo-Kazooie's own collectible note (ASSET_6D6_SPRITE_MUSIC_NOTE, the sprite
// src/core2/ch/musicnote.c and the levels' sprite props use), read from the player's
// Banjo-Kazooie ROM, as a collectible of its own beside DK64's colored bananas.
//
// The sprite has 10 frames of a half turn. Like Banjo-Kazooie (cube prop animation in
// src/core2/code_A5BC0.c, which the sprite's header selects), the note plays them forward
// and then back mirrored, two ticks a frame, for a full spin, and always faces the camera.
//
// For now the notes only exist as a short trail in the Training Grounds (note_trail below),
// laid out from where the player first stands there. They are drawn right after the
// player's own model (func_global_asm_80630DCC in patches_draw.c).
// TODO: collecting a note (touch radius, the sparkle and sound, a counter and save data).

#include "common_structs.h"
#include "jiggy.h"

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

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

// Where the notes go. Placeholder layout until notes have real placements: each is a
// distance ahead of where the player first stands on the map, and a step through the spin
// so the trail doesn't turn in lockstep.
// TODO: real placements per map (world coordinates), and more maps.
typedef struct {
    f32 ahead;
    u8 phase;
} NotePlacement;

#define NOTE_TRAIL_MAP MAP_TRAINING_GROUNDS
static const NotePlacement note_trail[] = {
    {90.0f, 0}, {150.0f, 3}, {210.0f, 6}, {270.0f, 9}, {330.0f, 12},
};
#define NOTE_COUNT (sizeof(note_trail) / sizeof(note_trail[0]))

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
static Mtx note_mtx[2][NOTE_COUNT];
static s32 note_frames;
static NotesLoadState note_state = NOTES_UNLOADED;
static s32 note_last_map = -1;
static s32 note_placed;
static f32 note_pos[NOTE_COUNT][3];

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

// Lays the trail out ahead of the player, along the way they face.
static void notes_place(Actor *player) {
    f32 forward_x = func_global_asm_80612794(player->y_rotation);
    f32 forward_z = func_global_asm_80612790(player->y_rotation);
    u32 i;

    for (i = 0; i < NOTE_COUNT; i++) {
        note_pos[i][0] = player->x_position + forward_x * note_trail[i].ahead;
        note_pos[i][1] = player->y_position + NOTE_HOVER;
        note_pos[i][2] = player->z_position + forward_z * note_trail[i].ahead;
    }
    note_placed = TRUE;
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
    }
    if (current_map != NOTE_TRAIL_MAP) {
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
    for (i = 0; i < NOTE_COUNT; i++) {
        u32 step = (object_timer / NOTE_TICKS_PER_FRAME + note_trail[i].phase) % steps;
        s32 mirrored = step >= (u32)note_frames;
        s32 frame = mirrored ? steps - step : step;

        note_billboard(&note_mtx[buffer][i], note_pos[i], note_width[frame]);
        dl = note_draw(dl, &note_mtx[buffer][i], frame, mirrored);
    }

    gDPPipeSync(dl++);
    return dl;
}
