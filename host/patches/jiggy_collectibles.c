// Golden Bananas as Jiggies: every Golden Banana is drawn with Banjo-Kazooie's Jiggy model
// (ASSET_35F_MODEL_JIGGY), read from the player's Banjo-Kazooie ROM. Only the look changes:
// the banana stays where it was, spins and scales as before, and is collected as before.
//
// Golden Bananas come two ways. The ones placed in a level are props (model 2 objects of type
// 0x74, or the 0x288 and 0x290 variants), drawn by func_global_asm_80636FFC in
// patches_matrix.c, which also spins them. The ones that appear as rewards (bonus barrels,
// the vulture) are ACTOR_GOLDEN_BANANA actors, drawn by func_global_asm_80630DCC in
// patches_draw.c. Both call in here in place of drawing the banana's own model.
//
// The Jiggy has no animation and one bone, so it is drawn in its rest pose
// (jiggy_bk_model_pose with animation -1). It is shiny through environment mapping, which
// uses the LookAt DK64 sets up each frame, as its own Golden Banana model does.

#include "common_structs.h"
#include "jiggy.h"

#define JIGGY_MODEL 0x35F
#define JIGGY_BLOCK_MAX 0x4000
// Jiggies drawn per frame, and the commands and matrices each pose takes (the pose of a
// one-bone model is a handful of commands and two matrices).
#define JIGGY_DRAWS_MAX 32
#define JIGGY_GFX_MAX 16
#define JIGGY_MTX_MAX 4
// The Golden Banana model is about 102 units tall and the Jiggy 78, so the Jiggy stands as
// tall as the banana it replaces.
#define JIGGY_SCALE 1.3f
// Golden Banana props, as func_global_asm_80636FFC spins them.
#define PROP_GOLDEN_BANANA 0x74
#define PROP_RAREWARE_GOLDEN_BANANA 0x288
#define PROP_GOLDEN_BANANA_2 0x290
// Reward Golden Bananas are actors spawned at scale 0.15 (func_global_asm_806A5DF0) where
// the props are placed at 0.25, and turn as fast as the props do: 10 degrees a frame.
#define ACTOR_TO_PROP_SCALE (0.25f / 0.15f)
#define SPIN_DEGREES_PER_FRAME 10

extern u8 D_global_asm_807444FC;
extern u32 object_timer;
s32 getActorMatrixTag(Actor *ac);
Gfx *simple_set_model_matrix(Gfx *dl, u8 *pushed, s32 id, s32 sub_id);
Gfx *simple_pop_model_matrix(Gfx *dl, u8 pushed);

typedef enum {
    JIGGY_UNLOADED,
    JIGGY_READY,
    JIGGY_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} JiggyLoadState;

static u8 jiggy_block[JIGGY_BLOCK_MAX] __attribute__((aligned(16)));
// Each Jiggy drawn in a frame gets its own display list and matrices, double-buffered like
// the game's own, since the frame is drawn after all of them are written.
static Gfx jiggy_gfx[2][JIGGY_DRAWS_MAX][JIGGY_GFX_MAX];
static Mtx jiggy_mtx[2][JIGGY_DRAWS_MAX][JIGGY_MTX_MAX];
static JiggyBkPose jiggy_pose;
static JiggyLoadState jiggy_state = JIGGY_UNLOADED;
static s32 jiggy_buffer = -1;
static s32 jiggy_draws;

static void jiggy_load(void) {
    s32 size;
    s32 i;

    jiggy_state = JIGGY_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(JIGGY_MODEL, jiggy_block, sizeof(jiggy_block));
    if (size > 0 && size <= (s32)sizeof(jiggy_block)) {
        jiggy_state = JIGGY_READY;
    }
    // The Jiggy has no appendages to pick.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        jiggy_pose.selectors[i] = 0;
    }
}

