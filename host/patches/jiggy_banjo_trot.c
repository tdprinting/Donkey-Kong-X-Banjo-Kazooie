// Banjo-Kazooie's Talon Trot (src/core2/bs/bTrot.c), for when Banjo is the player: Kazooie
// puts her legs out and carries Banjo, faster than he walks.
//
// In Banjo-Kazooie it's Z + C-left, held as long as Z is. Holding Z in DK64 keeps the player
// crouched and still, so here it toggles: crouch (Z) and press C-left to start trotting, and
// press Z or C-left again to stop. Swapping back to DK stops it too. Z in the air stops it
// and does the Beak Buster, as in Banjo-Kazooie. DK's own attacks and backflip stop it.
//
// Banjo still uses DK's movement and physics (jiggy_banjo3d.c): the trot raises DK's top
// speed (PlayerAdditionalActorData unk38, which DK's walk sets from the Kong's walking top
// speed) and plays the trot's own animations, with Kazooie's legs and upper half shown.
//
// Kazooie leads while trotting, so Banjo-Kazooie draws the model turned round and turns the
// player round at the start and end (baModel_setDirection, PLAYER_MODEL_DIR_KAZOOIE): Banjo
// stays facing the same way on screen and the trot heads off the way Kazooie faces. This
// does the same with the player's y_rotation.

#include "common_structs.h"
#include "jiggy.h"

// Banjo-Kazooie animation assets (asset_e) and how long one loop takes, in seconds.
#define TROT_ANIM_ENTER 0x16 // ASSET_16_ANIM_BSBTROT_ENTER
#define TROT_ANIM_IDLE 0x26  // ASSET_26_ANIM_BSBTROT_IDLE
#define TROT_ANIM_WALK 0x15  // ASSET_15_ANIM_BSBTROT_WALK
#define TROT_ANIM_JUMP 0x27  // ASSET_27_ANIM_BSBTROR_JUMP
#define TROT_ANIM_EXIT 0x07  // ASSET_7_ANIM_BSBTROT_EXIT
#define TROT_ENTER_SECONDS 1.0f
#define TROT_IDLE_SECONDS 1.2f
#define TROT_EXIT_SECONDS 0.6f
#define TROT_JUMP_SECONDS 1.4f
#define TROT_LAND_SECONDS 0.9f
// Where the trot jump's stages end, as fractions of the animation (bsbtrot_jump_update):
// take off, hang at the top, legs out as the ground nears, then land.
#define TROT_JUMP_START 0.2f
#define TROT_JUMP_RISE_END 0.4002f
#define TROT_JUMP_HANG_END 0.4653f
#define TROT_JUMP_HANG_SECONDS 10.0f
#define TROT_JUMP_DROP_END 0.7328f
#define TROT_JUMP_LAND_END 0.8798f
// Height above the floor, falling, where the legs come out (func_8028B254(0x8C): 140
// Banjo-Kazooie units).
#define TROT_JUMP_DROP_HEIGHT 35.0f
// Banjo-Kazooie's trot tops out at 700 units a second against 500 for a walk.
#define TROT_SPEED_SCALE 1.4f
// The distance one trot loop covers, in DK64 units, and the quickest a loop plays
// (gBsbTrotFastestWalkDuration).
#define TROT_STRIDE 75.0f
#define TROT_FASTEST_SECONDS 0.34f
// Player speed where the trot starts walking, as in jiggy_banjo3d.c.
#define TROT_WALK_SPEED 8.0f
// Enter animation progress after which moving starts the walk (bsbtrot_enter_update).
#define TROT_ENTER_INTERRUPT 0.5f

// DK64 player states (Actor control_state).
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
#define STATE_JUMP 0x17
#define STATE_FALL 0x1E
#define STATE_ATTACK 0x26
#define STATE_RUNNING_ATTACK 0x29
#define STATE_CROUCH_ENTER 0x3C
#define STATE_CROUCH 0x3D
#define STATE_BACKFLIP 0x3E

// Banjo-Kazooie's appendage selectors for Kazooie's upper half, and her legs and feet
// (modelAppendages_loadAppendage in src/core2/modelappendages.c).
static const u8 trot_selectors[] = {0x01, 0x09, 0x0C, 0x0F, 0x02, 0x0A, 0x0D, 0x10};

