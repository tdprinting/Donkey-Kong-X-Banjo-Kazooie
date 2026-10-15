// Banjo-Kazooie's swimming (src/core2/bs/swim.c on the surface, bSwim.c underwater), for
// Banjo in DK64's water: he paddles and treads water on the surface and strokes along under
// it with Banjo-Kazooie's own animations and splashes, as the player or beside DK.
//
// Banjo still uses DK's swimming and its controls (jiggy_banjo3d.c); this only picks Banjo's
// animation from DK64's water states and their stages (control_state_progress):
//   0x4E, on the surface (func_global_asm_806D87FC): 0 just landed in the water and bobbing
//         up, 1 treading water, 2 swimming;
//   0x4F, under it (func_global_asm_806D8B08): 0 sinking after a plunge, 1 diving down from
//         the surface, 2 floating, 3 swimming, 4 and up a lunge forward (B).
// Leaving the water, DK64 goes back to its land states and Banjo to his land moves.
//
// Banjo-Kazooie keeps the player 70 units under the water line on the surface
// (func_80294378(3) in code_C4B0.c), and draws the model 60 units higher once it's in the
// water (func_802A744C, baModel_setYDisplacement), Kazooie's head and wings out. Banjo is
// drawn at those heights rather than DK's.

#include "common_structs.h"
#include "jiggy.h"

// DK64 player states (Actor control_state).
#define STATE_SURFACE 0x4E
#define STATE_UNDERWATER 0x4F
// Their stages (control_state_progress).
#define SURFACE_LANDING 0
#define UNDERWATER_SINKING 0
#define UNDERWATER_DIVING 1
#define UNDERWATER_FLOATING 2
#define UNDERWATER_SWIMMING 3
#define UNDERWATER_LUNGE 4

