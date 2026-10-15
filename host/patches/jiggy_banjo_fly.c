// Banjo-Kazooie's Flight (src/core2/bs/bFly.c), for when Banjo is the player: a Flight pad
// (ASSET_48A_MODEL_FLIGHT_PAD, chFlightPad in src/core2/code_43A40.c) sits on the grass in the
// Training Grounds, and pressing A standing on it launches Banjo into the air, where Kazooie
// flies him about on Red Feathers (jiggy_feathers.c).
//
// Controls, as in Banjo-Kazooie: A flaps (a red feather each, up to five flaps' lift at once);
// the stick turns left and right (banking) and pitches, forward to dive and back to climb
// (holding R turns harder); B, after a second in the air, is the Beak Bomb (another red
// feather); Z drops into the Beak Buster; landing ends the flight. Banjo-Kazooie's takeoff is
// free (bs_getTypeOfJump); here it spends a red feather too, so with none the buzzer plays and
// Banjo only jumps (FLY_LAUNCH_FEATHERS). The red feather counter stays up while flying.
//
// The moves, from the decompilation, in Banjo-Kazooie units (scaled to DK64's as
// jiggy_banjo3d.c scales Banjo):
// - Takeoff (bsbfly_enter): ASSET_45_ANIM_BSBFLY_ENTER, liftoff at 0.2416 at 1600 a second
//   up under 1200 of gravity, with SFX_C_TAKING_FLIGHT_LIFTOFF; flight starts at the top.
//   DK has already jumped off the pad when this sees the A (DK64 handles it first), so the
//   animation starts at the liftoff.
// - Flight (bsbfly): ASSET_38_ANIM_BSBFLY looping over 0.62 s (0.3 s while flapping), a wing
//   beat (SFX_2_CLAW_SWIPE) at 0.1358. 600 a second forward under 300 of gravity, falling at
//   most 99.9. The stick's x turns the yaw 3 degrees a frame (6 with R) and banks up to 75
//   degrees (85); its y pitches from 60 degrees nose up to 80 nose down. Diving past 60
//   degrees trades speed (down to 60) for fall (gravity to 700, fall to 1000); climbing past
//   40 degrees loses speed (to none at 60) and lets the fall reach 400. Each flap lasts a
//   second (baflap), pitches 30 degrees up and lifts at 400 a second per flap going.
// - Beak Bomb (bsbfly_bomb): ASSET_43_ANIM_BSBFLY_BEAKBOMB_START over a second, hanging still,
//   with SFX_52_BANJO_YAH_OH; at 0.6905 a dash at 4200 a second (SFX_50_KAZOOIE_RRRUH),
//   looping ASSET_47 at 0.3 s, until 4000 units out (or a wall) ends it
//   (bsbfly_beakbombend: ASSET_CC_ANIM_BSFLY_BEAKBOMB_END, SFX_31 and SFX_61, normal gravity
//   for 0.35 s, then a free flap back into flight). Banjo-Kazooie aims the dash along the
//   camera; here it goes where Banjo faces and pitches.
// TODO: the Beak Bomb's crash into the ground (bsbfly_beakbomb_crash: a slice of health) and
// knockback off walls (bsbfly_knockback), the takeoff sparkles (func_802A36D0), the spent
// feathers (func_802D8BE4), and taking off from the Talon Trot (bTrot.c).
//
// How this flies the DK64 player: Banjo still is DK's actor, moved by DK's own physics, which
// keeps collision with walls, ceilings and the floor. Flight starts from DK's own jump off the
// pad and keeps DK in its jump state (control_state 0x17; DK's fall state 0x1E is put back to
// it), whose handler adds y_acceleration to y_velocity, clamps it both ways at
// terminal_velocity (applyActorYAcceleration) and moves the actor by y_velocity and by unkB8
// along unkEE (func_global_asm_806651FC). Once a frame, after DK's update (from
// jiggy_banjo_draw), this reads back the vertical speed DK ended with, runs Banjo-Kazooie's
// flight on it, and writes the next frame's: y_velocity, y_acceleration (Banjo-Kazooie's
// gravity for one frame), terminal_velocity (Banjo-Kazooie's, lowered when needed so the
// clamp never caps a climb), unkB8 and unkEE (the horizontal velocity) and y_rotation (where
// Banjo faces). DK's top speed (PlayerAdditionalActorData unk38) is raised to the flight
// speed so its air control doesn't cap it; what that air control still does with the stick in
// one frame is undone the next. DK's A, B and Z are kept from it while flying (masked where
// its buttons are read, func_global_asm_806DFF34 in jiggy_banjo_eggs.c), so DK's own jump,
// aerial attack and the like don't start under Banjo's. DK64 velocities move the actor by
// velocity x scale_y / 6 a frame at 30 frames a second, so a DK64 unit a second is
// 1 / (scale_y x 5) of DK64's velocity. The fall's start height (unk9C) follows Banjo, so
// landing from high up doesn't hurt as a fall. The terminal velocity DK had is put back when
// the flight ends.
//
// The pad is drawn with the player (jiggy_banjo_draw), like jiggy_banjo_shock.c's pad.
// TODO: pads on more maps.

#include "common_structs.h"
#include "jiggy.h"