extern u16 newly_pressed_input[];
extern f32 D_global_asm_807531FC[]; // Kong walking top speed.
extern s16 D_global_asm_807FD584;   // Current Kong, an index into the Kong tables.

typedef enum {
    TROT_OFF,
    TROT_ENTER,
    TROT_ON,
    TROT_EXIT,
} TrotState;

static TrotState trot_state;

static void trot_play(s32 *animation, f32 *progress, s32 id, f32 start) {
    if (*animation != id) {
        *animation = id;
        *progress = start;
    }
}

// Turns the player round, so Banjo keeps facing the same way when Kazooie takes the lead or
// hands it back.
// Each Talon Trot step (bsbtrot_walk_update): Kazooie's "ra" and a footfall, at the two points
// of the walk where a foot lands. The footfall is Banjo-Kazooie's ordinary-ground one (it picks
// others for other surfaces), its pitch alternating between steps.
#define TROT_STEP_1 0.2781f
#define TROT_STEP_2 0.7781f
static u8 trot_step_alt;

static void trot_step_sound(f32 ra_pitch) {
    trot_step_alt ^= 1;
    jiggy_bk_sfx_play(JIGGY_BK_SFX_KAZOOIE_RA, 22000.0f / 32767.0f, ra_pitch);
    jiggy_bk_sfx_play(JIGGY_BK_SFX_FOOTSTEP, 9000.0f / 32767.0f, trot_step_alt ? 1.2f : 1.4f);
}

static void trot_step_sounds(f32 before, f32 after) {
    if (before < TROT_STEP_1 && after >= TROT_STEP_1) {
        trot_step_sound(0.96f);
    }
    if (before < TROT_STEP_2 && after >= TROT_STEP_2) {
        trot_step_sound(1.04f);
    }
}

static void trot_turn_round(Actor *player) {
    player->y_rotation = (player->y_rotation + 0x800) & 0xFFF;
}

static f32 trot_walk_top_speed(void) {
    return D_global_asm_807531FC[D_global_asm_807FD584];
}

static void trot_stop(Actor *player, s32 turn_round) {
    PlayerAdditionalActorData *paad = player->PaaD;

    trot_state = TROT_OFF;
    if (turn_round) {
        trot_turn_round(player);
    }
    // DK's walk sets its own top speed again on its next state; put it back now if walking.
    if (paad != NULL && (player->control_state == STATE_IDLE || player->control_state == STATE_WALK)) {
        paad->unk38 = trot_walk_top_speed();
    }
}

// Faster on the ground, and keeps that speed through a jump.
static void trot_set_speed(Actor *player) {
    PlayerAdditionalActorData *paad = player->PaaD;
    f32 top_speed = trot_walk_top_speed() * TROT_SPEED_SCALE;

    if (paad == NULL) {
        return;
    }
    if (player->control_state == STATE_IDLE || player->control_state == STATE_WALK) {
        paad->unk38 = top_speed;
    } else if ((player->control_state == STATE_JUMP || player->control_state == STATE_FALL) &&
               paad->unk38 < top_speed) {
        paad->unk38 = top_speed;
    }
}

static void trot_jump(Actor *player, f32 seconds, s32 *animation, f32 *progress) {
    f32 end;
    f32 rate = 1.0f / TROT_JUMP_SECONDS;

    if (*animation != TROT_ANIM_JUMP) {
        // Walked off a ledge: straight to the hang.
        trot_play(animation, progress, TROT_ANIM_JUMP,
                  player->control_state == STATE_FALL ? TROT_JUMP_HANG_END : TROT_JUMP_START);
    }
    if (*progress < TROT_JUMP_RISE_END) {
        end = TROT_JUMP_RISE_END;
    } else if (player->y_velocity < 0.0f && player->y_position - player->floor < TROT_JUMP_DROP_HEIGHT) {
        end = TROT_JUMP_DROP_END;
    } else {
        end = TROT_JUMP_HANG_END;
        rate = 1.0f / TROT_JUMP_HANG_SECONDS;
    }
    if (*progress < end) {
        *progress += seconds * rate;
        if (*progress > end) {
            *progress = end;
        }
    }
}

