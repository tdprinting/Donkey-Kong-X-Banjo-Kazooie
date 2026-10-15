// Banjo-Kazooie's Shock Spring Jump (src/core2/bs/bShock.c), for when Banjo is the player: a
// Shock Spring Jump pad (ASSET_489_MODEL_SHOCKSPRING_PAD, chShockJump in src/core2/code_43A40.c)
// sits on the grass in the Training Grounds, and pressing A standing on it springs Banjo high.
//
// As in Banjo-Kazooie (bsjump_init with BA_FLAG_2_ON_SPRING_PAD, then bsbshock_charge and
// bsbshock): A on the pad is a jump with Banjo's "ahoo"; at the top Kazooie puts her legs out
// and crouches for the spring, lands, and launches with a boing and Banjo's "huiii".
// Releasing A as Banjo comes down onto the pad (before the spring point of the crouch) stops
// the spring, as Banjo-Kazooie does.
//
// Banjo still uses DK's movement and physics (jiggy_banjo3d.c). The hop is DK's own jump. The
// spring is DK's jump too: at the spring point this hands DK an A press (through
// func_global_asm_806DFF34 in jiggy_banjo_eggs.c, where the player's buttons are read), and
// when DK's jump starts it multiplies its launch speed (PlayerAdditionalActorData unk54, which
// the jump state copies to y_velocity) by the Shock Spring Jump's against Banjo-Kazooie's own
// jump: 1250 units a second against 710, and gravity 1200 against 1350, for the same 3.5
// times the height in DK's gravity.
//
// The pad is drawn with the player (jiggy_banjo_draw), like jiggy_jinjos.c draws its Jinjos.
// TODO: pads on more maps.

#include "common_structs.h"
#include "jiggy.h"

#define PAD_MODEL 0x489 // ASSET_489_MODEL_SHOCKSPRING_PAD
// A converted pad is about 6 KiB; its pose is under 80 commands and loads only the root
// matrix (it has no skeleton).
#define PAD_BLOCK_MAX 0x4000
#define PAD_GFX_MAX 128
#define PAD_MTX_MAX 4
// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo. The pad is 252 units
// across and 19 high in Banjo-Kazooie.
#define BK_TO_DK64 0.25f
#define PAD_RADIUS (126.0f * BK_TO_DK64)
#define PAD_HEIGHT (19.0f * BK_TO_DK64)
// How far the player's feet can be above or below the pad's base and still stand on it.
#define PAD_FEET_ABOVE 15.0f
#define PAD_FEET_BELOW 5.0f
// Placing the pad on the floor under its spot: looked for from this far above the spot, and
// only trusted this close to the spot's height (as jiggy_jinjos.c places its Jinjos).
#define PAD_FLOOR_PROBE 20.0f
#define PAD_FLOOR_MAX_STEP 40.0f
// Height above the floor that counts as in the air, as in jiggy_banjo3d.c.
#define SHOCK_AIRBORNE_HEIGHT 6.0f
// Player speed where Banjo walks, as in jiggy_banjo3d.c.
#define SHOCK_WALK_SPEED 8.0f

// Where the pad goes in the Training Grounds: on the grass in front of the treehouse, clear
// of the notes, Jinjos and Honeycombs placed there (the
// player starts on its deck, at about (1346, 227, 1098)). Snapped to the floor when placed.
#define PAD_MAP MAP_TRAINING_GROUNDS
#define PAD_X 1215.0f
#define PAD_Y 30.0f
#define PAD_Z 895.0f