// Banjo-Kazooie animation assets (asset_e) and how long one loop takes, in seconds.
#define SWIM_ANIM_MOVE 0x39       // ASSET_39_ANIM_BSSWIM_MOVE: paddling on the surface
#define SWIM_ANIM_DIVE_ENTER 0x3C // ASSET_3C_ANIM_BSSWIM_DIVE_ENTER: heading under
#define SWIM_ANIM_DIVE_MOVE 0x3F  // ASSET_3F_ANIM_BSSWIM_DIVE_MOVE: the B stroke
#define SWIM_ANIM_TWIRL 0x4F      // ASSET_4F_ANIM_BSTWIRL: the end of it, for a splash-down
#define SWIM_ANIM_IDLE 0x57       // ASSET_57_ANIM_BSSWIM_IDLE: treading water
#define SWIM_ANIM_IDLE2 0x58      // ASSET_58_ANIM_BSSWIM_IDLE2
#define SWIM_ANIM_DIVE_IDLE 0x70  // ASSET_70_ANIM_BSSWIM_DIVE_IDLE: floating under water
#define SWIM_ANIM_DIVE_SLOW 0x71  // ASSET_71_ANIM_BSSWIM_DIVE_SLOW: the A paddle under water
#define SWIM_IDLE_SECONDS 1.2f
#define SWIM_DIVE_ENTER_SECONDS 1.0f
#define SWIM_DIVE_IDLE_SECONDS 2.0f
#define SWIM_DIVE_SLOW_SECONDS 0.75f
#define SWIM_TWIRL_SECONDS 1.2f
// Paddling: Banjo-Kazooie's horizontal speeds (bsSwimHorzVelocityMin and Max, in its units a
// second) and the loop's length at each (bsSwimDurationMin and Max).
#define SWIM_SPEED_MIN 30.0f
#define SWIM_SPEED_MAX 300.0f
#define SWIM_SECONDS_SLOW 1.2f
#define SWIM_SECONDS_FAST 0.7f
// Where a new paddle or tread starts (bsswim_swim_init, bsswim_idle_init), and a tread after
// a gentle landing (bsbswim_landingInWater_init).
#define SWIM_MOVE_START 0.8f
#define SWIM_IDLE_START 0.3f
#define SWIM_IDLE_LANDING_START 0.6412f
// A splash-down faster than this (Banjo-Kazooie units a second) twirls (from 0.8204 on).
#define SWIM_TWIRL_SPEED 80.0f
#define SWIM_TWIRL_START 0.8204f
// Treading water switches between the two tread animations here, in Banjo-Kazooie's order
// (__bsswim_updateIdleAnimation, bsSwimAnimations).
#define SWIM_IDLE_SWITCH 0.4348f
static const s16 tread_animations[] = {
    SWIM_ANIM_IDLE, SWIM_ANIM_IDLE, SWIM_ANIM_IDLE,  SWIM_ANIM_IDLE2, SWIM_ANIM_IDLE,
    SWIM_ANIM_IDLE, SWIM_ANIM_IDLE, SWIM_ANIM_IDLE2, SWIM_ANIM_IDLE2, SWIM_ANIM_IDLE2,
};
// The B stroke (bsbdiveb_update): it starts at 0.4626, wraps round, kicks at 0.1, glides
// three times slower from 0.2766 and is over at 0.4625.
#define STROKE_START 0.4626f
#define STROKE_KICK 0.1f
#define STROKE_GLIDE 0.2766f
#define STROKE_END 0.4625f
#define STROKE_SECONDS 1.0f
#define STROKE_GLIDE_SECONDS 3.0f
// Player speed where Banjo starts paddling, as walking in jiggy_banjo3d.c.
#define SWIM_PADDLE_SPEED 8.0f
// Banjo-Kazooie units to DK64 units (BANJO_SCALE in jiggy_banjo3d.c), the depth Banjo floats
// at on the surface and how much higher the model is drawn in the water, in DK64 units, and
// how fast Banjo's height catches up when DK64 moves the player between them.
#define BK_TO_DK 0.25f
#define SWIM_SURFACE_DEPTH (70.0f * BK_TO_DK)
#define SWIM_UNDERWATER_RAISE (60.0f * BK_TO_DK)
#define SWIM_LIFT_RATE 90.0f

// Banjo-Kazooie's appendage selectors for Kazooie's upper half (head and wings), out of the
// backpack while in the water (modelAppendages_setKazooiesUpperHalfVisibility).
static const u8 swim_kazooie_selectors[] = {0x01, 0x09, 0x0C, 0x0F};

extern u32 object_timer;

static s32 swim_state;   // The DK64 water state Banjo is in, or 0 out of the water.
static u8 swim_tread;    // The next tread animation, an index into tread_animations.
static u8 swim_stroke_wrapped; // The B stroke has wrapped round past 1.
static f32 swim_lift;    // How far above the player Banjo is drawn.
static f32 swim_fall_speed; // How fast the player was falling the frame before the water.
static f32 swim_under_seconds; // Time since diving, for Banjo's breath on coming up.
static u8 swim_splashdown; // Landed in the water from out of it, and not settled yet.

static s32 swim_in_water(Actor *player) {
    return player->control_state == STATE_SURFACE || player->control_state == STATE_UNDERWATER;
}

static void swim_play(s32 *animation, f32 *progress, s32 id, f32 start) {
    if (*animation != id) {
        *animation = id;
        *progress = start;
    }
}

// One of Banjo-Kazooie's random pitches between low and high (sfx_randf2), from a few steps
// so each is converted once.
static f32 swim_pitch(f32 low, f32 high) {
    return low + (high - low) * (f32)(object_timer % 5) * 0.25f;
}

static s32 swim_crossed(f32 before, f32 after, f32 point) {
    return before < point && after >= point;
}

// Keeps a looping animation's progress under 1.
static void swim_wrap(f32 *progress) {
    while (*progress >= 1.0f) {
        *progress -= 1.0f;
    }
}

