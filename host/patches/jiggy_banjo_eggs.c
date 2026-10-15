// Egg Firing: while Banjo is the player and crouching (Z held), C-up has Kazooie fire a blue
// egg forward out of her mouth and C-down fires one backward out of her rear, as in
// Banjo-Kazooie (src/core2/bs/bEggHead.c, bEggAss.c). Pressing the button again during the
// move fires up to three eggs in a row, like Banjo-Kazooie does. Eggs are unlimited for now.
// TODO: an egg counter. It would hook in at egg_fire (spend one, or don't fire when empty).
//
// Each egg is one of DK's own Coconut Shooter shots (ACTOR_PROJECTILE_COCONUTS), spawned the
// way the player's gun spawns them (func_global_asm_806E225C, func_global_asm_8069084C), so
// hitting enemies, coconut switches, walls and the floor, and despawning, all work as for DK's
// coconuts. Only the look changes: the shot's own sprite is removed and Banjo-Kazooie's egg
// projectile sprite (ASSET_708_SPRITE_EGG_PROJECTILE, 10 frames of a turn,
// src/core2/code_CC1E0.c) is drawn at the shot as a camera-facing billboard. The forward egg flies straight like Banjo-Kazooie's;
// the backward egg is lobbed and falls (gravity added here, since DK's shot has none), bounces
// a few times and breaks on the ground.
//
// While the move can fire, DK64's own C-up and C-down actions (first-person view, the camera
// zoom, the Shockwave camera on Z + C-down) are kept from seeing those buttons, by masking them
// where the player's inputs are read (func_global_asm_806DFF34 below). C-right, which starts
// the Wonderwing from the same crouch (jiggy_banjo_wonderwing.c), is masked with them.
//
// jiggy_banjo3d.c calls jiggy_banjo_eggs once a frame while Banjo is on screen.

#include "common_structs.h"
#include "jiggy.h"

#define JIGGY_EGG_SPRITE 0x708
#define EGG_MAX_FRAMES 10
#define EGG_MAX_TEXELS 2048 // RGBA16 texels that fit in TMEM at once; the frames are 32x32.
// The egg's size on screen: Banjo-Kazooie grows the projectile from 20 to 50 units over its
// first frames (fxegg_head_update, fxegg_ass_update), in Banjo-Kazooie units to DK64 units
// (as jiggy_banjo3d.c scales Banjo).
#define BK_TO_DK64 0.25f
#define EGG_START_SIZE (20.0f * BK_TO_DK64)
#define EGG_FULL_SIZE (50.0f * BK_TO_DK64)
#define EGG_GROW_PER_FRAME (4.0f * BK_TO_DK64)
#define EGG_GROW_PER_FRAME_BACKWARD (8.0f * BK_TO_DK64)

// Banjo-Kazooie animation assets (asset_e): ASSET_2A_ANIM_BSEGGHEAD and ASSET_2B_ANIM_BSEGGASS,
// each played once over a second (baanim_playForDuration_onceSmooth(.., 1.0f)).
#define BANJO_ANIM_EGG_HEAD 0x2A
#define BANJO_ANIM_EGG_ASS 0x2B
#define EGG_MOVE_SECONDS 1.0f
// Points through each animation (anctrl_isAt in bsegghead_update / bseggass_update): where the
// egg comes out, and where the animation jumps back to fire another if asked for.
#define EGG_HEAD_FIRE_AT 0.4704f
#define EGG_HEAD_LOOP_AT 0.5919f
#define EGG_HEAD_LOOP_TO 0.3878f
#define EGG_ASS_FIRE_AT 0.3837f
#define EGG_ASS_LOOP_AT 0.4885f
#define EGG_ASS_LOOP_TO 0.349f
#define EGG_MAX_IN_A_ROW 3