#define PAD_MODEL 0x48A // ASSET_48A_MODEL_FLIGHT_PAD
// A converted pad is about 6 KiB; its pose is a handful of commands and loads only the root
// matrix (it has no skeleton).
#define PAD_BLOCK_MAX 0x2000
#define PAD_GFX_MAX 16
#define PAD_MTX_MAX 4
// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo. The pad is 212 units
// across and 16 high in Banjo-Kazooie.
#define BK_TO_DK64 0.25f
#define PAD_RADIUS (106.0f * BK_TO_DK64)
#define PAD_HEIGHT (16.0f * BK_TO_DK64)
// How far the player's feet can be above or below the pad's base and still stand on it.
#define PAD_FEET_ABOVE 15.0f
#define PAD_FEET_BELOW 5.0f
// Placing the pad on the floor under its spot (as jiggy_banjo_shock.c places its pad).
#define PAD_FLOOR_PROBE 20.0f
#define PAD_FLOOR_MAX_STEP 40.0f
// Height above the floor that counts as in the air, as in jiggy_banjo3d.c.
#define FLY_AIRBORNE_HEIGHT 6.0f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// Where the pad goes in the Training Grounds: on the grass east of the notes (the player
// starts on the treehouse deck, at about (1346, 227, 1098)), 89 units or more from the notes,
// Jinjos, Honeycombs and the Shock Spring Jump pad. Snapped to the floor when placed.
#define PAD_MAP MAP_TRAINING_GROUNDS
#define PAD_X 1560.0f
#define PAD_Y 35.0f
#define PAD_Z 900.0f

// Red feathers the takeoff spends; 0 is Banjo-Kazooie's (see the top of the file).
#define FLY_LAUNCH_FEATHERS 1

// Banjo-Kazooie animation assets (asset_e) and how they play.
#define FLY_ANIM_ENTER 0x45 // ASSET_45_ANIM_BSBFLY_ENTER
#define FLY_ENTER_LIFTOFF 0.2416f
#define FLY_ENTER_SECONDS 2.4f // From the liftoff on.
#define FLY_ANIM_FLY 0x38 // ASSET_38_ANIM_BSBFLY
#define FLY_LOOP_SECONDS 0.62f
#define FLY_FLAP_LOOP_SECONDS 0.3f
#define FLY_WING_BEAT_AT 0.1358f
#define FLY_ANIM_BOMB_START 0x43 // ASSET_43_ANIM_BSBFLY_BEAKBOMB_START
#define FLY_BOMB_START_SECONDS 1.0f
#define FLY_BOMB_DASH_AT 0.6905f
#define FLY_BOMB_REST_SECONDS 0.05f // The rest of the start, once dashing.
#define FLY_ANIM_BOMB 0x47 // The dash's loop (bsbfly_bomb_update).
#define FLY_BOMB_LOOP_SECONDS 0.3f
#define FLY_ANIM_BOMB_END 0xCC // ASSET_CC_ANIM_BSFLY_BEAKBOMB_END
#define FLY_BOMB_END_LOOP_SECONDS 0.38f
#define FLY_BOMB_END_SECONDS 0.35f
// Banjo's fall (jiggy_banjo3d.c's BANJO_ANIM_FALL), handed back when the flight ends in the air.
#define FLY_ANIM_FALL 0xB0

// Banjo-Kazooie's physics for each stage, in its units a second (and a second squared).
#define FLY_LIFTOFF_SPEED 1600.0f
#define FLY_ENTER_GRAVITY -1200.0f
#define FLY_DEFAULT_GRAVITY -2700.0f  // baphysics_default_gravity
#define FLY_DEFAULT_TERMINAL -4000.0f // baphysics_default_terminal_velocity
#define FLY_SPEED 600.0f
#define FLY_GRAVITY -300.0f
#define FLY_TERMINAL -99.9f
#define FLY_DIVE_SPEED 60.0f
#define FLY_DIVE_GRAVITY -700.0f
#define FLY_DIVE_TERMINAL -1000.33f
#define FLY_CLIMB_TERMINAL -399.99f
#define FLY_FLAP_LIFT 400.0f
#define FLY_BOMB_SPEED 4200.0f
#define FLY_BOMB_RANGE 4000.0f
// The flight's velocity follows its target by this much a frame (func_802A3430 picks the
// player's physics mode 4, func_80294378; D_80371E30).
#define FLY_SPEED_FOLLOW 0.05f
// Turning (func_802A354C, func_802A3648, func_802A33D8, func_802A3404): degrees of yaw a frame
// at full stick, banks, and how fast yaw, bank and pitch follow (degrees a second at most, and
// the share of the way a second).
#define FLY_YAW_STEP 3.0f
#define FLY_YAW_STEP_R 6.0f
#define FLY_ROLL_RANGE 75.0f
#define FLY_ROLL_RANGE_R 85.0f
#define FLY_TURN_LIMIT 500.0f
#define FLY_YAW_RATE 2.0f
#define FLY_YAW_RATE_R 30.0f
#define FLY_ROLL_RATE 2.0f
#define FLY_PITCH_RATE 1.2f
#define FLY_FLAP_PITCH_LIMIT 1000.0f
#define FLY_FLAP_PITCH_RATE 2.2f
#define FLY_FLAP_PITCH_UP 30.0f
// Flaps (baflap): each lasts this long (0.35 for the free one after a Beak Bomb), at most five
// at once, and one within 0.25 s of another only stretches the newest.
#define FLY_FLAPS 5
#define FLY_FLAP_SECONDS 1.0f
#define FLY_FREE_FLAP_SECONDS 0.35f
#define FLY_FLAP_SPACING 0.25f
// Seconds into a flight before B Beak Bombs.
#define FLY_BOMB_WAIT 1.0f
// Banjo is drawn this high while flying (baModel_setYDisplacement(60) in func_802A3430).
#define FLY_MODEL_LIFT (60.0f * BK_TO_DK64)
// Stick travel taken as full (DK64 clamps it at 80).
#define FLY_STICK_FULL 70.0f
// Frames DK's jump has to leave the ground in after the takeoff, or the flight is given up.
#define FLY_LAUNCH_FRAMES 10

