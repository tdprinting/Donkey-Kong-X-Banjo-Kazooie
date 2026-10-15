// Banjo in 3D: Banjo & Kazooie's own model (MODEL_BANJOKAZOOIE_HIGH_POLY), read from the
// player's Banjo-Kazooie ROM, walks beside the player in DK Isles and the Training Grounds
// outside DK's house, with Banjo-Kazooie's own idle, walk, run and jump animations.
// D-pad left swaps who is on screen: Banjo takes the player's place and DK steps out, until
// it's pressed again. As the player, Banjo shows his own move for what DK's controls do
// (Claw Swipe, Beak Barge, crouch, backflip) and has Banjo-Kazooie's air moves: press A
// again in the air and hold it to Feathery Flap, B for the Rat-a-tat Rap, Z for the Beak
// Buster. Crouched, C-left Talon Trots (jiggy_banjo_trot.c), C-right Wonderwings
// (jiggy_banjo_wonderwing.c) and C-up and C-down fire eggs (jiggy_banjo_eggs.c); A on the
// Shock Spring Jump pad springs (jiggy_banjo_shock.c) and A on the Flight pad flies
// (jiggy_banjo_fly.c). In the water Banjo swims as in Banjo-Kazooie (jiggy_banjo_swim.c).
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
#define BANJO_ANIM_FALL 0xB0
#define BANJO_ANIM_CLAW_SWIPE 0x05
#define BANJO_ANIM_BEAK_BARGE 0x1C
#define BANJO_ANIM_CROUCH_ENTER 0x01
#define BANJO_ANIM_CROUCH 0x10C
#define BANJO_ANIM_BACKFLIP 0x4B
#define BANJO_ANIM_FLAP 0x17
#define BANJO_ANIM_RAT_A_TAT_RAP 0x1A
#define BANJO_ANIM_BEAK_BUSTER 0x1D
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

// DK64 player states (Actor control_state) that have a Banjo-Kazooie move to show.
#define STATE_JUMP 0x17
#define STATE_FALL 0x1E
#define STATE_ATTACK 0x26
#define STATE_RUNNING_ATTACK 0x29
#define STATE_CROUCH_ENTER 0x3C
#define STATE_CROUCH 0x3D
#define STATE_BACKFLIP 0x3E

// Feathery Flap (src/core2/bs/bFlap.c): press A again in the air and hold it to flap,
// slowing the fall, for a while. In DK64 units per second; the game adds a frame of
// gravity after the clamp, so the fall settles at about -30.
#define FLAP_LIFT 120.0f
#define FLAP_FALL_SPEED -10.0f
#define FLAP_MAX_SECONDS 1.6f
#define FLAP_SECONDS 0.35f
// Kazooie grunts on each wing beat, at this point of it, once a flap has gone on this long.
#define FLAP_GRUNT_AT 0.5698f
#define FLAP_GRUNT_AFTER 0.7f
// Rat-a-tat Rap (bPeck.c): B in the air hovers while Kazooie pecks.
#define RAP_SECONDS 0.6f
#define RAP_PECK_SECONDS 0.2f
// Beak Buster (bbuster.c): Z in the air hangs for a moment, then slams straight down.
#define BUSTER_HANG_SECONDS 0.2f
#define BUSTER_SECONDS 1.02f
#define BUSTER_SLAM_SPEED -600.0f

