// Banjo-Kazooie's Wonderwing (src/core2/bs/bWhirl.c), for when Banjo is the player: Kazooie
// wraps her wings round Banjo, enlarged and shining with gold sparkles, and nothing can hurt
// him while it lasts.
//
// In Banjo-Kazooie it's Z + C-right, held as long as Z is. Holding Z in DK64 keeps the player
// crouched and still, so here it toggles, like the Talon Trot (jiggy_banjo_trot.c): crouch (Z)
// and press C-right to start it, and press C-right again to stop (Z, or DK's attacks and
// backflip, stop it at once). Swapping back to DK stops it too.
//
// It burns Gold Feathers (jiggy_feathers.c), as Banjo-Kazooie does (crouch.c, func_802AA58C,
// func_80346C10): one to start it, or, with none, Banjo-Kazooie's buzzer and no Wonderwing;
// then another every 2 seconds, with SFX_3E9, until they run out, when the buzzer plays and
// the wings fold away. The gold feather counter stays up while it's on.
//
// Banjo still uses DK's movement and physics (jiggy_banjo3d.c): the Wonderwing sets DK's top
// speed (PlayerAdditionalActorData unk38, as the trot does) to Banjo-Kazooie's, a little under
// a walk, and plays its own animations with Kazooie's upper half shown. Its golden look is the
// animations' own (they scale Kazooie's wings up round Banjo) and the yellow sparkles
// Banjo-Kazooie throws off every frame (COMMON_PARTICLE_2, src/core2/code_CD6E0.c), drawn
// here as camera-facing billboards like jiggy_banjo_eggs.c's eggs.
//
// Invulnerable: DK64's own grace counter after a hit (PlayerAdditionalActorData unkD4) is kept
// set while the Wonderwing is on. Enemies leave the player alone while it is set, and the
// player's hurt handlers take no health (func_global_asm_806EB400), though some still knock
// the player back.

#include "common_structs.h"
#include "jiggy.h"

// Banjo-Kazooie animation assets (asset_e) and how long each plays, in seconds.
#define WW_ANIM_ENTER 0x22 // ASSET_22_ANIM_BSWHIRL_EXIT: forward to start, backward to end.
#define WW_ANIM_IDLE 0x23  // ASSET_23_ANIM_BSWONDERWING_IDLE
#define WW_ANIM_WALK 0x11  // ASSET_11_ANIM_BSWHIRL_WALK
#define WW_ANIM_JUMP 0x1B  // ASSET_1B_ANIM_BSWHIRL_JUMP
#define WW_ENTER_SECONDS 0.5f
#define WW_EXIT_SECONDS 0.5f
#define WW_IDLE_SECONDS 1.0f
#define WW_JUMP_SECONDS 0.8f
// The walk loop takes 0.56 s at 80 Banjo-Kazooie units a second and 0.4 s at 425, its top
// speed (baanim_setVelocityMapRanges in bsbwhirl_walk_init).
#define WW_WALK_SLOW_SPEED 80.0f
#define WW_WALK_FAST_SPEED 425.0f
#define WW_WALK_SLOW_SECONDS 0.56f
#define WW_WALK_FAST_SECONDS 0.4f
// The jump (bsbwhirl_jump_init): starts at 0.14 and holds before 0.4495 until the ground nears
// (falling, within 130 Banjo-Kazooie units: 32.5 here), then plays out to the landing.
#define WW_JUMP_START 0.14f
#define WW_JUMP_HANG_END 0.4495f
#define WW_JUMP_DROP_HEIGHT 32.5f
// Banjo-Kazooie's Wonderwing tops out at 425 units a second against 500 for a walk.
#define WW_SPEED_SCALE (425.0f / 500.0f)
// Player speed where the walk starts, as in jiggy_banjo3d.c.
#define WW_WALK_SPEED 8.0f
// Feet land at these points of the walk (func_8029AD28 in bsbwhirl_walk_update).
#define WW_STEP_1 0.47f
#define WW_STEP_2 0.97f
// A gold feather lasts this long (func_802AA58C).
#define WW_FEATHER_SECONDS 2.0f
// DK64's grace counter after a hit, in frames, kept at least this high while on.
#define WW_GUARD_FRAMES 2