// DK64 player states (Actor control_state).
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
#define STATE_JUMP 0x17
#define STATE_FALL 0x1E
// The jump state's step once its launch speed is in (func_global_asm_806D24E4).
#define JUMP_PROGRESS_AIRBORNE 2

// Kazooie's upper half (head and wings), out of the backpack, all through a flight
// (modelAppendages_setKazooiesUpperHalfVisibility).
static const u8 fly_selectors[] = {0x01, 0x09, 0x0C, 0x0F};

// Feathers (jiggy_feathers.c): kind 0 is red.
#define FEATHER_RED 0
s32 jiggy_feathers_spend(s32 kind);
void jiggy_feathers_hud_hold(s32 kind);

extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u16 newly_pressed_input[];
extern OSContPad D_global_asm_807ECD58; // Buttons and stick held this frame.
extern f32 D_global_asm_807531FC[];     // Kong walking top speed.
extern s16 D_global_asm_807FD584;       // Current Kong, an index into the Kong tables.
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
f32 func_global_asm_80611BB4(f32 x, f32 z); // The angle of (x, z), in radians.
// The floor under (x, z), as the game looks for one (func_global_asm_80667180): writes its
// height to *y, leaving it as it was if there's none. The return value isn't reliable.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);

typedef enum {
    PAD_UNLOADED,
    PAD_READY,
    PAD_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} PadLoadState;

typedef enum {
    FLY_OFF,
    FLY_ENTER,    // The takeoff.
    FLY_ON,       // Flying.
    FLY_BOMB,     // The Beak Bomb's wind-up and dash.
    FLY_BOMB_END, // Flying out of a Beak Bomb.
} FlyState;

static u8 pad_block[PAD_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx pad_gfx[2][PAD_GFX_MAX];
static Mtx pad_mtx[2][PAD_MTX_MAX];
static JiggyBkPose pad_pose;
static PadLoadState pad_state = PAD_UNLOADED;
static s32 pad_last_map = -1;
static s32 pad_placed;
static f32 pad_pos[3];

static FlyState fly_state;
static s32 fly_map;            // The map the flight started on.
static f32 fly_yaw;            // Degrees, as y_rotation (0 faces +z, turning toward +x).
static f32 fly_yaw_ideal;
static f32 fly_pitch;          // Degrees, 0 to 360, nose down positive (Banjo-Kazooie's pitch).
static f32 fly_pitch_ideal;
static f32 fly_roll;           // Degrees, 0 to 360, right wing down positive.
static f32 fly_roll_ideal;
static f32 fly_velocity[2];    // Horizontal, x and z, Banjo-Kazooie units a second.
static f32 fly_vertical;       // Banjo-Kazooie units a second.
static f32 fly_flaps[FLY_FLAPS]; // Seconds left of each flap going (baflap).
static f32 fly_flap_stretch;   // A flap to add next frame (baflap's s_flap_max_timer).
static u8 fly_flapping;        // Pitched up for flapping (D_8037D344).
static u8 fly_flap_pending;    // A pressed, its feather not spent yet (D_8037D347).
static u8 fly_free_flap;       // Out of a Beak Bomb: a short flap for free (D_8037D346).
static f32 fly_loop_seconds;   // How long a loop of the flight animation takes now.
static f32 fly_timer;          // Till B Beak Bombs, or the end of a Beak Bomb's exit (D_8037D320).
static u8 fly_left_ground;
static u8 fly_launch_wait;     // Frames left for DK's jump to leave the ground.
static u8 fly_dashing;         // The Beak Bomb's dash has started.
static f32 fly_bomb_from[3];   // Where the dash started.
static f32 fly_last_pos[3];
static f32 fly_dk_terminal;    // DK's own terminal velocity, put back after.
static f32 fly_dk_acceleration;

static f32 fly_walk_top_speed(void) {
    return D_global_asm_807531FC[D_global_asm_807FD584];
}

static void pad_load(void) {
    s32 size;
    s32 i;

    pad_state = PAD_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(PAD_MODEL, pad_block, sizeof(pad_block));
    if (size <= 0 || size > (s32)sizeof(pad_block)) {
        return;
    }
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        pad_pose.selectors[i] = 0;
    }
    pad_state = PAD_READY;
}

// Stands the pad on the floor at its spot (or at the spot's height, if no floor is found near
// it).
static void pad_place(void) {
    f32 probe = PAD_Y + PAD_FLOOR_PROBE;
    f32 floor = probe;

    pad_pos[0] = PAD_X;
    pad_pos[1] = PAD_Y;
    pad_pos[2] = PAD_Z;
    func_global_asm_80667110(PAD_X, PAD_Z, &floor);
    if (floor != probe && floor - PAD_Y < PAD_FLOOR_MAX_STEP && PAD_Y - floor < PAD_FLOOR_MAX_STEP) {
        pad_pos[1] = floor;
    }
    pad_placed = TRUE;
}

// Whether the player is over the pad, with their feet at its height.
static s32 pad_at(Actor *player) {
    f32 dx = player->x_position - pad_pos[0];
    f32 dz = player->z_position - pad_pos[2];
    f32 dy = player->y_position - pad_pos[1];

    return pad_state == PAD_READY && pad_placed && current_map == PAD_MAP &&
           dx * dx + dz * dz < PAD_RADIUS * PAD_RADIUS && dy < PAD_FEET_ABOVE && dy > -PAD_FEET_BELOW;
}

