// Banjo in 3D: Banjo & Kazooie's own model (MODEL_BANJOKAZOOIE_HIGH_POLY), read from the
// player's Banjo-Kazooie ROM, walks beside the player in DK Isles and the Training Grounds
// outside DK's house, with Banjo-Kazooie's own idle, walk, run and jump animations.
//
// The host converts the model once (jiggy_bk_model_build) and, every frame, poses it with
// an animation and writes the display list and bone matrices that draw it
// (jiggy_bk_model_pose, see bridge/include/jiggy/bk_pose.hpp). This file picks the
// animation from how the player moves and draws Banjo right after the player's own model.

#include "common_structs.h"
#include "jiggy.h"

#define JIGGY_BANJO_MODEL 0x34E
#define BANJO_BLOCK_MAX 0x40000
#define BANJO_GFX_MAX 256
#define BANJO_MTX_MAX 72
// Banjo-Kazooie units to DK64 world units, and where Banjo stands relative to the player.
#define BANJO_SCALE 0.25f
#define BANJO_SIDE_OFFSET 70.0f

// Banjo-Kazooie animation assets (asset_e) and how long one loop takes, in seconds.
#define BANJO_ANIM_IDLE 0x6F
#define BANJO_ANIM_WALK 0x03
#define BANJO_ANIM_RUN 0x0C
#define BANJO_ANIM_JUMP 0x08
#define BANJO_IDLE_SECONDS 5.5f
#define BANJO_JUMP_SECONDS 1.9f
// Player speeds where Banjo starts walking and running, and the distance one walk or run
// loop covers, in DK64 units.
#define BANJO_WALK_SPEED 8.0f
#define BANJO_RUN_SPEED 140.0f
#define BANJO_WALK_STRIDE 70.0f
#define BANJO_RUN_STRIDE 140.0f
// Height above the floor that counts as in the air.
#define BANJO_AIRBORNE_HEIGHT 6.0f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// Banjo-Kazooie's appendage selectors for this model (src/core2/modelappendages.c).
#define SEL_TRAINERS_FIRST 0x12
#define SEL_TRAINERS_LAST 0x19
#define SEL_EYES_FIRST 0x1A
#define SEL_EYES_LAST 0x21
#define SEL_BOOTS_FIRST 0x22
#define SEL_BOOTS_LAST 0x29
// A blink every few seconds, through the eye frames (1 is open, 8 closed).
#define BLINK_EVERY_FRAMES 110
static const u8 blink_frames[] = {3, 6, 8, 8, 6, 3};

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    BANJO_UNLOADED,
    BANJO_READY,
    BANJO_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} BanjoLoadState;

static u8 banjo_block[BANJO_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx banjo_gfx[2][BANJO_GFX_MAX];
static Mtx banjo_mtx[2][BANJO_MTX_MAX];
static JiggyBkPose banjo_pose;
static BanjoLoadState banjo_state = BANJO_UNLOADED;
static s32 banjo_animation = -1;
static f32 banjo_progress;
static u32 banjo_last_timer;

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

static void banjo_play(s32 animation) {
    if (banjo_animation != animation) {
        banjo_animation = animation;
        banjo_progress = 0.0f;
    }
}

// Picks Banjo's animation from how the player moves and advances it.
static void banjo_animate(Actor *player) {
    f32 seconds;
    f32 speed = player->unkB8;
    u32 frames = object_timer - banjo_last_timer;

    banjo_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    seconds = frames / GAME_FPS;

    if (player->y_position - player->floor > BANJO_AIRBORNE_HEIGHT) {
        banjo_play(BANJO_ANIM_JUMP);
        // Plays once, holding the falling pose.
        banjo_progress += seconds / BANJO_JUMP_SECONDS;
        if (banjo_progress > 0.95f) {
            banjo_progress = 0.95f;
        }
        return;
    }
    if (speed < BANJO_WALK_SPEED) {
        banjo_play(BANJO_ANIM_IDLE);
        banjo_progress += seconds / BANJO_IDLE_SECONDS;
    } else if (speed < BANJO_RUN_SPEED) {
        banjo_play(BANJO_ANIM_WALK);
        banjo_progress += seconds * speed / BANJO_WALK_STRIDE;
    } else {
        banjo_play(BANJO_ANIM_RUN);
        banjo_progress += seconds * speed / BANJO_RUN_STRIDE;
    }
    while (banjo_progress >= 1.0f) {
        banjo_progress -= 1.0f;
    }
}

static void banjo_set_selectors(void) {
    s32 i;
    s32 eyes = 1;
    u32 blink = object_timer % BLINK_EVERY_FRAMES;

    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        banjo_pose.selectors[i] = 0;
    }
    // Normal feet rather than the Turbo Talon Trainers or Wading Boots; Kazooie stays in the
    // backpack.
    for (i = SEL_TRAINERS_FIRST; i <= SEL_TRAINERS_LAST; i++) {
        banjo_pose.selectors[i] = 1;
    }
    for (i = SEL_BOOTS_FIRST; i <= SEL_BOOTS_LAST; i++) {
        banjo_pose.selectors[i] = 1;
    }
    if (blink < sizeof(blink_frames)) {
        eyes = blink_frames[blink];
    }
    for (i = SEL_EYES_FIRST; i <= SEL_EYES_LAST; i++) {
        banjo_pose.selectors[i] = eyes;
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c).
Gfx *jiggy_banjo_draw(Gfx *dl, Actor *actor) {
    f32 yaw;
    f32 x, z;
    f32 rotate[4][4];
    f32 scale[4][4];
    s32 buffer = D_global_asm_807444FC;

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
    banjo_animate(actor);
    banjo_set_selectors();

    // Rotations are 12-bit angles. Banjo faces where the player faces, one step to their side.
    yaw = (actor->y_rotation & 0xFFF) * (360.0f / 4096.0f);
    x = actor->x_position + func_global_asm_80612790(actor->y_rotation) * BANJO_SIDE_OFFSET;
    z = actor->z_position - func_global_asm_80612794(actor->y_rotation) * BANJO_SIDE_OFFSET;
    guScaleF(scale, BANJO_SCALE, BANJO_SCALE, BANJO_SCALE);
    guRotateF(rotate, yaw, 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, banjo_pose.world);
    banjo_pose.world[3][0] = x;
    banjo_pose.world[3][1] = actor->y_position;
    banjo_pose.world[3][2] = z;

    banjo_pose.model = JIGGY_BANJO_MODEL;
    banjo_pose.block = banjo_block;
    banjo_pose.animation = banjo_animation;
    banjo_pose.progress = banjo_progress;
    banjo_pose.gfx = banjo_gfx[buffer];
    banjo_pose.gfx_capacity = BANJO_GFX_MAX;
    banjo_pose.mtx = banjo_mtx[buffer];
    banjo_pose.mtx_capacity = BANJO_MTX_MAX;
    if (jiggy_bk_model_pose(&banjo_pose) <= 0) {
        return dl;
    }

    gDPPipeSync(dl++);
    gSPDisplayList(dl++, banjo_gfx[buffer]);
    return dl;
}