// Where the egg starts, from the player's feet, in DK64 units: Banjo-Kazooie starts the forward
// egg 80 up and 70 ahead (fxegg_head_spawn) and the backward one 60 up (fxegg_ass_spawn).
#define EGG_HEAD_UP (80.0f * BK_TO_DK64)
#define EGG_HEAD_AHEAD (70.0f * BK_TO_DK64)
#define EGG_ASS_UP (60.0f * BK_TO_DK64)
#define EGG_ASS_BEHIND 5.0f
// Speeds, in the units DK's shots use (DK's Coconut Shooter fires at 350, D_global_asm_80753E2C).
// Banjo-Kazooie fires forward at 800 and backward at 200 out and 700 up, with gravity of 2200 a
// second (fxegg_ass_spawn); the backward egg keeps those proportions to the forward one.
#define EGG_FORWARD_SPEED 350.0f
#define EGG_BACKWARD_SPEED (EGG_FORWARD_SPEED * 200.0f / 800.0f)
#define EGG_BACKWARD_LIFT (EGG_FORWARD_SPEED * 700.0f / 800.0f)
#define EGG_GRAVITY_PER_FRAME (EGG_FORWARD_SPEED * 2200.0f / 800.0f / 30.0f)
#define EGG_BOUNCES 3
#define EGG_BOUNCE_KEEP 0.55f // Of the fall speed, after a bounce.
#define EGG_BOUNCE_SLOW 0.7f  // Of the speed along the ground, after a bounce.

// The DK64 shot each egg rides on, spawned as func_global_asm_806E225C's gun code does: its
// scale, and its flags (4 lives twice as long, which the lobbed egg needs).
#define EGG_SHOT_SCALE 0.3f
#define EGG_SHOT_FLAGS_FORWARD 0
#define EGG_SHOT_FLAGS_BACKWARD 4
// Eggs in flight, and fired eggs whose shot hasn't spawned yet (it spawns on the next pass of
// the game's actor spawners), given up on after a few frames.
#define EGG_MAX 8
#define EGG_PENDING_FRAMES 8

// DK64 player states the move can play in.
#define STATE_IDLE 0x0C
#define STATE_CROUCH_ENTER 0x3C
#define STATE_CROUCH 0x3D

// Kazooie's upper half (head and wings), and her rear, out of the backpack
// (src/core2/modelappendages.c).
static const u8 kazooie_head_selectors[] = {0x01, 0x09, 0x0C, 0x0F};
static const u8 kazooie_ass_selectors[] = {0x08, 0x0B, 0x0E, 0x11};

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u16 newly_pressed_input[];
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
// The actors that update this frame.
typedef struct {
    Actor *actor;
    s32 unk4;
} ActiveActor;
extern ActiveActor D_global_asm_807FB930[];
extern u16 D_global_asm_807FBB34; // How many.
s32 func_global_asm_8067AE0C(Actor *actor); // Whether it's one of them.
// Spawning a shot (func_global_asm_806E225C): where from, then what, and the flag that breaks it
// on its first frame (blocked by a wall).
void func_global_asm_806907F0(f32 x, f32 y, f32 z);
void func_global_asm_8069084C(s16 actor_type, s16 flags, f32 scale, s16 yaw, f32 speed, f32 y_speed, Actor *owner);
void func_global_asm_806907E0(void);
s32 func_global_asm_80672630(Actor *actor, s16 yaw, s16 arg2); // 0 if a wall is that way.
void func_global_asm_8071586C(Actor *actor); // Removes the sprites attached to an actor.
s32 getActorMatrixTag(Actor *ac);
Gfx *simple_set_model_matrix(Gfx *dl, u8 *pushed, s32 id, s32 sub_id);
Gfx *simple_pop_model_matrix(Gfx *dl, u8 pushed);
s32 jiggy_banjo_wonderwing_owns_c_right(void); // jiggy_banjo_wonderwing.c
u16 jiggy_banjo_shock_buttons(void);            // jiggy_banjo_shock.c
u16 jiggy_banjo_fly_masked_buttons(void);       // jiggy_banjo_fly.c

typedef enum {
    EGGS_UNLOADED,
    EGGS_READY,
    EGGS_UNAVAILABLE, // No Banjo-Kazooie ROM, or the sprite didn't decode.
} EggsLoadState;

typedef enum {
    EGG_MOVE_NONE,
    EGG_MOVE_HEAD, // C-up: forward, out of Kazooie's mouth.
    EGG_MOVE_ASS,  // C-down: backward.
} EggMove;