// Whether the player stands on the pad.
static s32 pad_under(Actor *player) {
    return pad_at(player) && player->y_position - player->floor <= FLY_AIRBORNE_HEIGHT;
}

static void fly_play(s32 *animation, f32 *progress, s32 id, f32 start) {
    if (*animation != id) {
        *animation = id;
        *progress = start;
    }
}

static f32 fly_map_range(f32 value, f32 from_a, f32 from_b, f32 to_a, f32 to_b) {
    f32 t = (value - from_a) / (from_b - from_a);

    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    return to_a + (to_b - to_a) * t;
}

static f32 fly_normalize(f32 degrees) {
    while (degrees >= 360.0f) {
        degrees -= 360.0f;
    }
    while (degrees < 0.0f) {
        degrees += 360.0f;
    }
    return degrees;
}

// Turns an angle toward its ideal, as Banjo-Kazooie's yaw, pitch and roll follow theirs
// (__pitch_update): `rate` of the way a second, at least 0.1 degrees and at most `limit`
// degrees a second.
static f32 fly_follow(f32 angle, f32 ideal, f32 limit, f32 rate, f32 seconds) {
    f32 diff = ideal - angle;
    f32 max = limit * seconds;
    f32 step;

    if (diff > 180.0f) {
        diff -= 360.0f;
    } else if (diff < -180.0f) {
        diff += 360.0f;
    }
    step = diff * rate * seconds;
    if (step < 0.0f) {
        step = step < -max ? -max : (step > -0.1f ? -0.1f : step);
    } else {
        step = step > max ? max : (step < 0.1f ? 0.1f : step);
    }
    if ((step < 0.0f ? -step : step) <= (diff < 0.0f ? -diff : diff)) {
        return fly_normalize(angle + step);
    }
    return fly_normalize(ideal);
}

static f32 fly_stick(s8 value) {
    f32 stick = value / FLY_STICK_FULL;

    return stick > 1.0f ? 1.0f : (stick < -1.0f ? -1.0f : stick);
}

// Flaps going (baflap_getCount).
static s32 fly_flap_count(void) {
    s32 count = 0;
    s32 i;

    for (i = 0; i < FLY_FLAPS; i++) {
        if (fly_flaps[i] != 0.0f) {
            count++;
        }
    }
    return count;
}

// A new flap of `seconds` (baflap_add): within 0.25 s of one going, it only stretches that
// one, next frame.
static void fly_flap_add(f32 seconds) {
    s32 i;

    for (i = 0; i < FLY_FLAPS; i++) {
        if (seconds - FLY_FLAP_SPACING < fly_flaps[i]) {
            fly_flap_stretch = seconds;
            return;
        }
    }
    for (i = 0; i < FLY_FLAPS; i++) {
        if (fly_flaps[i] == 0.0f) {
            fly_flaps[i] = seconds;
            return;
        }
    }
    fly_flap_stretch = seconds;
}

// baflap_update.
static void fly_flaps_update(f32 seconds) {
    f32 stretch = fly_flap_stretch;
    s32 i;

    for (i = 0; i < FLY_FLAPS; i++) {
        fly_flaps[i] = fly_flaps[i] > seconds ? fly_flaps[i] - seconds : 0.0f;
    }
    if (stretch != 0.0f) {
        fly_flap_stretch = 0.0f;
        fly_flap_add(stretch);
    }
}

static void fly_flaps_reset(void) {
    s32 i;

    for (i = 0; i < FLY_FLAPS; i++) {
        fly_flaps[i] = 0.0f;
    }
    fly_flap_stretch = 0.0f;
    fly_flapping = FALSE;
}

// DK64 velocity units for one Banjo-Kazooie unit a second (see the top of the file).
static f32 fly_dk_units(Actor *player) {
    f32 scale = player->animation_state != NULL ? player->animation_state->scale_y : 0.15f;

    return BK_TO_DK64 / (scale * (GAME_FPS / 6.0f));
}

// Hands DK the next frame of flight: the vertical speed, a frame of gravity, the terminal
// velocity and the horizontal velocity, in Banjo-Kazooie units, and where Banjo faces.
static void fly_drive(Actor *player, f32 gravity, f32 terminal) {
    PlayerAdditionalActorData *paad = player->PaaD;
    f32 units = fly_dk_units(player);
    f32 vertical = fly_vertical * units;
    f32 horizontal_x = fly_velocity[0] * units;
    f32 horizontal_z = fly_velocity[1] * units;
    f32 speed = _sqrtf(horizontal_x * horizontal_x + horizontal_z * horizontal_z);

    terminal *= units;
    // DK64 clamps climbs at the terminal velocity too.
    if (vertical > -terminal) {
        terminal = -vertical;
    }
    player->y_velocity = vertical;
    player->y_acceleration = gravity * units / GAME_FPS;
    player->terminal_velocity = terminal;
    if (player->control_state == STATE_FALL) {
        player->control_state = STATE_JUMP;
        player->control_state_progress = JUMP_PROGRESS_AIRBORNE;
    }
    // Before the jump state takes its launch speed (control_state_progress 0 and 1), that too.
    if (paad != NULL && player->control_state == STATE_JUMP && player->control_state_progress < JUMP_PROGRESS_AIRBORNE) {
        paad->unk54 = vertical;
    }
    player->unkB8 = speed;
    if (paad != NULL) {
        paad->unk38 = speed;
    }
    if (speed > 0.0f) {
        player->unkEE = (s16)(func_global_asm_80611BB4(horizontal_x, horizontal_z) * (4096.0f / (2.0f * 3.14159265f))) & 0xFFF;
    }
    player->y_rotation = (s16)(fly_yaw * (4096.0f / 360.0f)) & 0xFFF;
    player->unk9C = player->y_position;
}