// Banjo-Kazooie animation assets (asset_e) and how they play.
// The hop: ASSET_8_ANIM_BSJUMP from 0.3, over 1.9 s, up to 0.5042 (bsjump_init), then the
// crouch.
#define SHOCK_ANIM_HOP 0x08
#define SHOCK_HOP_START 0.3f
#define SHOCK_HOP_END 0.5042f
#define SHOCK_HOP_SECONDS 1.9f
// The crouch: ASSET_48_ANIM_BSBSHOCK_CHARGE, looping its first 0.1061 over 4.2 s in the air,
// then once the ground nears (falling, within 130 units: 32.5 here) playing on over 2.8 s.
// The spring comes at 0.3637; without it the crouch slows to 1.4 s at 0.4036 and plays out.
// Releasing A within 60 units (15 here) of the ground before the spring point stops the
// spring (bsbshock_charge_update).
#define SHOCK_ANIM_CHARGE 0x48
#define SHOCK_CHARGE_LOOP_END 0.1061f
#define SHOCK_CHARGE_LOOP_SECONDS 4.2f
#define SHOCK_CHARGE_SECONDS 2.8f
#define SHOCK_CHARGE_SLOW_AT 0.4036f
#define SHOCK_CHARGE_SLOW_SECONDS 1.4f
#define SHOCK_CHARGE_PULL_AT 0.11f
#define SHOCK_CHARGE_SPRING_AT 0.3637f
#define SHOCK_CHARGE_DROP_HEIGHT 32.5f
#define SHOCK_CHARGE_RELEASE_HEIGHT 15.0f
// The spring: ASSET_49_ANIM_BSBSHOCK_JUMP from 0.5304 over 0.8 s, Banjo's "huiii" at 0.7
// (bsbshock_init, bsbshock_update).
#define SHOCK_ANIM_JUMP 0x49
#define SHOCK_JUMP_START 0.5304f
#define SHOCK_JUMP_SECONDS 0.8f
#define SHOCK_JUMP_HUIII_AT 0.7f
// Banjo's fall (ASSET_B0, jiggy_banjo3d.c's BANJO_ANIM_FALL), handed back once the spring
// tops out (bsbshock_update goes to BS_2F_FALL).
#define SHOCK_ANIM_FALL 0xB0
// The spring's launch speed against DK's jump: (1250 / 710) * sqrt(1350 / 1200), so the
// height is Banjo-Kazooie's 3.5 jumps under DK's gravity.
#define SHOCK_JUMP_SCALE 1.867f
// Frames DK's jump has to start in once handed the A press, or the spring is given up.
#define SHOCK_LAUNCH_FRAMES 10

// DK64 player states (Actor control_state).
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
#define STATE_JUMP 0x17

// Kazooie's upper half and her legs and feet, out of the backpack, for the crouch and the
// spring (modelAppendages_loadAppendage in src/core2/modelappendages.c).
static const u8 shock_selectors[] = {0x01, 0x09, 0x0C, 0x0F, 0x02, 0x0A, 0x0D, 0x10};

extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u16 newly_pressed_input[];
extern OSContPad D_global_asm_807ECD58; // Buttons held this frame.
extern f32 D_global_asm_807531FC[];     // Kong walking top speed.
extern s16 D_global_asm_807FD584;       // Current Kong, an index into the Kong tables.
// The floor under (x, z), as the game looks for one (func_global_asm_80667180): writes its
// height to *y, leaving it as it was if there's none. The return value isn't reliable.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);

typedef enum {
    PAD_UNLOADED,
    PAD_READY,
    PAD_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} PadLoadState;

typedef enum {
    SHOCK_OFF,
    SHOCK_HOP,    // DK's jump off the pad.
    SHOCK_CHARGE, // In the air, Kazooie's legs out.
    SHOCK_CROUCH, // Coming down onto the pad and crouching for the spring.
    SHOCK_JUMP,   // The spring.
} ShockState;