typedef struct {
    Actor *shot;
    u8 backward;
    u8 bounces;
    u8 phase;
    f32 size;
} Egg;

typedef struct {
    u8 backward;
    u32 fired_at; // object_timer
} PendingEgg;

static u16 egg_texels[EGG_MAX_FRAMES][EGG_MAX_TEXELS] __attribute__((aligned(8)));
static s16 egg_width[EGG_MAX_FRAMES];
static s16 egg_height[EGG_MAX_FRAMES];
static Vtx egg_vtx[EGG_MAX_FRAMES][4] __attribute__((aligned(8)));
static Mtx egg_mtx[2][EGG_MAX];
static s32 egg_frames;
static EggsLoadState eggs_state = EGGS_UNLOADED;

static Egg eggs[EGG_MAX];
static s32 egg_count;
static PendingEgg egg_pending[EGG_MAX];
static s32 egg_pending_count;
static s32 eggs_map = -1;
static u32 eggs_last_timer;

static EggMove egg_move;
static f32 egg_progress;
static u8 egg_requested; // Eggs asked for in this move (D_8037D2F0 in bEggHead.c).
static u8 egg_fired;     // Eggs fired so far (D_8037D2F1).
// When Banjo was last seen as the player (object_timer), for the input mask.
static u32 eggs_banjo_player_timer;
static s32 eggs_banjo_player_seen;

static void egg_set_vtx(Vtx *v, s32 x, s32 y, s32 s, s32 t) {
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

static void eggs_load(void) {
    s32 count;
    s32 i;

    eggs_state = EGGS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(JIGGY_EGG_SPRITE);
    if (count <= 0) {
        return;
    }
    if (count > EGG_MAX_FRAMES) {
        count = EGG_MAX_FRAMES;
    }
    for (i = 0; i < count; i++) {
        s32 size = jiggy_bk_sprite_frame_rgba16(JIGGY_EGG_SPRITE, i, egg_texels[i], sizeof(egg_texels[0]));
        s32 width = (size >> 16) & 0xFFFF;
        s32 height = size & 0xFFFF;
        Vtx *v = egg_vtx[i];

        if (size <= 0 || width == 0 || height == 0 || width * height > EGG_MAX_TEXELS) {
            return;
        }
        egg_width[i] = width;
        egg_height[i] = height;
        // In texels, centered on the egg, before its matrix scales it.
        egg_set_vtx(&v[0], -width / 2, height / 2, 0, 0);
        egg_set_vtx(&v[1], width / 2, height / 2, width, 0);
        egg_set_vtx(&v[2], width / 2, -height / 2, width, height);
        egg_set_vtx(&v[3], -width / 2, -height / 2, 0, height);
    }
    egg_frames = count;
    eggs_state = EGGS_READY;
}

// Fires an egg from where Banjo stands, forward or backward.
// TODO: an egg counter goes here: spend an egg, or don't fire (and play Banjo-Kazooie's empty
// sound) when there are none.
static void egg_fire(Actor *player, s32 backward) {
    s16 yaw = player->y_rotation & 0xFFF;
    f32 forward_x, forward_z;
    f32 ahead;

    if (egg_pending_count >= EGG_MAX || egg_count + egg_pending_count >= EGG_MAX) {
        return;
    }
    if (backward) {
        yaw = (yaw + 0x800) & 0xFFF;
        jiggy_bk_sfx_play(JIGGY_BK_SFX_EGG_POOP, 28000.0f / 32767.0f, 1.4f);
    } else {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_KAZOOIE_HEGH, 1.0f, 1.0f);
    }
    // Rotations are 12-bit angles, with x along sin and z along cos, as DK64 moves its actors.
    forward_x = func_global_asm_80612794(yaw);
    forward_z = func_global_asm_80612790(yaw);
    ahead = backward ? EGG_ASS_BEHIND : EGG_HEAD_AHEAD;
    // Into a wall, the shot breaks right away, as DK's own do (func_global_asm_806E225C).
    if (func_global_asm_80672630(player, yaw, 0) == 0) {
        func_global_asm_806907E0();
    }
    func_global_asm_806907F0(player->x_position + forward_x * ahead,
        player->y_position + (backward ? EGG_ASS_UP : EGG_HEAD_UP),
        player->z_position + forward_z * ahead);
    func_global_asm_8069084C(ACTOR_PROJECTILE_COCONUTS, backward ? EGG_SHOT_FLAGS_BACKWARD : EGG_SHOT_FLAGS_FORWARD,
        EGG_SHOT_SCALE, yaw, backward ? EGG_BACKWARD_SPEED : EGG_FORWARD_SPEED, backward ? EGG_BACKWARD_LIFT : 0.0f,
        player);
    egg_pending[egg_pending_count].backward = backward;
    egg_pending[egg_pending_count].fired_at = object_timer;
    egg_pending_count++;
}

