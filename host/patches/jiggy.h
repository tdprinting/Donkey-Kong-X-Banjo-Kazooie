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

#endif