// Called from banjo_animate (jiggy_banjo3d.c) before Banjo's own moves. Starts and stops the
// trot, sets its speed and, while trotting, picks Banjo's animation. Returns 1 when it did.
s32 jiggy_banjo_trot_animate(Actor *player, s32 is_player, s32 airborne, f32 seconds, s32 *animation,
                             f32 *progress) {
    u16 pressed = newly_pressed_input[0];
    s32 state = player->control_state;
    f32 speed = player->unkB8;
    f32 rate;
    f32 before;

    if (!is_player) {
        if (trot_state != TROT_OFF) {
            trot_stop(player, FALSE);
        }
        return FALSE;
    }
    if (trot_state == TROT_OFF) {
        if (!(pressed & L_CBUTTONS) || airborne || (state != STATE_CROUCH_ENTER && state != STATE_CROUCH)) {
            return FALSE;
        }
        trot_state = TROT_ENTER;
        trot_turn_round(player);
    } else if ((pressed & Z_TRIG) || state == STATE_ATTACK || state == STATE_RUNNING_ATTACK ||
               state == STATE_BACKFLIP) {
        // Z crouches (or, in the air, Beak Busts) and DK's attacks show Banjo's own moves.
        trot_stop(player, TRUE);
        return FALSE;
    } else if ((pressed & L_CBUTTONS) && trot_state != TROT_EXIT) {
        trot_state = TROT_EXIT;
    }
    if (trot_state == TROT_EXIT && (airborne || speed >= TROT_WALK_SPEED)) {
        // Kazooie only puts Banjo down standing still.
        trot_stop(player, TRUE);
        return FALSE;
    }
    trot_set_speed(player);

    switch (trot_state) {
        case TROT_ENTER:
            trot_play(animation, progress, TROT_ANIM_ENTER, 0.0f);
            *progress += seconds / TROT_ENTER_SECONDS;
            if (*progress >= 0.99f || airborne || (*progress > TROT_ENTER_INTERRUPT && speed >= TROT_WALK_SPEED)) {
                trot_state = TROT_ON;
            } else {
                return TRUE;
            }
            break;
        case TROT_EXIT:
            trot_play(animation, progress, TROT_ANIM_EXIT, 0.0f);
            *progress += seconds / TROT_EXIT_SECONDS;
            if (*progress >= 0.99f) {
                trot_stop(player, TRUE);
                return FALSE;
            }
            return TRUE;
        default:
            break;
    }

    if (airborne) {
        trot_jump(player, seconds, animation, progress);
        return TRUE;
    }
    if (*animation == TROT_ANIM_JUMP && *progress < TROT_JUMP_LAND_END && speed < TROT_WALK_SPEED) {
        // Landed standing still: finish the landing.
        if (*progress < TROT_JUMP_DROP_END) {
            *progress = TROT_JUMP_DROP_END;
        }
        *progress += seconds / TROT_LAND_SECONDS;
        return TRUE;
    }
    if (speed < TROT_WALK_SPEED) {
        trot_play(animation, progress, TROT_ANIM_IDLE, 0.0f);
        *progress += seconds / TROT_IDLE_SECONDS;
    } else {
        trot_play(animation, progress, TROT_ANIM_WALK, 0.0f);
        rate = speed / TROT_STRIDE;
        if (rate > 1.0f / TROT_FASTEST_SECONDS) {
            rate = 1.0f / TROT_FASTEST_SECONDS;
        }
        before = *progress;
        *progress += seconds * rate;
        if (is_player) {
            trot_step_sounds(before, *progress);
        }
    }
    while (*progress >= 1.0f) {
        *progress -= 1.0f;
    }
    return TRUE;
}

// Called from banjo_set_selectors (jiggy_banjo3d.c): Kazooie's whole body while trotting.
void jiggy_banjo_trot_selectors(s32 *selectors) {
    s32 i;

    if (trot_state == TROT_OFF) {
        return;
    }
    for (i = 0; i < (s32)sizeof(trot_selectors); i++) {
        selectors[trot_selectors[i]] = 1;
    }
}

// Degrees to turn Banjo's model by: round while trotting, so Kazooie faces where the player
// heads (jiggy_banjo_draw in jiggy_banjo3d.c).
f32 jiggy_banjo_trot_yaw(void) {
    return trot_state == TROT_OFF ? 0.0f : 180.0f;
}
