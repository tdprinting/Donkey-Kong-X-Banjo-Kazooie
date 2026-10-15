// Banjo in 3D: Banjo & Kazooie's own model (MODEL_BANJOKAZOOIE_HIGH_POLY), read from the
// player's Banjo-Kazooie ROM, stands beside the player in DK Isles and the Training Grounds
// outside DK's house.
//
// The host converts the model to an F3DEX2 display list (jiggy_bk_model_build, see
// bridge/include/jiggy/bk_gfx.hpp); this file places it in the world and draws it right
// after the player's own model. It's in its rest pose for now: animation comes later.

#include "common_structs.h"
#include "jiggy.h"

#define JIGGY_BANJO_MODEL 0x34E
#define BANJO_BLOCK_MAX 0x40000
// Banjo-Kazooie units to DK64 world units, and where Banjo stands relative to the player.
#define BANJO_SCALE 0.25f
#define BANJO_SIDE_OFFSET 40.0f

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    BANJO_UNLOADED,
    BANJO_READY,
    BANJO_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} BanjoLoadState;

static u8 banjo_block[BANJO_BLOCK_MAX] __attribute__((aligned(16)));
static Mtx banjo_mtx[2];
static BanjoLoadState banjo_state = BANJO_UNLOADED;

static void banjo_load(void) {
    s32 size;

    banjo_state = BANJO_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(JIGGY_BANJO_MODEL, banjo_block, sizeof(banjo_block));
    if (size > 0 && size <= (s32)sizeof(banjo_block)) {
        banjo_state = BANJO_READY;
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c).
Gfx *jiggy_banjo_draw(Gfx *dl, Actor *actor) {
    f32 yaw;
    f32 x, z;
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 place[4][4];

    if (actor != gPlayerPointer || cc_number_of_players != 1 ||
        (current_map != MAP_DK_ISLES_OVERWORLD && current_map != MAP_TRAINING_GROUNDS)) {
        return dl;
    }
    if (banjo_state == BANJO_UNLOADED) {
        banjo_load();
    }
    if (banjo_state != BANJO_READY) {
        return dl;
    }

    // Rotations are 12-bit angles. Banjo faces where the player faces, one step to their side.
    yaw = (actor->y_rotation & 0xFFF) * (360.0f / 4096.0f);
    x = actor->x_position + func_global_asm_80612790(actor->y_rotation) * BANJO_SIDE_OFFSET;
    z = actor->z_position - func_global_asm_80612794(actor->y_rotation) * BANJO_SIDE_OFFSET;
    guScaleF(scale, BANJO_SCALE, BANJO_SCALE, BANJO_SCALE);
    guRotateF(rotate, yaw, 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, place);
    place[3][0] = x;
    place[3][1] = actor->y_position;
    place[3][2] = z;
    guMtxF2L(place, &banjo_mtx[D_global_asm_807444FC]);

    gDPPipeSync(dl++);
    gSPMatrix(dl++, &banjo_mtx[D_global_asm_807444FC], G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPDisplayList(dl++, banjo_block);
    gSPPopMatrix(dl++, G_MTX_MODELVIEW);
    return dl;
}