// The horizontal velocity eases toward `speed` the way Banjo faces (__baphysics_update_normal).
static void fly_follow_speed(f32 speed, f32 seconds) {
    s16 yaw = (s16)(fly_yaw * (4096.0f / 360.0f)) & 0xFFF;
    f32 follow = FLY_SPEED_FOLLOW * seconds * GAME_FPS;
    f32 target_x = func_global_asm_80612794(yaw) * speed;
    f32 target_z = func_global_asm_80612790(yaw) * speed;

    if (follow > 1.0f) {
        follow = 1.0f;
    }
    fly_velocity[0] += (target_x - fly_velocity[0]) * follow;
    fly_velocity[1] += (target_z - fly_velocity[1]) * follow;
}

// Straight to `speed` the way Banjo faces (baphysics_set_horizontal_velocity).
static void fly_set_speed(f32 speed) {
    s16 yaw = (s16)(fly_yaw * (4096.0f / 360.0f)) & 0xFFF;

    fly_velocity[0] = func_global_asm_80612794(yaw) * speed;
    fly_velocity[1] = func_global_asm_80612790(yaw) * speed;
}

static void fly_stop(Actor *player) {
    PlayerAdditionalActorData *paad = player->PaaD;

    fly_state = FLY_OFF;
    fly_flaps_reset();
    player->terminal_velocity = fly_dk_terminal;
    player->y_acceleration = fly_dk_acceleration;
    // DK's walk sets its own top speed again on its next state; put it back now if walking.
    if (paad != NULL && (player->control_state == STATE_IDLE || player->control_state == STATE_WALK)) {
        paad->unk38 = fly_walk_top_speed();
    }
}

// Into flight proper (bsbfly_init and func_802A3430).
static void fly_start_flying(s32 *animation, f32 *progress) {
    fly_state = FLY_ON;
    *animation = FLY_ANIM_FLY;
    *progress = 0.0f;
    fly_loop_seconds = FLY_LOOP_SECONDS;
    fly_set_speed(FLY_SPEED);
    fly_flaps_reset();
    fly_flap_pending = FALSE;
    fly_timer = FLY_BOMB_WAIT;
}

// The takeoff (bsbfly_enter_init and its liftoff): Kazooie's "auw" (basfx_80299BD4) and the
// liftoff sound (func_8030E58C: 22000, pitch 0.7).
static void fly_start(Actor *player, s32 *animation, f32 *progress) {
    fly_state = FLY_ENTER;
    fly_map = current_map;
    fly_dk_terminal = player->terminal_velocity;
    fly_dk_acceleration = player->y_acceleration;
    fly_yaw = fly_yaw_ideal = (player->y_rotation & 0xFFF) * (360.0f / 4096.0f);
    fly_pitch = fly_pitch_ideal = 0.0f;
    fly_roll = fly_roll_ideal = 0.0f;
    fly_velocity[0] = fly_velocity[1] = 0.0f;
    fly_vertical = FLY_LIFTOFF_SPEED;
    fly_free_flap = FALSE;
    fly_left_ground = FALSE;
    fly_launch_wait = FLY_LAUNCH_FRAMES;
    fly_flaps_reset();
    *animation = FLY_ANIM_ENTER;
    *progress = FLY_ENTER_LIFTOFF;
    jiggy_bk_sfx_play(JIGGY_BK_SFX_KAZOOIE_AUW, 22000.0f / 32767.0f, 1.117f);
    jiggy_bk_sfx_play(JIGGY_BK_SFX_FLIGHT_LIFTOFF, 22000.0f / 32767.0f, 0.7f);
}

// The Beak Bomb (bsbfly_bomb_init): Banjo's "yah-oh" (FUNC_8030E624: 28000).
static void fly_start_bomb(Actor *player, s32 *animation, f32 *progress) {
    fly_state = FLY_BOMB;
    fly_dashing = FALSE;
    fly_roll_ideal = 0.0f;
    fly_velocity[0] = fly_velocity[1] = 0.0f;
    fly_vertical = 0.0f;
    fly_flaps_reset();
    *animation = FLY_ANIM_BOMB_START;
    *progress = 0.0f;
    jiggy_bk_sfx_play(JIGGY_BK_SFX_BANJO_YAH_OH, 28000.0f / 32767.0f, 1.0f);
}

// Out of a Beak Bomb (bsbfly_beakbombend_init): level again, normal gravity for a moment,
// Banjo's "ohhwaaooo" and a cartoon fall (basfx_80299CF4, basfx_80299D2C: 0x7FFF).
static void fly_end_bomb(s32 *animation, f32 *progress) {
    fly_state = FLY_BOMB_END;
    fly_pitch_ideal = 0.0f;
    fly_roll_ideal = 0.0f;
    fly_timer = FLY_BOMB_END_SECONDS;
    *animation = FLY_ANIM_BOMB_END;
    *progress = 0.0f;
    jiggy_bk_sfx_play(JIGGY_BK_SFX_BANJO_OHHWAAOOO, 32767.0f / 32767.0f, 1.0f);
    jiggy_bk_sfx_play(JIGGY_BK_SFX_CARTOONY_FALL, 32767.0f / 32767.0f, 1.0f);
}