// Draws the Jiggy placed by `world` (the Golden Banana's transform, before JIGGY_SCALE).
// Returns FALSE, writing nothing, if it can't: then the banana's own model is drawn.
static s32 jiggy_draw(Gfx **dl, f32 world[4][4]) {
    f32 scale[4][4];
    s32 buffer = D_global_asm_807444FC;
    Gfx *gfx;

    if (jiggy_state == JIGGY_UNLOADED) {
        jiggy_load();
    }
    if (jiggy_state != JIGGY_READY) {
        return FALSE;
    }
    // A new frame starts on the other buffer.
    if (buffer != jiggy_buffer) {
        jiggy_buffer = buffer;
        jiggy_draws = 0;
    }
    if (jiggy_draws >= JIGGY_DRAWS_MAX) {
        return FALSE;
    }

    guScaleF(scale, JIGGY_SCALE, JIGGY_SCALE, JIGGY_SCALE);
    guMtxCatF(scale, world, jiggy_pose.world);
    gfx = jiggy_gfx[buffer][jiggy_draws];
    jiggy_pose.model = JIGGY_MODEL;
    jiggy_pose.block = jiggy_block;
    jiggy_pose.animation = -1;
    jiggy_pose.progress = 0.0f;
    jiggy_pose.gfx = gfx;
    jiggy_pose.gfx_capacity = JIGGY_GFX_MAX;
    jiggy_pose.mtx = jiggy_mtx[buffer][jiggy_draws];
    jiggy_pose.mtx_capacity = JIGGY_MTX_MAX;
    if (jiggy_bk_model_pose(&jiggy_pose) <= 0) {
        return FALSE;
    }
    jiggy_draws++;

    gDPPipeSync((*dl)++);
    gSPDisplayList((*dl)++, gfx);
    return TRUE;
}

static s32 is_golden_banana_prop(s32 prop_type) {
    return prop_type == PROP_GOLDEN_BANANA || prop_type == PROP_RAREWARE_GOLDEN_BANANA ||
           prop_type == PROP_GOLDEN_BANANA_2;
}

// Called in place of drawing a prop's model (func_global_asm_80636FFC in patches_matrix.c),
// with the prop's transform (scale, rotation, position). Returns TRUE if it drew a Jiggy.
s32 jiggy_golden_banana_prop_draw(Gfx **dl, s32 prop_type, f32 prop_matrix[4][4]) {
    if (!is_golden_banana_prop(prop_type)) {
        return FALSE;
    }
    return jiggy_draw(dl, prop_matrix);
}

// Called in place of drawing an actor's model (func_global_asm_80630DCC in patches_draw.c).
// Returns TRUE if it drew a Jiggy.
s32 jiggy_golden_banana_actor_draw(Gfx **dl, Actor *actor) {
    f32 world[4][4];
    f32 rotate[4][4];
    f32 scale;
    f32 yaw;
    u8 pushed = FALSE;
    Gfx *start = *dl;

    if (actor->unk58 != ACTOR_GOLDEN_BANANA) {
        return FALSE;
    }
    // Rotations are 12-bit angles. The actor turns like the props do.
    scale = actor->animation_state->scale[1] * ACTOR_TO_PROP_SCALE;
    yaw = (actor->y_rotation & 0xFFF) * (360.0f / 4096.0f) +
          (f32)((object_timer * SPIN_DEGREES_PER_FRAME) % 360);
    guScaleF(world, scale, scale, scale);
    guRotateF(rotate, yaw, 0.0f, 1.0f, 0.0f);
    guMtxCatF(world, rotate, world);
    world[3][0] = actor->x_position;
    world[3][1] = actor->y_position;
    world[3][2] = actor->z_position;

    // Tagged like the actor's own model, so it's interpolated at high frame rates.
    *dl = simple_set_model_matrix(*dl, &pushed, getActorMatrixTag(actor), 0);
    if (!jiggy_draw(dl, world)) {
        *dl = start;
        return FALSE;
    }
    *dl = simple_pop_model_matrix(*dl, pushed);
    return TRUE;
}
