#ifndef __JIGGY_H__
#define __JIGGY_H__

// Banjo-Kazooie assets from the player's ROM, served by the host (src/game/jiggy_api.cpp).
// Asset ids are the Banjo-Kazooie decompilation's asset_e values.

#include "patch_helpers.h"

// 1 if the player has given us a Banjo-Kazooie ROM, 0 otherwise.
DECLARE_FUNC(s32, jiggy_bk_available);
// Decompressed size of an asset, or -1 if it doesn't exist.
DECLARE_FUNC(s32, jiggy_bk_asset_size, u32 asset_id);
// Copies an asset into dst. Returns the bytes copied, or -1 if it is missing or doesn't fit.
DECLARE_FUNC(s32, jiggy_bk_asset_read, u32 asset_id, void* dst, u32 dst_len);
// Frames in a sprite asset (e.g. a dialogue portrait), or -1 if the asset isn't a sprite.
DECLARE_FUNC(s32, jiggy_bk_sprite_frame_count, u32 asset_id);
// Decodes a sprite frame to RGBA16 texels, width padded to a multiple of 4. Returns
// (width << 16) | height, or -1. Pass dst NULL to get the size without writing.
DECLARE_FUNC(s32, jiggy_bk_sprite_frame_rgba16, u32 asset_id, u32 frame, void* dst, u32 dst_len);
// Converts a model to a block DK64 can draw, built to live at dst (8-byte aligned): its
// F3DEX2 display lists, vertices, textures and render modes. Returns the size, writing only
// if it fits (pass dst NULL to ask), or -1 if it isn't a model. Draw it with
// jiggy_bk_model_pose.
DECLARE_FUNC(s32, jiggy_bk_model_build, u32 asset_id, void* dst, u32 dst_len);

// Banjo-Kazooie's appendage selectors (modelRender_setAppendageVisibility).
#define JIGGY_BK_SELECTOR_COUNT 0x2A

#ifndef __cplusplus
// A request to draw a built model in a pose. Matches the offsets in jiggy_api.cpp.
typedef struct {
    /* 0x00 */ s32 model;       // Asset id given to jiggy_bk_model_build.
    /* 0x04 */ void *block;     // Where it was built.
    /* 0x08 */ s32 animation;   // Animation asset id, or -1 for the rest pose.
    /* 0x0C */ f32 progress;    // 0 to 1 through the animation.
    /* 0x10 */ f32 world[4][4]; // Where the model stands (MtxF, as guMtxCatF builds).
    /* 0x50 */ s32 selectors[JIGGY_BK_SELECTOR_COUNT]; // 0 hides, n draws branch n.
    /* 0xF8 */ Gfx *gfx;        // Display list to write (ends with gSPEndDisplayList).
    /* 0xFC */ u32 gfx_capacity; // In commands.
    /* 0x100 */ Mtx *mtx;       // Matrices it loads: the root, then one per bone.
    /* 0x104 */ u32 mtx_capacity;
} JiggyBkPose;
#endif

// Fills pose->gfx and pose->mtx. Returns the commands written, or -1 (model not built at
// pose->block, or the buffers are too small). Call gSPDisplayList on pose->gfx after.
DECLARE_FUNC(s32, jiggy_bk_model_pose, void *pose); // A JiggyBkPose.

#endif