// A frame of flight proper (bsbfly_update), the Beak Bomb starting from it.
static void fly_flying(Actor *player, f32 seconds, u32 frames, u16 pressed, s32 *animation, f32 *progress) {
    s32 r_held = (D_global_asm_807ECD58.button & R_TRIG) != 0;
    f32 stick_x = fly_stick(D_global_asm_807ECD58.stick_x);
    f32 stick_y = fly_stick(D_global_asm_807ECD58.stick_y);
    f32 yaw_step = r_held ? FLY_YAW_STEP_R : FLY_YAW_STEP;
    f32 roll_range = r_held ? FLY_ROLL_RANGE_R : FLY_ROLL_RANGE;
    f32 pitch = fly_pitch;
    f32 pitch_limit = FLY_TURN_LIMIT;
    f32 pitch_rate = FLY_PITCH_RATE;
    f32 gravity = FLY_GRAVITY;
    f32 terminal;
    f32 speed;
    f32 before;
    s32 flapped = FALSE;
    s32 flaps;

    // Turning and banking with the stick's x (func_802A354C), pitching with its y
    // (func_802A3648): forward dives, back climbs.
    fly_roll_ideal = fly_map_range(stick_x, -1.0f, 1.0f, -roll_range, roll_range);
    fly_yaw_ideal = fly_normalize(fly_yaw_ideal + fly_map_range(stick_x, -1.0f, 1.0f, yaw_step, -yaw_step) * frames);
    if (stick_y < 0.0f) {
        fly_pitch_ideal = fly_map_range(stick_y, -1.0f, 0.0f, 300.0f, 360.0f);
    } else {
        fly_pitch_ideal = fly_map_range(stick_y, 0.0f, 1.0f, 0.0f, 80.0f);
    }

    // A flaps, spending its red feather on the next frame that's a multiple of 3, as
    // Banjo-Kazooie's globalTimer does; without one, the buzzer.
    if (pressed & A_BUTTON) {
        fly_flap_pending = TRUE;
    }
    if (fly_flap_pending && object_timer % 3 == 0) {
        fly_flap_pending = FALSE;
        flapped = jiggy_feathers_spend(FEATHER_RED);
    }
    if (flapped || fly_free_flap) {
        if (flapped) {
            fly_loop_seconds = FLY_FLAP_LOOP_SECONDS;
        }
        fly_flap_add(fly_free_flap ? FLY_FREE_FLAP_SECONDS : FLY_FLAP_SECONDS);
        fly_free_flap = FALSE;
    }
    flaps = fly_flap_count();
    if (flaps) {
        fly_flapping = TRUE;
    }
    if (fly_flapping) {
        // Nose up 30 more, no further than 60 up.
        fly_pitch_ideal = fly_normalize(fly_pitch_ideal - FLY_FLAP_PITCH_UP);
        if (fly_pitch_ideal > 80.0f && fly_pitch_ideal < 300.0f) {
            fly_pitch_ideal = 300.0f;
        }
        pitch_limit = FLY_FLAP_PITCH_LIMIT;
        pitch_rate = FLY_FLAP_PITCH_RATE;
        if (flaps) {
            fly_vertical = flaps * FLY_FLAP_LIFT;
        } else {
            fly_loop_seconds = FLY_LOOP_SECONDS;
            fly_flapping = FALSE;
        }
    }

    // Speed and fall from the pitch.
    if (pitch <= 80.0f) {
        terminal = fly_map_range(pitch, 60.0f, 80.0f, FLY_TERMINAL, FLY_DIVE_TERMINAL);
        gravity = fly_map_range(pitch, 60.0f, 80.0f, FLY_GRAVITY, FLY_DIVE_GRAVITY);
        speed = fly_map_range(pitch, 60.0f, 80.0f, FLY_SPEED, FLY_DIVE_SPEED);
    } else {
        terminal = fly_map_range(pitch, 300.0f, 310.0f, FLY_CLIMB_TERMINAL, FLY_TERMINAL);
        speed = fly_map_range(pitch, 300.0f, 340.0f, 0.0f, FLY_SPEED);
    }
    fly_follow_speed(speed, seconds);

    fly_yaw = fly_follow(fly_yaw, fly_yaw_ideal, FLY_TURN_LIMIT, r_held ? FLY_YAW_RATE_R : FLY_YAW_RATE, seconds);
    fly_roll = fly_follow(fly_roll, fly_roll_ideal, FLY_TURN_LIMIT, FLY_ROLL_RATE, seconds);
    fly_pitch = fly_follow(fly_pitch, fly_pitch_ideal, pitch_limit, pitch_rate, seconds);
    fly_flaps_update(seconds);
    fly_drive(player, gravity, terminal);

    fly_play(animation, progress, FLY_ANIM_FLY, 0.0f);
    before = *progress;
    *progress += seconds / fly_loop_seconds;
    // A wing beat (func_8030EBC8: pitch 0.6 to 0.7, volume 10000 to 12000).
    if (before < FLY_WING_BEAT_AT && *progress >= FLY_WING_BEAT_AT) {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_WING_BEAT, (10000.0f + (object_timer % 5) * 500.0f) / 32767.0f,
                          0.6f + (object_timer % 3) * 0.05f);
    }
    while (*progress >= 1.0f) {
        *progress -= 1.0f;
    }

    // B Beak Bombs, after a second in the air and for a red feather (the buzzer without).
    fly_timer = fly_timer > seconds ? fly_timer - seconds : 0.0f;
    if (fly_timer == 0.0f && (pressed & B_BUTTON) && jiggy_feathers_spend(FEATHER_RED)) {
        fly_start_bomb(player, animation, progress);
        fly_drive(player, 0.0f, -FLY_BOMB_SPEED);
    }
}