// The sparkles (func_803546E8, func_8035489C): one a frame, each living 20 frames. For its
// first 5 it rides along 48 units in front of Banjo, 50 to 85 up, then flies off at 250 units
// a second out (each one 45 degrees round from the last) and 250 up, under 700 of gravity.
// Banjo-Kazooie units, scaled to DK64's as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
#define SPARKLE_SPRITE 0x713 // ASSET_713_SPRITE_SPARKLE_YELLOW
#define SPARKLE_MAX_TEXELS 1024 // A 32x32 frame.
#define SPARKLE_MAX 20
#define SPARKLE_LIFE 20
#define SPARKLE_RIDE_UNTIL 16 // Rides along while its life is at least this.
#define SPARKLE_AHEAD (48.0f * BK_TO_DK64)
#define SPARKLE_SPREAD (10.0f * BK_TO_DK64)
#define SPARKLE_UP_MIN (50.0f * BK_TO_DK64)
#define SPARKLE_UP_RANGE (35.0f * BK_TO_DK64)
#define SPARKLE_SPEED (250.0f * BK_TO_DK64)
#define SPARKLE_GRAVITY (700.0f * BK_TO_DK64)
#define SPARKLE_TURN 0x200 // 45 degrees, as a 12-bit angle.
// Size (Banjo-Kazooie units across) for each life left (D_803726D4), and the alpha, from 40 at
// the end up to full with 8 frames left (func_80354670).
static const u8 sparkle_sizes[SPARKLE_LIFE + 1] = {10, 10, 15, 20, 25, 30, 35, 40, 45, 50, 54,
                                                   58, 62, 66, 70, 74, 76, 78, 80, 40, 20};
#define SPARKLE_ALPHA_MIN 40
#define SPARKLE_ALPHA_FULL_AT 8

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

// Kazooie's upper half (head and wings), out of the backpack (src/core2/modelappendages.c).
static const u8 ww_selectors[] = {0x01, 0x09, 0x0C, 0x0F};

// Feathers (jiggy_feathers.c): kind 1 is gold.
#define FEATHER_GOLD 1
s32 jiggy_feathers_spend(s32 kind);
void jiggy_feathers_hud_hold(s32 kind);

extern u16 newly_pressed_input[];
extern f32 D_global_asm_807531FC[]; // Kong walking top speed.
extern s16 D_global_asm_807FD584;   // Current Kong, an index into the Kong tables.
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    WW_OFF,
    WW_ENTER,
    WW_ON,
    WW_EXIT,
} WonderwingState;

typedef struct {
    f32 pos[3];
    f32 offset[3]; // From the player, while riding along.
    f32 velocity[3];
    s8 life;       // Frames left; -1 when free.
} Sparkle;

static WonderwingState ww_state;
static f32 ww_feather_seconds; // Since the last feather (D_8037D3B0).
static s32 ww_music;           // The gold feather music (jiggy_bk_jingle_play's handle), or 0.
static u8 ww_step_alt;

static u16 sparkle_texels[SPARKLE_MAX_TEXELS] __attribute__((aligned(8)));
static s16 sparkle_width;
static s16 sparkle_height;
static Vtx sparkle_vtx[4] __attribute__((aligned(8)));
static Mtx sparkle_mtx[2][SPARKLE_MAX];
static s32 sparkles_loaded; // 1 ready, -1 unavailable.
static Sparkle sparkles[SPARKLE_MAX];
static s16 sparkle_angle;
static u32 sparkle_last_timer;
static u32 sparkle_random_state = 0x57494E47;

static void ww_play(s32 *animation, f32 *progress, s32 id, f32 start) {
    if (*animation != id) {
        *animation = id;
        *progress = start;
    }
}

static f32 ww_walk_top_speed(void) {
    return D_global_asm_807531FC[D_global_asm_807FD584];
}

