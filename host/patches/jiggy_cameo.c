// The first crossover: when the player has given us a Banjo-Kazooie ROM, Banjo's own
// dialogue portrait (ASSET_7EF_SPRITE_BANJO, straight from their ROM) fades in at the
// bottom left each time DK Isles loads, talks for a few seconds, then fades away.
//
// Nothing here is new art: the portrait is decoded from the player's ROM by the host
// (jiggy_bk_sprite_frame_rgba16) and drawn with the DK64 engine's own display list.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

Gfx *alignHUDGameplay(Gfx *dl, u8 alignment);
Gfx *popHUD(Gfx *dl, u8 alignment);

#define JIGGY_BANJO_PORTRAIT 0x7EF
#define CAMEO_MAX_FRAMES 16
#define CAMEO_MAX_TEXELS (64 * 64)
#define CAMEO_SCALE 2
// Ticks are 1/60 s (the engine's lag counter counts in 60 Hz frames).
#define CAMEO_FADE_IN_TICKS 20
#define CAMEO_TALK_TICKS 300
#define CAMEO_FADE_OUT_TICKS 30
#define CAMEO_TICKS_PER_FRAME 6
// RGBA16 texels that fit in TMEM (4 KiB) at once.
#define TMEM_RGBA16_TEXELS 2048

extern u32 D_global_asm_80744478;
extern s16 D_global_asm_80744490;
extern s16 D_global_asm_80744494;
extern u8 cc_number_of_players;

typedef enum {
    CAMEO_UNLOADED,
    CAMEO_READY,
    CAMEO_UNAVAILABLE, // No Banjo-Kazooie ROM, or the portrait didn't decode.
} CameoLoadState;

static u16 cameo_texels[CAMEO_MAX_FRAMES][CAMEO_MAX_TEXELS] __attribute__((aligned(8)));
static s16 cameo_width[CAMEO_MAX_FRAMES];
static s16 cameo_height[CAMEO_MAX_FRAMES];
static s32 cameo_frames;
static CameoLoadState cameo_state = CAMEO_UNLOADED;
static s32 cameo_last_map = -1;
static u32 cameo_ticks;
static u8 cameo_active;

static void cameo_load(void) {
    s32 count;
    s32 i;

    cameo_state = CAMEO_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(JIGGY_BANJO_PORTRAIT);
    if (count <= 0) {
        return;
    }
    if (count > CAMEO_MAX_FRAMES) {
        count = CAMEO_MAX_FRAMES;
    }
    cameo_frames = 0;
    for (i = 0; i < count; i++) {
        s32 size = jiggy_bk_sprite_frame_rgba16(JIGGY_BANJO_PORTRAIT, i, cameo_texels[cameo_frames],
                                                sizeof(cameo_texels[0]));
        s32 width = (size >> 16) & 0xFFFF;
        s32 height = size & 0xFFFF;
        // Skip frames that failed or didn't fit; the rest still animate.
        if (size <= 0 || width == 0 || height == 0 || width * height > CAMEO_MAX_TEXELS) {
            continue;
        }
        cameo_width[cameo_frames] = width;
        cameo_height[cameo_frames] = height;
        cameo_frames++;
    }
    if (cameo_frames > 0) {
        cameo_state = CAMEO_READY;
    }
}

static Gfx *cameo_draw_frame(Gfx *dl, s32 frame, s32 x, s32 y, u8 alpha) {
    s32 width = cameo_width[frame];
    s32 height = cameo_height[frame];
    s32 rows_per_load = TMEM_RGBA16_TEXELS / width;
    s32 row;

    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);

    for (row = 0; row < height; row += rows_per_load) {
        s32 rows = row + rows_per_load > height ? height - row : rows_per_load;

        gDPLoadTextureTile(dl++, &cameo_texels[frame][row * width], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, rows,
            0, 0, width - 1, rows - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(dl++,
            x << 2, (y + row * CAMEO_SCALE) << 2,
            (x + width * CAMEO_SCALE) << 2, (y + (row + rows) * CAMEO_SCALE) << 2,
            G_TX_RENDERTILE,
            0, 0, (1 << 10) / CAMEO_SCALE, (1 << 10) / CAMEO_SCALE);
    }
    gDPPipeSync(dl++);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c).
Gfx *jiggy_cameo_draw(Gfx *dl) {
    s32 frame;
    s32 x, y;
    s32 alpha;
    u32 total;

    if ((s32)current_map != cameo_last_map) {
        cameo_last_map = current_map;
        cameo_active = (current_map == MAP_DK_ISLES_OVERWORLD);
        cameo_ticks = 0;
    }
    if (!cameo_active || cc_number_of_players != 1) {
        return dl;
    }
    if (cameo_state == CAMEO_UNLOADED) {
        cameo_load();
    }
    if (cameo_state != CAMEO_READY) {
        cameo_active = FALSE;
        return dl;
    }

    total = CAMEO_FADE_IN_TICKS + CAMEO_TALK_TICKS + CAMEO_FADE_OUT_TICKS;
    cameo_ticks += D_global_asm_80744478;
    if (cameo_ticks >= total) {
        cameo_active = FALSE;
        return dl;
    }

    frame = (cameo_ticks / CAMEO_TICKS_PER_FRAME) % cameo_frames;
    // Texture rectangles can't start off-screen cleanly, so it fades rather than slides.
    x = 16;
    y = D_global_asm_80744494 - cameo_height[frame] * CAMEO_SCALE - 24;
    alpha = 0xFF;
    if (cameo_ticks < CAMEO_FADE_IN_TICKS) {
        alpha = 0xFF * (s32)cameo_ticks / CAMEO_FADE_IN_TICKS;
    } else if (cameo_ticks > CAMEO_FADE_IN_TICKS + CAMEO_TALK_TICKS) {
        alpha = 0xFF * (s32)(total - cameo_ticks) / CAMEO_FADE_OUT_TICKS;
    }

    dl = alignHUDGameplay(dl, ALIGN_LEFT);
    dl = cameo_draw_frame(dl, frame, x, y, (u8)alpha);
    dl = popHUD(dl, ALIGN_LEFT);
    return dl;
}