// A frame of the Beak Bomb (bsbfly_bomb_update): hanging still for the wind-up, then the dash.
static void fly_bombing(Actor *player, f32 seconds, s32 *animation, f32 *progress) {
    s16 yaw = (s16)(fly_yaw * (4096.0f / 360.0f)) & 0xFFF;
    s16 pitch = (s16)(fly_pitch * (4096.0f / 360.0f)) & 0xFFF;
    f32 before = *progress;
    f32 dx, dy, dz;
    f32 moved, expected;

    if (!fly_dashing) {
        *progress += seconds / FLY_BOMB_START_SECONDS;
        if (before < FLY_BOMB_DASH_AT && *progress >= FLY_BOMB_DASH_AT) {
            // Off along where Banjo faces, nose down positive.
            fly_dashing = TRUE;
            fly_velocity[0] = func_global_asm_80612794(yaw) * func_global_asm_80612790(pitch) * FLY_BOMB_SPEED;
            fly_velocity[1] = func_global_asm_80612790(yaw) * func_global_asm_80612790(pitch) * FLY_BOMB_SPEED;
            fly_vertical = -func_global_asm_80612794(pitch) * FLY_BOMB_SPEED;
            fly_bomb_from[0] = fly_last_pos[0] = player->x_position;
            fly_bomb_from[1] = fly_last_pos[1] = player->y_position;
            fly_bomb_from[2] = fly_last_pos[2] = player->z_position;
            // basfx_80299CF4(SFX_50_KAZOOIE_RRRUH, 1.3, 0x7FFF).
            jiggy_bk_sfx_play(JIGGY_BK_SFX_KAZOOIE_RRRUH, 32767.0f / 32767.0f, 1.3f);
        }
        fly_drive(player, 0.0f, -FLY_BOMB_SPEED);
        return;
    }
    // The rest of the wind-up quickly, then the dash's loop.
    if (*animation == FLY_ANIM_BOMB_START) {
        *progress += seconds / FLY_BOMB_REST_SECONDS;
        if (*progress >= 0.99f) {
            *animation = FLY_ANIM_BOMB;
            *progress = 0.0f;
        }
    } else {
        *progress += seconds / FLY_BOMB_LOOP_SECONDS;
        while (*progress >= 1.0f) {
            *progress -= 1.0f;
        }
    }
    // Out of range (4000 units, 16000000 squared), or stopped by a wall: the exit.
    dx = (player->x_position - fly_bomb_from[0]) / BK_TO_DK64;
    dy = (player->y_position - fly_bomb_from[1]) / BK_TO_DK64;
    dz = (player->z_position - fly_bomb_from[2]) / BK_TO_DK64;
    expected = _sqrtf(fly_velocity[0] * fly_velocity[0] + fly_velocity[1] * fly_velocity[1] +
                      fly_vertical * fly_vertical) * seconds * BK_TO_DK64;
    moved = _sqrtf((player->x_position - fly_last_pos[0]) * (player->x_position - fly_last_pos[0]) +
                   (player->y_position - fly_last_pos[1]) * (player->y_position - fly_last_pos[1]) +
                   (player->z_position - fly_last_pos[2]) * (player->z_position - fly_last_pos[2]));
    fly_last_pos[0] = player->x_position;
    fly_last_pos[1] = player->y_position;
    fly_last_pos[2] = player->z_position;
    if (dx * dx + dy * dy + dz * dz > FLY_BOMB_RANGE * FLY_BOMB_RANGE ||
        (*animation == FLY_ANIM_BOMB && moved < expected * 0.25f)) {
        fly_end_bomb(animation, progress);
        fly_drive(player, FLY_DEFAULT_GRAVITY, FLY_DEFAULT_TERMINAL);
        return;
    }
    fly_drive(player, 0.0f, -FLY_BOMB_SPEED);
}