// Kazooie's "auw" as the wings come out (basfx_80299BD4) and Banjo-Kazooie's gold feather
// music (COMUSIC_25_USING_GOLD_FEATHERS, coMusicPlayer_playMusic(0x25, 0x6D60)). The caller has
// spent the first gold feather.
static void wonderwing_start(Actor *player) {
    ww_state = WW_ENTER;
    ww_feather_seconds = 0.0f;
    jiggy_bk_sfx_play(JIGGY_BK_SFX_KAZOOIE_AUW, 22000.0f / 32767.0f, 1.117f);
    jiggy_bk_sound_stop(ww_music);
    ww_music = jiggy_bk_jingle_play(JIGGY_BK_JINGLE_GOLD_FEATHERS, 28000.0f / 32767.0f);
}

static void wonderwing_stop(Actor *player) {
    PlayerAdditionalActorData *paad = player->PaaD;

    ww_state = WW_OFF;
    jiggy_bk_sound_stop(ww_music);
    ww_music = 0;
    // DK's walk sets its own top speed again on its next state; put it back now if walking.
    if (paad != NULL && (player->control_state == STATE_IDLE || player->control_state == STATE_WALK)) {
        paad->unk38 = ww_walk_top_speed();
    }
}

// A feather's time is up: spend the next, with SFX_3E9_UNKNOWN (FUNC_8030E624: pitch 0.8,
// 28000), or, out of them, the exit (jiggy_feathers_spend plays Banjo-Kazooie's buzzer,
// func_80346C10's COMUSIC_2C_BUZZER at 22000).
static void wonderwing_feather_due(void) {
    if (jiggy_feathers_spend(FEATHER_GOLD)) {
        jiggy_bk_sfx_play(JIGGY_BK_SFX_FEATHER_SPENT, 28000.0f / 32767.0f, 0.8f);
    } else {
        ww_state = WW_EXIT;
    }
}

// Banjo-Kazooie's Wonderwing speed on the ground, and no faster in the air.
static void ww_set_speed(Actor *player) {
    PlayerAdditionalActorData *paad = player->PaaD;
    f32 top_speed = ww_walk_top_speed() * WW_SPEED_SCALE;

    if (paad == NULL) {
        return;
    }
    if (player->control_state == STATE_IDLE || player->control_state == STATE_WALK ||
        ((player->control_state == STATE_JUMP || player->control_state == STATE_FALL) && paad->unk38 > top_speed)) {
        paad->unk38 = top_speed;
    }
}

// Nothing hurts the player while it's on (see the top of the file).
static void ww_guard(Actor *player) {
    PlayerAdditionalActorData *paad = player->PaaD;

    if (paad != NULL && paad->unkD4 < WW_GUARD_FRAMES) {
        paad->unkD4 = WW_GUARD_FRAMES;
    }
}

static void ww_jump(Actor *player, f32 seconds, s32 *animation, f32 *progress) {
    f32 end = WW_JUMP_HANG_END;

    // Walked off a ledge: straight to the hang.
    ww_play(animation, progress, WW_ANIM_JUMP, player->control_state == STATE_FALL ? WW_JUMP_HANG_END : WW_JUMP_START);
    if (player->y_velocity < 0.0f && player->y_position - player->floor < WW_JUMP_DROP_HEIGHT) {
        end = 0.99f;
    }
    if (*progress < end) {
        *progress += seconds / WW_JUMP_SECONDS;
        if (*progress > end) {
            *progress = end;
        }
    }
}

// Each Wonderwing step: Banjo-Kazooie's ordinary-ground footfall (SFX_8_BANJO_LANDING_04 at
// 9000, its pitch alternating between 1.0 and 1.2: func_802F4604 for this move's group).
static void ww_step_sounds(f32 before, f32 after) {
    if ((before < WW_STEP_1 && after >= WW_STEP_1) || (before < WW_STEP_2 && after >= WW_STEP_2)) {
        ww_step_alt ^= 1;
        jiggy_bk_sfx_play(JIGGY_BK_SFX_FOOTSTEP, 9000.0f / 32767.0f, ww_step_alt ? 1.0f : 1.2f);
    }
}