static u8 pad_block[PAD_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx pad_gfx[2][PAD_GFX_MAX];
static Mtx pad_mtx[2][PAD_MTX_MAX];
static JiggyBkPose pad_pose;
static PadLoadState pad_state = PAD_UNLOADED;
static s32 pad_last_map = -1;
static s32 pad_placed;
static f32 pad_pos[3];

static ShockState shock_state;
static u8 shock_spring;      // Still springs (D_8037D380): A wasn't let go near the ground.
static u8 shock_a_held;      // A, held last frame.
static u8 shock_boosted;     // DK's jump for the spring has been sped up.
static u8 shock_left_ground; // And has left the ground.
static u8 shock_launch_wait; // Frames left for DK's jump to start.
static u8 shock_inject;      // Button reads left to hand DK an A press in.

static f32 shock_walk_top_speed(void) {
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

// Whether the player stands on the pad.
static s32 pad_under(Actor *player) {
    f32 dx = player->x_position - pad_pos[0];
    f32 dz = player->z_position - pad_pos[2];
    f32 dy = player->y_position - pad_pos[1];

    return pad_state == PAD_READY && pad_placed && current_map == PAD_MAP &&
           player->y_position - player->floor <= SHOCK_AIRBORNE_HEIGHT && dx * dx + dz * dz < PAD_RADIUS * PAD_RADIUS &&
           dy < PAD_FEET_ABOVE && dy > -PAD_FEET_BELOW;
}

static void shock_play(s32 *animation, f32 *progress, s32 id, f32 start) {
    if (*animation != id) {
        *animation = id;
        *progress = start;
    }
}

static s32 shock_crossed(f32 before, f32 after, f32 at) {
    return before < at && after >= at;
}

static void shock_stop(Actor *player) {
    PlayerAdditionalActorData *paad = player->PaaD;

    // The crouch held DK still; give DK its walking speed back.
    if (shock_state == SHOCK_CROUCH && paad != NULL &&
        (player->control_state == STATE_IDLE || player->control_state == STATE_WALK)) {
        paad->unk38 = shock_walk_top_speed();
    }
    shock_state = SHOCK_OFF;
    shock_inject = 0;
}

// The spring: DK is handed an A press to jump with, sped up once the jump starts.
static void shock_launch(s32 *animation, f32 *progress) {
    shock_state = SHOCK_JUMP;
    shock_boosted = FALSE;
    shock_left_ground = FALSE;
    shock_launch_wait = SHOCK_LAUNCH_FRAMES;
    shock_inject = 2; // The held buttons, then the newly pressed ones.
    *animation = SHOCK_ANIM_JUMP;
    *progress = SHOCK_JUMP_START;
    // SFX_E_SHOCKSPRING_BOING (sfxsource_playHighPriority: 22000, as recorded).
    jiggy_bk_sfx_play(JIGGY_BK_SFX_SHOCKSPRING_BOING, 22000.0f / 32767.0f, 1.0f);
}

// Speeds up DK's jump once it starts: before the jump state copies its launch speed to
// y_velocity (control_state_progress 0 and 1), that speed; after, y_velocity itself.
static void shock_boost(Actor *player) {
    PlayerAdditionalActorData *paad = player->PaaD;

    if (shock_boosted || player->control_state != STATE_JUMP || paad == NULL) {
        return;
    }
    if (player->control_state_progress <= 1) {
        paad->unk54 *= SHOCK_JUMP_SCALE;
    } else if (player->y_velocity > 0.0f) {
        player->y_velocity *= SHOCK_JUMP_SCALE;
    }
    shock_boosted = TRUE;
}

// Called from banjo_animate (jiggy_banjo3d.c) after the Talon Trot and Wonderwing, before
// Banjo's air moves. Starts the move on the pad and, while it plays, picks Banjo's animation.
// Returns 1 when it did.
s32 jiggy_banjo_shock_animate(Actor *player, s32 is_player, s32 airborne, f32 seconds, s32 *animation,
                              f32 *progress) {
    PlayerAdditionalActorData *paad = player->PaaD;
    u16 pressed = newly_pressed_input[0];
    s32 a_held = (D_global_asm_807ECD58.button & A_BUTTON) != 0;
    s32 a_released = shock_a_held && !a_held;
    f32 height = player->y_position - player->floor;
    f32 before;

    shock_a_held = a_held;
    if (!is_player || current_map != PAD_MAP) {
        if (shock_state != SHOCK_OFF) {
            shock_stop(player);
        }
        return FALSE;
    }
    if (shock_state == SHOCK_OFF) {
        if (!(pressed & A_BUTTON) || !pad_under(player) ||
            (player->control_state != STATE_IDLE && player->control_state != STATE_WALK)) {
            return FALSE;
        }
        // DK jumps on this A: the hop, with Banjo's "ahoo", Banjo-Kazooie's jump sound on a
        // spring pad (SFX_33_BANJO_AHOO, func_8030E6D4: 0x7FF8, as recorded).
        shock_state = SHOCK_HOP;
        shock_spring = TRUE;
        jiggy_bk_sfx_play(JIGGY_BK_SFX_BANJO_AHOO, 32760.0f / 32767.0f, 1.0f);
        *animation = SHOCK_ANIM_HOP;
        *progress = SHOCK_HOP_START;
        return TRUE;
    }
    // In the air, B, Z or a fresh A hands over to Banjo's air moves, as Banjo-Kazooie allows a
    // Feathery Flap or Beak Buster out of the crouch and the spring.
    if (airborne && (pressed & (A_BUTTON | B_BUTTON | Z_TRIG)) && shock_state != SHOCK_HOP) {
        shock_stop(player);
        return FALSE;
    }

    switch (shock_state) {
        case SHOCK_HOP:
            shock_play(animation, progress, SHOCK_ANIM_HOP, SHOCK_HOP_START);
            *progress += seconds / SHOCK_HOP_SECONDS;
            if (*progress < SHOCK_HOP_END) {
                return TRUE;
            }
            // Kazooie's legs come out, with her "auw" (basfx_80299BD4: SFX_44_KAZOOIE_AUW,
            // 22000, pitch 1.117).
            shock_state = SHOCK_CHARGE;
            *animation = SHOCK_ANIM_CHARGE;
            *progress = 0.0f;
            jiggy_bk_sfx_play(JIGGY_BK_SFX_KAZOOIE_AUW, 22000.0f / 32767.0f, 1.117f);
            // Fall through: it may already be near the ground.
        case SHOCK_CHARGE:
            shock_play(animation, progress, SHOCK_ANIM_CHARGE, 0.0f);
            if (airborne && !(player->y_velocity < 0.0f && height < SHOCK_CHARGE_DROP_HEIGHT)) {
                *progress += seconds / SHOCK_CHARGE_LOOP_SECONDS;
                while (*progress >= SHOCK_CHARGE_LOOP_END) {
                    *progress -= SHOCK_CHARGE_LOOP_END;
                }
                return TRUE;
            }
            shock_state = SHOCK_CROUCH;
            // Fall through.
        case SHOCK_CROUCH:
            shock_play(animation, progress, SHOCK_ANIM_CHARGE, 0.0f);
            if (!airborne && paad != NULL) {
                // Banjo-Kazooie holds Banjo still on the pad for the spring.
                paad->unk38 = 0.0f;
                player->unkB8 = 0.0f;
            }
            if (*progress < SHOCK_CHARGE_SPRING_AT && height < SHOCK_CHARGE_RELEASE_HEIGHT && a_released) {
                shock_spring = FALSE;
            }
            before = *progress;
            *progress += seconds / (before < SHOCK_CHARGE_SLOW_AT ? SHOCK_CHARGE_SECONDS : SHOCK_CHARGE_SLOW_SECONDS);
            if (shock_crossed(before, *progress, SHOCK_CHARGE_PULL_AT)) {
                // SFX_2C_PULLING_NOISE: Banjo-Kazooie starts it at pitch 1.4 and slides it down
                // (func_8030E04C(.., 1.4, 0.4, -1.2)); played here at its starting pitch.
                jiggy_bk_sfx_play(JIGGY_BK_SFX_PULLING_NOISE, 22000.0f / 32767.0f, 1.4f);
            }
            if (shock_spring && *progress >= SHOCK_CHARGE_SPRING_AT) {
                if (airborne) {
                    // Still coming down: wait for the ground to spring from.
                    *progress = SHOCK_CHARGE_SPRING_AT;
                    return TRUE;
                }
                shock_launch(animation, progress);
                return TRUE;
            }
            if (*progress >= 0.99f) {
                shock_stop(player);
                return FALSE;
            }
            return TRUE;
        case SHOCK_JUMP:
            shock_boost(player);
            if (shock_boosted && airborne && player->y_velocity > 0.0f) {
                shock_left_ground = TRUE;
            }
            if (!shock_left_ground) {
                if (shock_launch_wait == 0 || --shock_launch_wait == 0) {
                    // DK didn't jump (something else had the player).
                    shock_stop(player);
                    return FALSE;
                }
            } else if (player->y_velocity < 0.0f || !airborne) {
                // Topped out: Banjo's own fall takes over (or he's down already).
                shock_stop(player);
                if (airborne) {
                    *animation = SHOCK_ANIM_FALL;
                    *progress = 0.0f;
                }
                return FALSE;
            }
            before = *progress;
            *progress += seconds / SHOCK_JUMP_SECONDS;
            if (*progress > 0.99f) {
                *progress = 0.99f;
            }
            if (shock_crossed(before, *progress, SHOCK_JUMP_HUIII_AT)) {
                // SFX_53_BANJO_HUIII (sfxsource_playHighPriority: 22000, as recorded).
                jiggy_bk_sfx_play(JIGGY_BK_SFX_BANJO_HUIII, 22000.0f / 32767.0f, 1.0f);
            }
            return TRUE;
        default:
            return FALSE;
    }
}

// Called from banjo_set_selectors (jiggy_banjo3d.c): Kazooie's upper half, legs and feet for
// the crouch and the spring.
void jiggy_banjo_shock_selectors(s32 *selectors) {
    s32 i;

    if (shock_state != SHOCK_CHARGE && shock_state != SHOCK_CROUCH && shock_state != SHOCK_JUMP) {
        return;
    }
    for (i = 0; i < (s32)sizeof(shock_selectors); i++) {
        selectors[shock_selectors[i]] = 1;
    }
}

// How far up to draw Banjo: standing on the pad, on top of it rather than in it.
f32 jiggy_banjo_shock_lift(Actor *player) {
    return pad_under(player) ? PAD_HEIGHT : 0.0f;
}

// Buttons to add to the player's, where DK64 reads them (func_global_asm_806DFF34 in
// jiggy_banjo_eggs.c): A for the spring's jump, held and newly pressed, for one frame.
u16 jiggy_banjo_shock_buttons(void) {
    if (shock_inject == 0 || cc_number_of_players != 1 || cc_player_index != 0) {
        return 0;
    }
    shock_inject--;
    return A_BUTTON;
}

// Called once a frame from jiggy_banjo_draw (jiggy_banjo3d.c), after Banjo: draws the pad.
Gfx *jiggy_banjo_shock_pad_draw(Gfx *dl, Actor *player) {
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
        if (player->y_position - player->floor > SHOCK_AIRBORNE_HEIGHT) {
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