// Called from banjo_animate (jiggy_banjo3d.c) after the Shock Spring Jump, before Banjo's air
// moves. Takes off from the pad and, while flying, flies the player and picks Banjo's
// animation. Returns 1 when it did.
s32 jiggy_banjo_fly_animate(Actor *player, s32 is_player, s32 airborne, f32 seconds, s32 *animation,
                            f32 *progress) {
    u16 pressed = newly_pressed_input[0];
    u32 frames = (u32)(seconds * GAME_FPS + 0.5f);
    s32 state = player->control_state;

    if (fly_state != FLY_OFF && (!is_player || (s32)current_map != fly_map || player->PaaD == NULL)) {
        fly_stop(player);
        return FALSE;
    }
    if (!is_player) {
        return FALSE;
    }
    if (fly_state == FLY_OFF) {
        // DK64 handles the A before this runs, so DK's jump off the pad has already started.
        if (!(pressed & A_BUTTON) || !pad_at(player) || state != STATE_JUMP || player->PaaD == NULL) {
            return FALSE;
        }
        if (FLY_LAUNCH_FEATHERS && !jiggy_feathers_spend(FEATHER_RED)) {
            // The buzzer, and only DK's jump.
            return FALSE;
        }
        fly_start(player, animation, progress);
        fly_drive(player, FLY_ENTER_GRAVITY, FLY_DEFAULT_TERMINAL);
        return TRUE;
    }
    jiggy_feathers_hud_hold(FEATHER_RED);
    // What DK's velocity came to over its update, back in Banjo-Kazooie units.
    fly_vertical = player->y_velocity / fly_dk_units(player);

    // Taken out of the air by anything but its jump, or landed: the flight is over.
    if (state != STATE_JUMP && state != STATE_FALL) {
        fly_stop(player);
        return FALSE;
    }
    if (airborne) {
        fly_left_ground = TRUE;
    } else if (!fly_left_ground) {
        if (fly_launch_wait == 0 || --fly_launch_wait == 0) {
            // DK didn't leave the ground (something else had the player).
            fly_stop(player);
            return FALSE;
        }
    } else if (fly_vertical <= 0.0f) {
        fly_stop(player);
        return FALSE;
    }
    // Z drops into Banjo's Beak Buster (bainput_should_beak_bust).
    if (pressed & Z_TRIG) {
        fly_stop(player);
        *animation = FLY_ANIM_FALL;
        *progress = 0.0f;
        return FALSE;
    }

    switch (fly_state) {
        case FLY_ENTER:
            // Up and slowing until it starts to fall (bsbfly_enter_update), then flying.
            fly_play(animation, progress, FLY_ANIM_ENTER, FLY_ENTER_LIFTOFF);
            *progress += seconds / FLY_ENTER_SECONDS;
            if (*progress > 0.99f) {
                *progress = 0.99f;
            }
            if (fly_vertical >= 0.0f) {
                fly_drive(player, FLY_ENTER_GRAVITY, FLY_DEFAULT_TERMINAL);
                return TRUE;
            }
            fly_start_flying(animation, progress);
            // Fall through.
        case FLY_ON:
            fly_flying(player, seconds, frames, pressed, animation, progress);
            return TRUE;
        case FLY_BOMB:
            fly_bombing(player, seconds, animation, progress);
            return TRUE;
        case FLY_BOMB_END:
            // Normal gravity for a moment (bsbfly_beakbombend_update), then flying again with a
            // short flap.
            fly_play(animation, progress, FLY_ANIM_BOMB_END, 0.0f);
            *progress += seconds / FLY_BOMB_END_LOOP_SECONDS;
            while (*progress >= 1.0f) {
                *progress -= 1.0f;
            }
            fly_pitch = fly_follow(fly_pitch, fly_pitch_ideal, FLY_TURN_LIMIT, FLY_PITCH_RATE, seconds);
            fly_roll = fly_follow(fly_roll, fly_roll_ideal, FLY_TURN_LIMIT, FLY_ROLL_RATE, seconds);
            fly_follow_speed(FLY_SPEED, seconds);
            fly_timer -= seconds;
            if (fly_timer < 0.0f) {
                fly_start_flying(animation, progress);
                fly_free_flap = TRUE;
                fly_flying(player, seconds, frames, pressed, animation, progress);
                return TRUE;
            }
            fly_drive(player, FLY_DEFAULT_GRAVITY, FLY_DEFAULT_TERMINAL);
            return TRUE;
        default:
            return FALSE;
    }
}

// Called from banjo_set_selectors (jiggy_banjo3d.c): Kazooie's upper half while flying.
void jiggy_banjo_fly_selectors(s32 *selectors) {
    s32 i;

    if (fly_state == FLY_OFF) {
        return;
    }
    for (i = 0; i < (s32)sizeof(fly_selectors); i++) {
        selectors[fly_selectors[i]] = 1;
    }
}

// Degrees to tip Banjo's model by while flying (jiggy_banjo_draw): pitch, nose down positive,
// and bank, right wing down positive.
f32 jiggy_banjo_fly_pitch(void) {
    return fly_state == FLY_OFF ? 0.0f : fly_pitch;
}

f32 jiggy_banjo_fly_roll(void) {
    return fly_state == FLY_OFF ? 0.0f : fly_roll;
}

// How far up to draw Banjo: standing on the pad, on top of it rather than in it; flying, as
// Banjo-Kazooie raises the model (not during the takeoff).
f32 jiggy_banjo_fly_lift(Actor *player) {
    if (fly_state == FLY_ENTER) {
        return 0.0f;
    }
    if (fly_state != FLY_OFF) {
        return FLY_MODEL_LIFT;
    }
    return pad_under(player) ? PAD_HEIGHT : 0.0f;
}

// Buttons to keep from DK while flying, where DK64 reads them (func_global_asm_806DFF34 in
// jiggy_banjo_eggs.c): A, B and Z belong to the flight.
u16 jiggy_banjo_fly_masked_buttons(void) {
    if (fly_state == FLY_OFF || cc_number_of_players != 1 || cc_player_index != 0) {
        return 0;
    }
    return A_BUTTON | B_BUTTON | Z_TRIG;
}

// Called once a frame from jiggy_banjo_draw (jiggy_banjo3d.c), after Banjo: draws the pad.
Gfx *jiggy_banjo_fly_pad_draw(Gfx *dl, Actor *player) {
    s32 buffer = D_global_asm_807444FC;

    if ((s32)current_map != pad_last_map) {
        pad_last_map = current_map;
        pad_placed = FALSE;
    }
    if (current_map != PAD_MAP || cc_number_of_players != 1) {
        return dl;
    }
    if (pad_state == PAD_UNLOADED) {
        pad_load();
    }
    if (pad_state != PAD_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!pad_placed) {
        if (player->y_position - player->floor > FLY_AIRBORNE_HEIGHT) {
            return dl;
        }
        pad_place();
    }

    guScaleF(pad_pose.world, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    pad_pose.world[3][0] = pad_pos[0];
    pad_pose.world[3][1] = pad_pos[1];
    pad_pose.world[3][2] = pad_pos[2];
    pad_pose.model = PAD_MODEL;
    pad_pose.block = pad_block;
    pad_pose.animation = -1;
    pad_pose.progress = 0.0f;
    pad_pose.gfx = pad_gfx[buffer];
    pad_pose.gfx_capacity = PAD_GFX_MAX;
    pad_pose.mtx = pad_mtx[buffer];
    pad_pose.mtx_capacity = PAD_MTX_MAX;
    if (jiggy_bk_model_pose(&pad_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, pad_gfx[buffer]);
    return dl;
}