static s32 egg_tracked(Actor *shot) {
    s32 i;

    for (i = 0; i < egg_count; i++) {
        if (eggs[i].shot == shot) {
            return TRUE;
        }
    }
    return FALSE;
}

// Matches fired eggs to the Coconut Shooter shots that spawned for them.
static void eggs_claim_shots(Actor *player) {
    s32 i;

    // Given up on: never spawned.
    while (egg_pending_count > 0 && object_timer - egg_pending[0].fired_at > EGG_PENDING_FRAMES) {
        for (i = 1; i < egg_pending_count; i++) {
            egg_pending[i - 1] = egg_pending[i];
        }
        egg_pending_count--;
    }
    for (i = 0; i < D_global_asm_807FBB34 && egg_pending_count > 0 && egg_count < EGG_MAX; i++) {
        Actor *shot = D_global_asm_807FB930[i].actor;
        Egg *egg;
        s32 j;

        if (shot->unk58 != ACTOR_PROJECTILE_COCONUTS || shot->unk11C != player || egg_tracked(shot)) {
            continue;
        }
        egg = &eggs[egg_count++];
        egg->shot = shot;
        egg->backward = egg_pending[0].backward;
        egg->bounces = 0;
        egg->phase = object_timer % EGG_MAX_FRAMES;
        egg->size = EGG_START_SIZE;
        for (j = 1; j < egg_pending_count; j++) {
            egg_pending[j - 1] = egg_pending[j];
        }
        egg_pending_count--;
    }
}

// Moves the eggs on: drops shots that are gone (they hit something or ran out of time), hides
// the shot's own sprite, and gives the backward egg its arc and bounces.
static void eggs_update(u32 frames) {
    s32 i;

    for (i = 0; i < egg_count;) {
        Egg *egg = &eggs[i];
        Actor *shot = egg->shot;
        f32 step;

        if (!func_global_asm_8067AE0C(shot) || shot->unk58 != ACTOR_PROJECTILE_COCONUTS) {
            // It broke (SFX_D_EGGSHELL_BREAKING, at Banjo-Kazooie's slightly random pitch).
            jiggy_bk_sfx_play(JIGGY_BK_SFX_EGGSHELL_BREAK, 13000.0f / 32767.0f, 1.6f + 0.01f * (object_timer % 11));
            *egg = eggs[--egg_count];
            continue;
        }
        // The shot's coconut sprite is attached on its first update; it's ours to draw instead.
        func_global_asm_8071586C(shot);
        egg->size += (egg->backward ? EGG_GROW_PER_FRAME_BACKWARD : EGG_GROW_PER_FRAME) * frames;
        if (egg->size > EGG_FULL_SIZE) {
            egg->size = EGG_FULL_SIZE;
        }
        if (egg->backward) {
            shot->y_velocity -= EGG_GRAVITY_PER_FRAME * frames;
            // How far the shot moves for its speed in a frame (func_global_asm_806651FC). Just
            // before it would reach the floor, it bounces instead; once out of bounces it lands
            // and breaks.
            step = (shot->animation_state != NULL ? shot->animation_state->scale_y : 0.15f) * 0.16666666675f;
            if (shot->y_velocity < 0.0f && egg->bounces < EGG_BOUNCES &&
                shot->y_position + shot->y_velocity * step <= shot->floor + 1.0f &&
                shot->y_position - shot->floor < EGG_FULL_SIZE * 4.0f) {
                shot->y_velocity = -shot->y_velocity * EGG_BOUNCE_KEEP;
                shot->unkB8 *= EGG_BOUNCE_SLOW;
                egg->bounces++;
            }
        }
        i++;
    }
}