// Banjo-Kazooie's appendage selectors for this model (src/core2/modelappendages.c).
#define SEL_TRAINERS_FIRST 0x12
#define SEL_TRAINERS_LAST 0x19
#define SEL_EYES_FIRST 0x1A
#define SEL_EYES_LAST 0x21
#define SEL_BOOTS_FIRST 0x22
#define SEL_BOOTS_LAST 0x29
// Kazooie's upper half (head and wings), out of the backpack.
static const u8 kazooie_selectors[] = {0x01, 0x09, 0x0C, 0x0F};
// A blink every few seconds, through the eye frames (1 is open, 8 closed).
#define BLINK_EVERY_FRAMES 110
static const u8 blink_frames[] = {3, 6, 8, 8, 6, 3};

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u16 newly_pressed_input[];
extern OSContPad D_global_asm_807ECD58; // Buttons held this frame.
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
// Talon Trot (jiggy_banjo_trot.c).
s32 jiggy_banjo_trot_animate(Actor *player, s32 is_player, s32 airborne, f32 seconds, s32 *animation, f32 *progress);
void jiggy_banjo_trot_selectors(s32 *selectors);
f32 jiggy_banjo_trot_yaw(void);
// Wonderwing (jiggy_banjo_wonderwing.c).
s32 jiggy_banjo_wonderwing_animate(Actor *player, s32 is_player, s32 airborne, f32 seconds, s32 *animation, f32 *progress);
void jiggy_banjo_wonderwing_selectors(s32 *selectors);
Gfx *jiggy_banjo_wonderwing_draw(Gfx *dl, Actor *player);
// Shock Spring Jump (jiggy_banjo_shock.c).
s32 jiggy_banjo_shock_animate(Actor *player, s32 is_player, s32 airborne, f32 seconds, s32 *animation, f32 *progress);
void jiggy_banjo_shock_selectors(s32 *selectors);
f32 jiggy_banjo_shock_lift(Actor *player);
Gfx *jiggy_banjo_shock_pad_draw(Gfx *dl, Actor *player);
// Flight (jiggy_banjo_fly.c).
s32 jiggy_banjo_fly_animate(Actor *player, s32 is_player, s32 airborne, f32 seconds, s32 *animation, f32 *progress);
void jiggy_banjo_fly_selectors(s32 *selectors);
f32 jiggy_banjo_fly_pitch(void);
f32 jiggy_banjo_fly_roll(void);
f32 jiggy_banjo_fly_lift(Actor *player);
Gfx *jiggy_banjo_fly_pad_draw(Gfx *dl, Actor *player);
// Swimming (jiggy_banjo_swim.c).
s32 jiggy_banjo_swimming(Actor *player);
s32 jiggy_banjo_swim_animate(Actor *player, s32 is_player, f32 seconds, s32 *animation, f32 *progress);
void jiggy_banjo_swim_selectors(s32 *selectors);
f32 jiggy_banjo_swim_lift(void);

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
static s32 banjo_is_player;
// Banjo-Kazooie air moves, at most one of each per jump.
typedef enum {
    AIR_MOVE_NONE,
    AIR_MOVE_FLAP,
    AIR_MOVE_RAP,
    AIR_MOVE_BUSTER,
} BanjoAirMove;

static BanjoAirMove banjo_air_move;
static u8 banjo_air_moves_used; // Bits: 1 << BanjoAirMove.
static f32 banjo_air_move_seconds;

// The Banjo-Kazooie move shown for a DK64 player state, played once over `seconds`.
typedef struct {
    u8 state;
    s16 animation;
    f32 seconds;
} BanjoMove;

static const BanjoMove banjo_moves[] = {
    {STATE_ATTACK, BANJO_ANIM_CLAW_SWIPE, 0.8f},
    {STATE_RUNNING_ATTACK, BANJO_ANIM_BEAK_BARGE, 0.8f},
    {STATE_CROUCH_ENTER, BANJO_ANIM_CROUCH_ENTER, 0.5f},
    {STATE_BACKFLIP, BANJO_ANIM_BACKFLIP, 1.4f},
};

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

// Plays an animation once, holding its last pose.
static void banjo_play_once(s32 animation, f32 seconds, f32 elapsed) {
    banjo_play(animation);
    banjo_progress += elapsed / seconds;
    if (banjo_progress > 0.99f) {
        banjo_progress = 0.99f;
    }
}

static s32 flap_sound; // The flap sound playing (jiggy_bk_sfx_play's handle), or 0.

static void banjo_start_air_move(Actor *player, BanjoAirMove move) {
    banjo_air_move = move;
    banjo_air_moves_used |= 1 << move;
    banjo_air_move_seconds = 0.0f;
    if (move == AIR_MOVE_FLAP) {
        player->y_velocity = FLAP_LIFT;
        // Kazooie's flap (SFX_4E_KAZOOIE_BRUIII); a new one cuts the last, as Banjo-Kazooie
        // plays both on one channel.
        jiggy_bk_sound_stop(flap_sound);
        flap_sound = jiggy_bk_sfx_play(JIGGY_BK_SFX_FLAP, 20000.0f / 32767.0f, 1.24f);
    }
}