// Called from banjo_animate (jiggy_banjo3d.c) before Banjo's own moves. Starts and stops the
// Wonderwing, keeps the player safe and at its speed and, while it's on, picks Banjo's
// animation. Returns 1 when it did.
s32 jiggy_banjo_wonderwing_animate(Actor *player, s32 is_player, s32 airborne, f32 seconds, s32 *animation,
                                   f32 *progress) {
    u16 pressed = newly_pressed_input[0];
    s32 state = player->control_state;
    f32 speed = player->unkB8;
    f32 bk_speed;
    f32 loop;
    f32 before;

    if (!is_player) {
        if (ww_state != WW_OFF) {
            wonderwing_stop(player);
        }
        return FALSE;
    }
    if (ww_state == WW_OFF) {
        if (!(pressed & R_CBUTTONS) || airborne || (state != STATE_CROUCH_ENTER && state != STATE_CROUCH)) {
            return FALSE;
        }
        // A gold feather to start, or the buzzer and no Wonderwing.
        if (!jiggy_feathers_spend(FEATHER_GOLD)) {
            return FALSE;
        }
        wonderwing_start(player);
    } else if ((pressed & Z_TRIG) || state == STATE_ATTACK || state == STATE_RUNNING_ATTACK ||
               state == STATE_BACKFLIP) {
        // Z crouches (or, in the air, Beak Busts) and DK's attacks show Banjo's own moves.
        wonderwing_stop(player);
        return FALSE;
    } else if ((pressed & R_CBUTTONS) && ww_state != WW_EXIT) {
        ww_state = WW_EXIT;
    }
    if (ww_state == WW_EXIT && (airborne || speed >= WW_WALK_SPEED)) {
        // The wings only fold away standing still.
        wonderwing_stop(player);
        return FALSE;
    }
    ww_guard(player);
    ww_set_speed(player);
    jiggy_feathers_hud_hold(FEATHER_GOLD);

    switch (ww_state) {
        case WW_ENTER:
            ww_play(animation, progress, WW_ANIM_ENTER, 0.0f);
            *progress += seconds / WW_ENTER_SECONDS;
            if (*progress < 0.99f) {
                return TRUE;
            }
            ww_state = WW_ON;
            break;
        case WW_EXIT:
            // The enter animation, backward (bsbwhirl_exit_init).
            ww_play(animation, progress, WW_ANIM_ENTER, 0.99f);
            *progress -= seconds / WW_EXIT_SECONDS;
            if (*progress <= 0.0f) {
                wonderwing_stop(player);
                return FALSE;
            }
            return TRUE;
        default:
            break;
    }

    ww_feather_seconds += seconds;
    if (ww_feather_seconds > WW_FEATHER_SECONDS) {
        ww_feather_seconds = 0.0f;
        wonderwing_feather_due();
    }

    if (airborne) {
        ww_jump(player, seconds, animation, progress);
        return TRUE;
    }
    if (*animation == WW_ANIM_JUMP && *progress < 0.99f && speed < WW_WALK_SPEED) {
        // Landed standing still: finish the landing.
        *progress += seconds / WW_JUMP_SECONDS;
        return TRUE;
    }
    if (speed < WW_WALK_SPEED) {
        ww_play(animation, progress, WW_ANIM_IDLE, 0.0f);
        *progress += seconds / WW_IDLE_SECONDS;
    } else {
        ww_play(animation, progress, WW_ANIM_WALK, 0.0f);
        // The player's speed as Banjo-Kazooie's, for the walk's pace.
        bk_speed = speed / (ww_walk_top_speed() * WW_SPEED_SCALE) * WW_WALK_FAST_SPEED;
        if (bk_speed <= WW_WALK_SLOW_SPEED) {
            loop = WW_WALK_SLOW_SECONDS;
        } else if (bk_speed >= WW_WALK_FAST_SPEED) {
            loop = WW_WALK_FAST_SECONDS;
        } else {
            loop = WW_WALK_SLOW_SECONDS + (WW_WALK_FAST_SECONDS - WW_WALK_SLOW_SECONDS) *
                                              (bk_speed - WW_WALK_SLOW_SPEED) / (WW_WALK_FAST_SPEED - WW_WALK_SLOW_SPEED);
        }
        before = *progress;
        *progress += seconds / loop;
        ww_step_sounds(before, *progress);
    }
    while (*progress >= 1.0f) {
        *progress -= 1.0f;
    }
    return TRUE;
}