// A matrix that stands the egg at pos facing the camera, `size` units across for a frame `width`
// texels wide (as jiggy_notes.c stands its notes).
static void egg_billboard(Mtx *m, f32 pos[3], f32 size, s32 width) {
    CharacterChange *cc = &character_change_array[cc_player_index];
    f32 mf[4][4];
    f32 f[3], r[3], u[3];
    f32 length;
    f32 scale = size / width;

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

static Gfx *eggs_draw(Gfx *dl) {
    s32 buffer = D_global_asm_807444FC;
    s32 i;

    if (egg_count == 0) {
        return dl;
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

    for (i = 0; i < egg_count; i++) {
        Egg *egg = &eggs[i];
        f32 pos[3];
        u8 pushed = FALSE;
        // Banjo-Kazooie plays the turn from the last frame back, a frame a tick (D_803726A0).
        s32 frame = egg_frames - 1 - (s32)((object_timer + egg->phase) % egg_frames);
        s32 width = egg_width[frame];
        s32 height = egg_height[frame];

        pos[0] = egg->shot->x_position;
        pos[1] = egg->shot->y_position;
        pos[2] = egg->shot->z_position;
        egg_billboard(&egg_mtx[buffer][i], pos, egg->size, width);
        // Tagged like the shot, so it's interpolated at high frame rates.
        dl = simple_set_model_matrix(dl, &pushed, getActorMatrixTag(egg->shot), 0);
        gSPMatrix(dl++, &egg_mtx[buffer][i], G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPLoadTextureTile(dl++, egg_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, height,
            0, 0, width - 1, height - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(dl++, egg_vtx[frame], 4, 0);
        gSP2Triangles(dl++, 0, 3, 2, 0, 0, 2, 1, 0);
        dl = simple_pop_model_matrix(dl, pushed);
    }
    gDPPipeSync(dl++);
    return dl;
}

static s32 egg_crossed(f32 before, f32 after, f32 at) {
    return before < at && after >= at;
}

// Banjo-Kazooie's egg moves (bsegghead_update, bseggass_update), when Banjo is the player.
static void egg_move_update(Actor *player, f32 seconds) {
    u16 pressed = newly_pressed_input[0];
    u8 state = player->control_state;
    s32 crouching = state == STATE_CROUCH_ENTER || state == STATE_CROUCH;
    u16 button;
    f32 before;
    f32 fire_at, loop_at, loop_to;

    if (egg_move == EGG_MOVE_NONE) {
        if (!crouching) {
            return;
        }
        if (pressed & U_CBUTTONS) {
            egg_move = EGG_MOVE_HEAD;
        } else if (pressed & D_CBUTTONS) {
            egg_move = EGG_MOVE_ASS;
        } else {
            return;
        }
        egg_progress = 0.0f;
        egg_requested = 1;
        egg_fired = 1;
        return;
    }
    // Jumping, walking off or anything else DK does cuts the move short.
    if (!crouching && state != STATE_IDLE) {
        egg_move = EGG_MOVE_NONE;
        return;
    }
    if (egg_move == EGG_MOVE_HEAD) {
        button = U_CBUTTONS;
        fire_at = EGG_HEAD_FIRE_AT;
        loop_at = EGG_HEAD_LOOP_AT;
        loop_to = EGG_HEAD_LOOP_TO;
    } else {
        button = D_CBUTTONS;
        fire_at = EGG_ASS_FIRE_AT;
        loop_at = EGG_ASS_LOOP_AT;
        loop_to = EGG_ASS_LOOP_TO;
    }
    if ((pressed & button) && egg_requested < EGG_MAX_IN_A_ROW) {
        egg_requested++;
    }
    before = egg_progress;
    egg_progress += seconds / EGG_MOVE_SECONDS;
    if (egg_crossed(before, egg_progress, fire_at)) {
        egg_fire(player, egg_move == EGG_MOVE_ASS);
    }
    if (egg_crossed(before, egg_progress, loop_at) && egg_fired < egg_requested) {
        egg_progress = loop_to;
        egg_fired++;
    }
    if (egg_progress >= 1.0f) {
        egg_move = EGG_MOVE_NONE;
    }
}

// Called once a frame from jiggy_banjo_draw (jiggy_banjo3d.c) while Banjo is on screen, after
// his animation and selectors are picked. Fires and moves the eggs, draws them, and while an
// egg move plays, puts its animation on Banjo and Kazooie out of the backpack.
Gfx *jiggy_banjo_eggs(Gfx *dl, Actor *player, s32 banjo_is_player, s32 *animation, f32 *progress, s32 *selectors) {
    u32 frames = object_timer - eggs_last_timer;
    const u8 *kazooie;
    s32 i;

    eggs_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    if ((s32)current_map != eggs_map) {
        // The map's actors are gone.
        eggs_map = current_map;
        egg_count = 0;
        egg_pending_count = 0;
        egg_move = EGG_MOVE_NONE;
    }
    if (eggs_state == EGGS_UNLOADED) {
        eggs_load();
    }
    if (eggs_state != EGGS_READY) {
        return dl;
    }
    eggs_banjo_player_seen = banjo_is_player;
    eggs_banjo_player_timer = object_timer;

    if (banjo_is_player) {
        egg_move_update(player, frames / 30.0f);
    } else {
        egg_move = EGG_MOVE_NONE;
    }
    eggs_claim_shots(player);
    eggs_update(frames);

    if (egg_move != EGG_MOVE_NONE) {
        *animation = (egg_move == EGG_MOVE_HEAD) ? BANJO_ANIM_EGG_HEAD : BANJO_ANIM_EGG_ASS;
        *progress = egg_progress < 0.99f ? egg_progress : 0.99f;
        kazooie = (egg_move == EGG_MOVE_HEAD) ? kazooie_head_selectors : kazooie_ass_selectors;
        for (i = 0; i < 4; i++) {
            selectors[kazooie[i]] = 1;
        }
    }
    return eggs_draw(dl);
}

// Whether C-up and C-down belong to the egg moves right now (and C-right to the Wonderwing):
// Banjo is the player and crouching, or firing.
static s32 eggs_own_c_buttons(void) {
    if (cc_number_of_players != 1 || cc_player_index != 0 || !eggs_banjo_player_seen || gPlayerPointer == NULL ||
        object_timer - eggs_banjo_player_timer > 2) {
        return FALSE;
    }
    return egg_move != EGG_MOVE_NONE || gPlayerPointer->control_state == STATE_CROUCH_ENTER ||
           gPlayerPointer->control_state == STATE_CROUCH;
}

// @jiggy: the player's buttons, held and newly pressed, go through here (remapped by the table
// at arg1) before the control state's input handlers see them (handleInputsForControlState).
// Without C-up and C-down there, DK64's own actions for them (first-person view, camera zoom,
// Shockwave camera) stay out of the way of Egg Firing, and without C-right DK64's camera turn
// stays out of the way of the Wonderwing. Both read the raw buttons. The Shock Spring Jump
// hands DK an A press here for its spring, and Flight keeps A, B and Z from DK while flying.
RECOMP_PATCH u16 func_global_asm_806DFF34(u16 arg0, u16 *arg1) {
    u16 i;
    u16 j;
    u16 buttons;

    j = 0;
    i = 0;
    while (*arg1) {
        j |= *arg1;
        if (*arg1++ & arg0) {
            i |= *arg1;
        }
        arg1++;
    }
    buttons = i | (arg0 & ~j);
    if (eggs_own_c_buttons()) {
        buttons &= ~(U_CBUTTONS | D_CBUTTONS | R_CBUTTONS);
    } else if (jiggy_banjo_wonderwing_owns_c_right()) {
        buttons &= ~R_CBUTTONS;
    }
    buttons &= ~jiggy_banjo_fly_masked_buttons();
    return buttons | jiggy_banjo_shock_buttons();
}