// Banjo-Kazooie's air moves, when Banjo is the player: a second press of A starts a
// Feathery Flap (lasting while A is held), B a Rat-a-tat Rap and Z a Beak Buster.
static void banjo_air_moves(Actor *player, s32 airborne, f32 seconds) {
    u16 pressed = newly_pressed_input[0];

    if (!airborne || !banjo_is_player) {
        // A Beak Buster plays out its landing unless the player jumps again.
        if (airborne || !banjo_is_player || banjo_air_move != AIR_MOVE_BUSTER || banjo_progress >= 0.99f ||
            player->control_state == STATE_JUMP) {
            banjo_air_move = AIR_MOVE_NONE;
        }
        banjo_air_moves_used = 0;
        return;
    }
    // The Beak Buster can cut a flap or rap short; nothing interrupts it.
    if ((pressed & Z_TRIG) && banjo_air_move != AIR_MOVE_BUSTER && !(banjo_air_moves_used & (1 << AIR_MOVE_BUSTER))) {
        banjo_start_air_move(player, AIR_MOVE_BUSTER);
    } else if (banjo_air_move == AIR_MOVE_NONE) {
        if ((pressed & A_BUTTON) && !(banjo_air_moves_used & (1 << AIR_MOVE_FLAP))) {
            banjo_start_air_move(player, AIR_MOVE_FLAP);
        } else if ((pressed & B_BUTTON) && !(banjo_air_moves_used & (1 << AIR_MOVE_RAP))) {
            banjo_start_air_move(player, AIR_MOVE_RAP);
        }
    }
    banjo_air_move_seconds += seconds;
    switch (banjo_air_move) {
        case AIR_MOVE_FLAP:
            if (!(D_global_asm_807ECD58.button & A_BUTTON) || banjo_air_move_seconds > FLAP_MAX_SECONDS) {
                banjo_air_move = AIR_MOVE_NONE;
            } else if (player->y_velocity < FLAP_FALL_SPEED) {
                player->y_velocity = FLAP_FALL_SPEED;
            }
            break;
        case AIR_MOVE_RAP:
            if (banjo_air_move_seconds > RAP_SECONDS) {
                banjo_air_move = AIR_MOVE_NONE;
            } else if (player->y_velocity < 0.0f) {
                player->y_velocity = 0.0f;
            }
            break;
        case AIR_MOVE_BUSTER:
            // Hangs, then falls until landing ends it.
            player->y_velocity = (banjo_air_move_seconds < BUSTER_HANG_SECONDS) ? 0.0f : BUSTER_SLAM_SPEED;
            break;
        default:
            break;
    }
}