// Treading water (bsswim_idle_update): the idle paddle's sound at the top of each loop, and
// the next tread animation from part-way through it, carrying on where the last left off.
static void swim_tread_water(s32 is_player, f32 seconds, s32 *animation, f32 *progress) {
    f32 before;

    if (*animation != SWIM_ANIM_IDLE && *animation != SWIM_ANIM_IDLE2) {
        swim_play(animation, progress, SWIM_ANIM_IDLE, SWIM_IDLE_START);
        swim_tread = 0;
    }
    before = *progress;
    *progress += seconds / SWIM_IDLE_SECONDS;
    if (*progress >= 1.0f) {
        swim_wrap(progress);
        before -= 1.0f;
    }
    if (swim_crossed(before, *progress, SWIM_IDLE_SWITCH)) {
        *animation = tread_animations[swim_tread];
        swim_tread = (swim_tread + 1) % (sizeof(tread_animations) / sizeof(tread_animations[0]));
    }
    // SFX_DC_IDLE_PADDLING at 0.01, 16000/32767, pitch 0.85 to 1.15.
    if (is_player && swim_crossed(before, *progress, 0.01f)) {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_IDLE_PADDLING, 16000.0f / 32767.0f, swim_pitch(0.85f, 1.15f));
    }
}

// Paddling on the surface (bsswim_swim_update): quicker the faster Banjo goes, with a splash
// for each arm (SFX_12_WATER_PADDLING_1 at 0.2 and 0.7, 22000/32767, pitch 0.9 to 1.1).
static void swim_paddle(s32 is_player, f32 speed, f32 seconds, s32 *animation, f32 *progress) {
    f32 bk_speed = speed / BK_TO_DK;
    f32 t = (bk_speed - SWIM_SPEED_MIN) / (SWIM_SPEED_MAX - SWIM_SPEED_MIN);
    f32 before;

    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    swim_play(animation, progress, SWIM_ANIM_MOVE, SWIM_MOVE_START);
    before = *progress;
    *progress += seconds / (SWIM_SECONDS_SLOW + (SWIM_SECONDS_FAST - SWIM_SECONDS_SLOW) * t);
    if (*progress >= 1.0f) {
        swim_wrap(progress);
        before -= 1.0f;
    }
    if (is_player && (swim_crossed(before, *progress, 0.2f) || swim_crossed(before, *progress, 0.7f))) {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_WATER_PADDLING, 22000.0f / 32767.0f, swim_pitch(0.9f, 1.1f));
    }
}

// Landing in the water (bsbswim_landingInWater_init): from a fast fall, the end of the twirl,
// held; from a gentle one, treading water from part-way in.
static void swim_land(s32 is_player, f32 seconds, s32 *animation, f32 *progress) {
    if (swim_state == 0) {
        if (swim_fall_speed / BK_TO_DK > SWIM_TWIRL_SPEED) {
            swim_play(animation, progress, SWIM_ANIM_TWIRL, SWIM_TWIRL_START);
        } else {
            swim_play(animation, progress, SWIM_ANIM_IDLE, SWIM_IDLE_LANDING_START);
            swim_tread = 0;
        }
    }
    if (*animation == SWIM_ANIM_TWIRL) {
        *progress += seconds / SWIM_TWIRL_SECONDS;
        if (*progress > 0.99f) {
            *progress = 0.99f;
        }
        return;
    }
    swim_tread_water(is_player, seconds, animation, progress);
}