// Called from banjo_set_selectors (jiggy_banjo3d.c): Kazooie's upper half while it's on.
void jiggy_banjo_wonderwing_selectors(s32 *selectors) {
    s32 i;

    if (ww_state == WW_OFF) {
        return;
    }
    for (i = 0; i < (s32)sizeof(ww_selectors); i++) {
        selectors[ww_selectors[i]] = 1;
    }
}

// Whether C-right belongs to the Wonderwing (it stops it), kept from DK64's own C-right
// (func_global_asm_806DFF34 in jiggy_banjo_eggs.c).
s32 jiggy_banjo_wonderwing_owns_c_right(void) {
    return ww_state != WW_OFF && cc_number_of_players == 1 && cc_player_index == 0;
}

// 0 to 1. Its own generator, so the sparkles don't shift DK64's random numbers.
static f32 sparkle_random(void) {
    sparkle_random_state = sparkle_random_state * 1664525 + 1013904223;
    return (sparkle_random_state >> 8) * (1.0f / 16777216.0f);
}

static void sparkle_set_vtx(Vtx *v, s32 x, s32 y, s32 s, s32 t) {
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

static void sparkles_load(void) {
    s32 size;
    s32 width, height;
    s32 i;

    sparkles_loaded = -1;
    for (i = 0; i < SPARKLE_MAX; i++) {
        sparkles[i].life = -1;
    }
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_sprite_frame_rgba16(SPARKLE_SPRITE, 0, sparkle_texels, sizeof(sparkle_texels));
    width = (size >> 16) & 0xFFFF;
    height = size & 0xFFFF;
    if (size <= 0 || width == 0 || height == 0 || width * height > SPARKLE_MAX_TEXELS) {
        return;
    }
    sparkle_width = width;
    sparkle_height = height;
    // In texels, centered on the sparkle, before its matrix scales it.
    sparkle_set_vtx(&sparkle_vtx[0], -width / 2, height / 2, 0, 0);
    sparkle_set_vtx(&sparkle_vtx[1], width / 2, height / 2, width, 0);
    sparkle_set_vtx(&sparkle_vtx[2], width / 2, -height / 2, width, height);
    sparkle_set_vtx(&sparkle_vtx[3], -width / 2, -height / 2, 0, height);
    sparkles_loaded = 1;
}

static void sparkle_spawn(Actor *player) {
    Sparkle *sparkle = NULL;
    s16 yaw = player->y_rotation & 0xFFF;
    s32 i;

    for (i = 0; i < SPARKLE_MAX; i++) {
        if (sparkles[i].life < 0) {
            sparkle = &sparkles[i];
            break;
        }
    }
    if (sparkle == NULL) {
        return;
    }
    // Rotations are 12-bit angles, with x along sin and z along cos, as DK64 moves its actors.
    sparkle->offset[0] = (sparkle_random() * 2.0f - 1.0f) * SPARKLE_SPREAD + func_global_asm_80612794(yaw) * SPARKLE_AHEAD;
    sparkle->offset[1] = SPARKLE_UP_MIN + sparkle_random() * SPARKLE_UP_RANGE;
    sparkle->offset[2] = (sparkle_random() * 2.0f - 1.0f) * SPARKLE_SPREAD + func_global_asm_80612790(yaw) * SPARKLE_AHEAD;
    sparkle->velocity[0] = func_global_asm_80612794(sparkle_angle) * SPARKLE_SPEED;
    sparkle->velocity[1] = SPARKLE_SPEED;
    sparkle->velocity[2] = func_global_asm_80612790(sparkle_angle) * SPARKLE_SPEED;
    sparkle_angle = (sparkle_angle + SPARKLE_TURN) & 0xFFF;
    sparkle->life = SPARKLE_LIFE;
}

// A frame of every sparkle: riding along with the player, then flying off and falling.
static void sparkles_update(Actor *player, s32 spawn) {
    f32 dt = 1.0f / 30.0f;
    s32 i;

    for (i = 0; i < SPARKLE_MAX; i++) {
        Sparkle *sparkle = &sparkles[i];

        if (sparkle->life < 0) {
            continue;
        }
        sparkle->life--;
        if (sparkle->life < 0) {
            continue;
        }
        if (sparkle->life >= SPARKLE_RIDE_UNTIL) {
            sparkle->pos[0] = player->x_position + sparkle->offset[0];
            sparkle->pos[1] = player->y_position + sparkle->offset[1];
            sparkle->pos[2] = player->z_position + sparkle->offset[2];
        } else {
            sparkle->velocity[1] -= SPARKLE_GRAVITY * dt;
            sparkle->pos[0] += sparkle->velocity[0] * dt;
            sparkle->pos[1] += sparkle->velocity[1] * dt;
            sparkle->pos[2] += sparkle->velocity[2] * dt;
        }
    }
    if (spawn) {
        sparkle_spawn(player);
        // Placed now, so it's drawn this frame.
        for (i = 0; i < SPARKLE_MAX; i++) {
            if (sparkles[i].life == SPARKLE_LIFE) {
                sparkles[i].pos[0] = player->x_position + sparkles[i].offset[0];
                sparkles[i].pos[1] = player->y_position + sparkles[i].offset[1];
                sparkles[i].pos[2] = player->z_position + sparkles[i].offset[2];
            }
        }
    }
}

// A matrix that stands the sparkle at pos facing the camera, `size` units across (as
// jiggy_banjo_eggs.c stands its eggs).
static void sparkle_billboard(Mtx *m, f32 pos[3], f32 size) {
    CharacterChange *cc = &character_change_array[cc_player_index];
    f32 mf[4][4];
    f32 f[3], r[3], u[3];
    f32 length;
    f32 scale = size / sparkle_width;

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

// Called once a frame from jiggy_banjo_draw (jiggy_banjo3d.c) while Banjo is on screen, after
// Banjo is drawn: moves the sparkles, throws off a new one while the Wonderwing is on (not
// while the wings open or fold), and draws them.
Gfx *jiggy_banjo_wonderwing_draw(Gfx *dl, Actor *player) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames = object_timer - sparkle_last_timer;
    s32 drawn = FALSE;
    u32 f;
    s32 i;

    sparkle_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    if (sparkles_loaded == 0) {
        if (ww_state == WW_OFF) {
            return dl;
        }
        sparkles_load();
    }
    if (sparkles_loaded < 0) {
        return dl;
    }
    for (f = 0; f < frames; f++) {
        sparkles_update(player, ww_state == WW_ON);
    }

    for (i = 0; i < SPARKLE_MAX; i++) {
        Sparkle *sparkle = &sparkles[i];
        s32 alpha;

        if (sparkle->life < 0) {
            continue;
        }
        if (!drawn) {
            drawn = TRUE;
            gDPPipeSync(dl++);
            gSPClearGeometryMode(dl++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN);
            gSPSetGeometryMode(dl++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
            gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
            gDPSetCycleType(dl++, G_CYC_1CYCLE);
            gDPSetTexturePersp(dl++, G_TP_PERSP);
            gDPSetTextureLUT(dl++, G_TT_NONE);
            gDPSetTextureFilter(dl++, G_TF_BILERP);
            gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPSetRenderMode(dl++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
            gDPLoadTextureTile(dl++, sparkle_texels, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                sparkle_width, sparkle_height,
                0, 0, sparkle_width - 1, sparkle_height - 1,
                0,
                G_TX_CLAMP, G_TX_CLAMP,
                G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        }
        alpha = 0xFF;
        if (sparkle->life < SPARKLE_ALPHA_FULL_AT) {
            alpha = SPARKLE_ALPHA_MIN + (0xFF - SPARKLE_ALPHA_MIN) * sparkle->life / SPARKLE_ALPHA_FULL_AT;
        }
        gDPPipeSync(dl++);
        gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, alpha);
        sparkle_billboard(&sparkle_mtx[buffer][i], sparkle->pos, sparkle_sizes[(s32)sparkle->life] * BK_TO_DK64);
        gSPMatrix(dl++, &sparkle_mtx[buffer][i], G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(dl++, sparkle_vtx, 4, 0);
        gSP2Triangles(dl++, 0, 3, 2, 0, 0, 2, 1, 0);
        gSPPopMatrix(dl++, G_MTX_MODELVIEW);
    }
    if (drawn) {
        gDPPipeSync(dl++);
    }
    return dl;
}