// Picks Banjo's animation from what the player is doing and advances it.
static void banjo_animate(Actor *player) {
    f32 seconds;
    f32 before;
    f32 speed = player->unkB8;
    u32 frames = object_timer - banjo_last_timer;
    s32 airborne = player->y_position - player->floor > BANJO_AIRBORNE_HEIGHT;
    // In the water Banjo only swims: his other moves stop as they do when DK takes over.
    s32 moves_on = banjo_is_player && !jiggy_banjo_swimming(player);
    s32 i;

    banjo_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    seconds = frames / GAME_FPS;

    if (jiggy_banjo_trot_animate(player, moves_on, airborne, seconds, &banjo_animation, &banjo_progress) ||
        jiggy_banjo_wonderwing_animate(player, moves_on, airborne, seconds, &banjo_animation, &banjo_progress) ||
        jiggy_banjo_shock_animate(player, moves_on, airborne, seconds, &banjo_animation, &banjo_progress) ||
        jiggy_banjo_fly_animate(player, moves_on, airborne, seconds, &banjo_animation, &banjo_progress)) {
        return;
    }
    if (jiggy_banjo_swim_animate(player, banjo_is_player, seconds, &banjo_animation, &banjo_progress)) {
        // No air moves in the water: A and B swim, and DK64 counts it as off the floor.
        banjo_air_move = AIR_MOVE_NONE;
        banjo_air_moves_used = 0;
        return;
    }
    banjo_air_moves(player, airborne, seconds);
    switch (banjo_air_move) {
        case AIR_MOVE_FLAP:
            banjo_play(BANJO_ANIM_FLAP);
            before = banjo_progress;
            banjo_progress += seconds / FLAP_SECONDS;
            // Once the flap has gone on a while, Kazooie grunts on each beat (SFX_47_KAZOOIE_HEUGH).
            if (banjo_air_move_seconds > FLAP_GRUNT_AFTER && before < FLAP_GRUNT_AT && banjo_progress >= FLAP_GRUNT_AT) {
                jiggy_bk_sound_stop(flap_sound);
                flap_sound = jiggy_bk_sfx_play(JIGGY_BK_SFX_KAZOOIE_HEUGH, 22000.0f / 32767.0f, 1.0f);
            }
            break;
        case AIR_MOVE_RAP:
            banjo_play(BANJO_ANIM_RAT_A_TAT_RAP);
            banjo_progress += seconds / RAP_PECK_SECONDS;
            break;
        case AIR_MOVE_BUSTER:
            banjo_play_once(BANJO_ANIM_BEAK_BUSTER, BUSTER_SECONDS, seconds);
            return;
        default:
            break;
    }
    if (banjo_air_move != AIR_MOVE_NONE) {
        while (banjo_progress >= 1.0f) {
            banjo_progress -= 1.0f;
        }
        return;
    }
    for (i = 0; i < (s32)(sizeof(banjo_moves) / sizeof(banjo_moves[0])); i++) {
        if (player->control_state == banjo_moves[i].state) {
            banjo_play_once(banjo_moves[i].animation, banjo_moves[i].seconds, seconds);
            return;
        }
    }
    if (player->control_state == STATE_CROUCH) {
        banjo_play(BANJO_ANIM_CROUCH);
        banjo_progress += seconds;
    } else if (airborne) {
        if (player->control_state == STATE_FALL || banjo_animation == BANJO_ANIM_FLAP ||
            banjo_animation == BANJO_ANIM_RAT_A_TAT_RAP || banjo_animation == BANJO_ANIM_FALL) {
            // Walked off a ledge, or coming down after a flap or rap.
            banjo_play(BANJO_ANIM_FALL);
            banjo_progress += seconds;
        } else {
            banjo_play_once(BANJO_ANIM_JUMP, BANJO_JUMP_SECONDS, seconds);
            return;
        }
    } else if (speed < BANJO_WALK_SPEED) {
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
    // Kazooie comes out for her own moves.
    if (banjo_animation == BANJO_ANIM_FLAP || banjo_animation == BANJO_ANIM_RAT_A_TAT_RAP ||
        banjo_animation == BANJO_ANIM_BEAK_BUSTER || banjo_animation == BANJO_ANIM_BEAK_BARGE) {
        for (i = 0; i < (s32)sizeof(kazooie_selectors); i++) {
            banjo_pose.selectors[kazooie_selectors[i]] = 1;
        }
    }
    jiggy_banjo_trot_selectors(banjo_pose.selectors);
    jiggy_banjo_wonderwing_selectors(banjo_pose.selectors);
    jiggy_banjo_shock_selectors(banjo_pose.selectors);
    jiggy_banjo_fly_selectors(banjo_pose.selectors);
    jiggy_banjo_swim_selectors(banjo_pose.selectors);
    if (blink < sizeof(blink_frames)) {
        eyes = blink_frames[blink];
    }
    for (i = SEL_EYES_FIRST; i <= SEL_EYES_LAST; i++) {
        banjo_pose.selectors[i] = eyes;
    }
}

static s32 banjo_here(Actor *actor) {
    return actor == gPlayerPointer && cc_number_of_players == 1 &&
           (current_map == MAP_DK_ISLES_OVERWORLD || current_map == MAP_TRAINING_GROUNDS || current_map == MAP_JAPES);
}

// Whether Banjo is in one of his own air attacks, which DK64's states don't show: the
// Rat-a-tat Rap, and the Beak Buster once it slams down. Enemies take hits from them
// (jiggy_grublins.c).
s32 jiggy_banjo_air_attack(Actor *player) {
    return banjo_here(player) && banjo_is_player &&
           (banjo_air_move == AIR_MOVE_RAP ||
            (banjo_air_move == AIR_MOVE_BUSTER && banjo_air_move_seconds >= BUSTER_HANG_SECONDS));
}

// Whether Banjo is standing in for this actor, so its own model shouldn't be drawn
// (func_global_asm_80630DCC in patches_draw.c).
s32 jiggy_banjo_replaces(Actor *actor) {
    return banjo_is_player && banjo_state == BANJO_READY && banjo_here(actor);
}

Gfx *jiggy_banjo_eggs(Gfx *dl, Actor *player, s32 banjo_is_player, s32 *animation, f32 *progress, s32 *selectors);

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c).
Gfx *jiggy_banjo_draw(Gfx *dl, Actor *actor) {
    f32 yaw;
    f32 x, z;
    f32 offset;
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 tilt[4][4];
    f32 tilted[4][4];
    f32 lift;
    s32 buffer = D_global_asm_807444FC;

    if (!banjo_here(actor)) {
        return dl;
    }
    if (banjo_state == BANJO_UNLOADED) {
        banjo_load();
    }
    if (banjo_state != BANJO_READY) {
        return dl;
    }
    if (newly_pressed_input[0] & L_JPAD) {
        banjo_is_player = !banjo_is_player;
    }
    banjo_animate(actor);
    banjo_set_selectors();
    // Egg Firing (jiggy_banjo_eggs.c): fires and draws eggs, and plays its own move on Banjo.
    dl = jiggy_banjo_eggs(dl, actor, banjo_is_player, &banjo_animation, &banjo_progress, banjo_pose.selectors);

    // Rotations are 12-bit angles. Banjo faces where the player faces, in their place or one
    // step to their side.
    offset = banjo_is_player ? 0.0f : BANJO_SIDE_OFFSET;
    yaw = (actor->y_rotation & 0xFFF) * (360.0f / 4096.0f) + jiggy_banjo_trot_yaw();
    x = actor->x_position + func_global_asm_80612790(actor->y_rotation) * offset;
    z = actor->z_position - func_global_asm_80612794(actor->y_rotation) * offset;
    guScaleF(scale, BANJO_SCALE, BANJO_SCALE, BANJO_SCALE);
    // Flying, Banjo banks (about where he faces) and pitches (about his side) first
    // (jiggy_banjo_fly.c).
    guRotateF(tilt, jiggy_banjo_fly_roll(), 0.0f, 0.0f, 1.0f);
    guMtxCatF(scale, tilt, tilted);
    guRotateF(tilt, jiggy_banjo_fly_pitch(), 1.0f, 0.0f, 0.0f);
    guMtxCatF(tilted, tilt, scale);
    guRotateF(rotate, yaw, 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, banjo_pose.world);
    lift = banjo_is_player ? jiggy_banjo_shock_lift(actor) + jiggy_banjo_fly_lift(actor) : 0.0f;
    // Swimming, Banjo floats at his own depth, beside DK as well (jiggy_banjo_swim.c).
    lift += jiggy_banjo_swim_lift();
    banjo_pose.world[3][0] = x;
    banjo_pose.world[3][1] = actor->y_position + lift;
    banjo_pose.world[3][2] = z;

    banjo_pose.model = JIGGY_BANJO_MODEL;
    banjo_pose.block = banjo_block;
    banjo_pose.animation = banjo_animation;
    banjo_pose.progress = banjo_progress;
    banjo_pose.gfx = banjo_gfx[buffer];
    banjo_pose.gfx_capacity = BANJO_GFX_MAX;
    banjo_pose.mtx = banjo_mtx[buffer];
    banjo_pose.mtx_capacity = BANJO_MTX_MAX;
    if (jiggy_bk_model_pose(&banjo_pose) > 0) {
        gDPPipeSync(dl++);
        gSPDisplayList(dl++, banjo_gfx[buffer]);
    }
    // The Wonderwing's sparkles, the Shock Spring Jump pad and the Flight pad.
    dl = jiggy_banjo_wonderwing_draw(dl, actor);
    dl = jiggy_banjo_shock_pad_draw(dl, actor);
    return jiggy_banjo_fly_pad_draw(dl, actor);
}