// The B stroke (bsbdiveb_update), once through: a kick with a bloop (SFX_0_BLOOP, 22000/32767,
// pitch 1.2 to 1.4), then a glide, holding the end.
static void swim_stroke(s32 is_player, f32 seconds, s32 *animation, f32 *progress) {
    f32 before;

    if (*animation != SWIM_ANIM_DIVE_MOVE) {
        swim_play(animation, progress, SWIM_ANIM_DIVE_MOVE, STROKE_START);
        swim_stroke_wrapped = FALSE;
    }
    if (swim_stroke_wrapped && *progress >= STROKE_END) {
        *progress = STROKE_END;
        return;
    }
    before = *progress;
    *progress += seconds / ((swim_stroke_wrapped && *progress >= STROKE_GLIDE) ? STROKE_GLIDE_SECONDS : STROKE_SECONDS);
    if (*progress >= 1.0f) {
        swim_wrap(progress);
        swim_stroke_wrapped = TRUE;
        before = 0.0f;
    }
    if (swim_stroke_wrapped && is_player && swim_crossed(before, *progress, STROKE_KICK)) {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_BLOOP, 22000.0f / 32767.0f, swim_pitch(1.2f, 1.4f));
    }
}

// Swimming along under water (bsswim_divea_update): the A paddle, a bloop for each kick
// (SFX_0_BLOOP at 0.17 and 0.67, 10000/32767, pitch 1.9 and 1.7).
static void swim_underwater_paddle(s32 is_player, f32 seconds, s32 *animation, f32 *progress) {
    f32 before;

    swim_play(animation, progress, SWIM_ANIM_DIVE_SLOW, 0.0f);
    before = *progress;
    *progress += seconds / SWIM_DIVE_SLOW_SECONDS;
    if (*progress >= 1.0f) {
        swim_wrap(progress);
        before -= 1.0f;
    }
    if (!is_player) {
        return;
    }
    if (swim_crossed(before, *progress, 0.17f)) {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_BLOOP, 10000.0f / 32767.0f, 1.9f);
    }
    if (swim_crossed(before, *progress, 0.67f)) {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_BLOOP, 10000.0f / 32767.0f, 1.7f);
    }
}

// Heading under (bsSwim_dive_init and _update): Kazooie's "auw" (SFX_44_KAZOOIE_AUW,
// 22000/32767, pitch 1.117) and a bloop at 0.6 (SFX_0_BLOOP, 22000/32767). Returns 1 while
// it plays.
static s32 swim_dive(s32 is_player, f32 seconds, s32 *animation, f32 *progress) {
    f32 before;

    if (*animation != SWIM_ANIM_DIVE_ENTER) {
        if (swim_state != STATE_SURFACE) {
            // Already under, or plunged straight in.
            return FALSE;
        }
        swim_play(animation, progress, SWIM_ANIM_DIVE_ENTER, 0.0f);
        if (is_player) {
            jiggy_bk_sfx_play(JIGGY_BK_SFX_KAZOOIE_AUW, 22000.0f / 32767.0f, 1.117f);
        }
    }
    if (*progress >= 0.99f) {
        return FALSE;
    }
    before = *progress;
    *progress += seconds / SWIM_DIVE_ENTER_SECONDS;
    if (*progress > 0.99f) {
        *progress = 0.99f;
    }
    if (is_player && swim_crossed(before, *progress, 0.6f)) {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_BLOOP, 22000.0f / 32767.0f, 1.0f);
    }
    return TRUE;
}

// Eases Banjo's height toward `target`, so he doesn't jump when DK64 moves the player.
static void swim_lift_toward(f32 target, f32 seconds) {
    f32 step = SWIM_LIFT_RATE * seconds;

    if (swim_lift < target - step) {
        swim_lift += step;
    } else if (swim_lift > target + step) {
        swim_lift -= step;
    } else {
        swim_lift = target;
    }
}

// Called from banjo_animate (jiggy_banjo3d.c) first. While the player is in the water, picks
// Banjo's animation and plays his swimming sounds when he's the player, and returns 1; Banjo's
// other moves are off then.
s32 jiggy_banjo_swim_animate(Actor *player, s32 is_player, f32 seconds, s32 *animation, f32 *progress) {
    s32 state = player->control_state;
    s32 stage = player->control_state_progress;
    f32 target;

    if (!swim_in_water(player)) {
        swim_state = 0;
        swim_lift = 0.0f;
        swim_splashdown = FALSE;
        swim_under_seconds = 0.0f;
        swim_fall_speed = -player->y_velocity;
        return FALSE;
    }
    if (state == STATE_UNDERWATER) {
        swim_under_seconds += seconds;
    } else if (swim_state == STATE_UNDERWATER) {
        // Coming up for air (bsswim_idle_init): Banjo catches his breath, louder the longer he
        // was under (SFX_AF_BANJO_CATCHING_BREATH, 8000 to 30000/32767 as his air runs out over
        // a minute).
        if (is_player) {
            f32 spent = swim_under_seconds / 60.0f;
            if (spent > 1.0f) {
                spent = 1.0f;
            }
            jiggy_bk_sfx_play(JIGGY_BK_SFX_BANJO_CATCHING_BREATH, (8000.0f + 22000.0f * spent) / 32767.0f, 1.0f);
        }
    }
    if (state != STATE_UNDERWATER) {
        swim_under_seconds = 0.0f;
    }

    // DK64 also starts the surface state at its landing stage on coming up from under water.
    if (swim_state == 0) {
        swim_splashdown = (state == STATE_SURFACE && stage == SURFACE_LANDING);
    } else if (state != STATE_SURFACE || stage != SURFACE_LANDING) {
        swim_splashdown = FALSE;
    }

    if (state == STATE_SURFACE) {
        if (swim_splashdown && (swim_state == 0 || *animation == SWIM_ANIM_TWIRL)) {
            swim_land(is_player, seconds, animation, progress);
        } else if (player->unkB8 < SWIM_PADDLE_SPEED) {
            swim_tread_water(is_player, seconds, animation, progress);
        } else {
            swim_paddle(is_player, player->unkB8, seconds, animation, progress);
        }
        // Splashing down, Banjo goes under with the player; then floats at his own depth.
        target = swim_splashdown ? SWIM_UNDERWATER_RAISE : (player->unkAC - SWIM_SURFACE_DEPTH) - player->y_position;
    } else {
        if (stage == UNDERWATER_DIVING && swim_dive(is_player, seconds, animation, progress)) {
            // Heading under.
        } else if (stage >= UNDERWATER_LUNGE) {
            swim_stroke(is_player, seconds, animation, progress);
        } else if (stage == UNDERWATER_SWIMMING) {
            swim_underwater_paddle(is_player, seconds, animation, progress);
        } else {
            // Floating, or sinking after a plunge (bsbdive_idle_init).
            swim_play(animation, progress, SWIM_ANIM_DIVE_IDLE, 0.0f);
            *progress += seconds / SWIM_DIVE_IDLE_SECONDS;
            swim_wrap(progress);
        }
        target = SWIM_UNDERWATER_RAISE;
    }
    if (swim_state == 0) {
        swim_lift = target;
    } else {
        swim_lift_toward(target, seconds);
    }
    swim_state = state;
    return TRUE;
}

// Called from banjo_set_selectors (jiggy_banjo3d.c): Kazooie's head and wings out under water
// and splashing down, as in Banjo-Kazooie.
void jiggy_banjo_swim_selectors(s32 *selectors) {
    s32 i;

    if (swim_state == 0 || (swim_state == STATE_SURFACE && !swim_splashdown)) {
        return;
    }
    for (i = 0; i < (s32)sizeof(swim_kazooie_selectors); i++) {
        selectors[swim_kazooie_selectors[i]] = 1;
    }
}

// How far above the player to draw Banjo while swimming (jiggy_banjo_draw in jiggy_banjo3d.c).
f32 jiggy_banjo_swim_lift(void) {
    return swim_state == 0 ? 0.0f : swim_lift;
}

// Whether Banjo is swimming, so his other moves stay off (jiggy_banjo3d.c).
s32 jiggy_banjo_swimming(Actor *player) {
    return swim_in_water(player);
}
