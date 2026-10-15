// Slappas: Gobi's Valley's hands in the sand, read from the player's Banjo-Kazooie ROM, as
// enemies in Angry Aztec, where the two worlds have collided.
//
// Like chSlappa (src/GV/ch/slappa.c), each Slappa (ASSET_376_MODEL_SLAPPA) waits under the sand
// at its spot; when the player comes near (chSlappa's 800 units, scaled) it rises (its rise, 0.8
// seconds) with a rumble (SFX_3EC, as its sfx source plays it while it moves), then slides after
// them. Close in (175 units, scaled) it pauses (20 frames), then slaps the ground in front of it
// (SFX_3_DULL_CANNON_SHOT at 0.79 through the slap), lies there (75 frames) and gets up (1.2
// seconds) to chase again. The slap catching the player, or running into them, hurts them
// through DK64's own enemy hit, as the Grublins do (jiggy_grublins.c). If the player gets away
// (1100 units, scaled) or it strays too far from its spot, it sinks back (1.4 seconds) and
// waits a couple of seconds before it can rise again.
//
// The player's attacks (as the Grublins are hit) or shots defeat it while it's out of the sand,
// with Banjo-Kazooie's cry (SFX_D7_GRABBA_DEATH, as chSlappa's death plays it): it sinks away
// and is gone until the map is loaded again. Nothing is saved and it drops nothing.
//
// Gobi's Valley's models move their vertices themselves (jiggy_aztec.h), so each Slappa is drawn
// at rest and moved as a whole: rising, leaning into its slap, lying flat. Drawn right after the
// player's own model (func_global_asm_80630DCC in patches_draw.c). Without a Banjo-Kazooie ROM
// nothing is drawn and nothing happens.

#include "jiggy_aztec.h"

#define SLAPPA_MODEL 0x376 // ASSET_376_MODEL_SLAPPA
// The Slappa converts to about 7 KiB with 25 skeleton entries, posed in about 20 commands.
#define SLAPPA_BLOCK_MAX 0x2000
#define SLAPPA_MTX_MAX 26
#define SLAPPA_GFX_MAX 32
#define SLAPPA_MAX 3

#define SLAPPA_SFX_SLAP 0x03 // SFX_3_DULL_CANNON_SHOT: the slap landing, 32000/32767, 1.0
#define SLAPPA_SFX_DIE 0xD7  // SFX_D7_GRABBA_DEATH: defeated, 32000/32767, 1.0

// Where they wait: one in the first sand area (south, toward the tunnel), one in the tunnel to
// the main area, one in the main area's south sand. Flat sand at 110 to 120, clear of Gobi,
// Trunker, Grabba's room and the Ancient Ones.
static const f32 slappa_spots[SLAPPA_MAX][3] = {
    {2450.0f, 120.0f, 1260.0f},
    {3000.0f, 120.0f, 1800.0f},
    {3480.0f, 120.0f, 4330.0f},
};

// Sizes, distances (DK64 units) and timings (seconds), from chSlappa's, scaled.
#define SLAPPA_SCALE 0.8f
#define SLAPPA_SINK 70.0f
#define SLAPPA_SEE_RADIUS 200.0f
#define SLAPPA_SEE_HEIGHT 60.0f
#define SLAPPA_LOSE_RADIUS 275.0f
#define SLAPPA_LEASH 300.0f
#define SLAPPA_ATTACK_RADIUS 44.0f
#define SLAPPA_CONTACT_RADIUS 20.0f
#define SLAPPA_SLAP_FORWARD 26.0f
#define SLAPPA_SLAP_RADIUS 26.0f
#define SLAPPA_BODY_RADIUS 18.0f
#define SLAPPA_BODY_HEIGHT 60.0f
#define SLAPPA_SPEED 70.0f
#define SLAPPA_TURN_RATE 0.12f
#define SLAPPA_RISE_SECONDS 0.8f
#define SLAPPA_WINDUP_SECONDS (20.0f / 30.0f)
#define SLAPPA_SLAP_SECONDS 0.8f
#define SLAPPA_SLAP_AT 0.79f
#define SLAPPA_LIE_SECONDS (75.0f / 30.0f)
#define SLAPPA_GET_UP_SECONDS 1.2f
#define SLAPPA_LOWER_SECONDS 1.4f
#define SLAPPA_REST_SECONDS 2.0f
#define SLAPPA_DIE_SECONDS 0.8f
#define SLAPPA_BACK_OFF_SECONDS 1.5f // After running into the player.
// Short of lying flat, so its arm (under the sand, pivoting at the surface) stays under it.
#define SLAPPA_FLAT_PITCH 70.0f
#define STEP_UP 20.0f
#define STEP_DOWN 30.0f
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
s32 func_global_asm_806EB400(Actor *player); // Whether the player can be hurt now.

typedef enum {
    SLAPPAS_UNLOADED,
    SLAPPAS_READY,
    SLAPPAS_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} SlappasLoadState;

typedef enum {
    SLAPPA_HIDDEN,
    SLAPPA_RISING,
    SLAPPA_CHASING,
    SLAPPA_WINDUP,
    SLAPPA_SLAPPING,
    SLAPPA_LYING,
    SLAPPA_GETTING_UP,
    SLAPPA_LOWERING,
    SLAPPA_DYING,
    SLAPPA_GONE,
} SlappaAction;

typedef struct {
    const f32 *home;
    f32 pos[3];
    f32 yaw;
    f32 depth; // Under the sand: 0 up, SLAPPA_SINK hidden.
    f32 time;  // Seconds into the action.
    f32 rest;  // Seconds before it can rise again.
    f32 back_off;
    SlappaAction action;
} Slappa;

static u8 slappa_block[SLAPPA_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx slappa_gfx[SLAPPA_MAX][2][SLAPPA_GFX_MAX];
static Mtx slappa_mtx[SLAPPA_MAX][2][SLAPPA_MTX_MAX];
static JiggyBkPose slappa_pose;
static SlappasLoadState slappas_state = SLAPPAS_UNLOADED;

static s32 slappas_last_map = -1;
static u32 slappas_last_timer;
static s32 slappas_placed;
static Slappa slappas[SLAPPA_MAX];
static s32 slappas_rumble;

static void slappas_load(void) {
    slappas_state = SLAPPAS_UNAVAILABLE;
    if (!jiggy_bk_available() || !aztec_model_load(SLAPPA_MODEL, slappa_block, sizeof(slappa_block))) {
        return;
    }
    aztec_pose_init(&slappa_pose);
    slappas_state = SLAPPAS_READY;
}

static void slappas_place(void) {
    s32 i;

    for (i = 0; i < SLAPPA_MAX; i++) {
        Slappa *s = &slappas[i];

        s->home = slappa_spots[i];
        s->pos[0] = s->home[0];
        s->pos[1] = aztec_floor(s->home[0], s->home[1], s->home[2], FLOOR_PROBE, FLOOR_MAX_STEP);
        s->pos[2] = s->home[2];
        s->yaw = 0.0f;
        s->depth = SLAPPA_SINK;
        s->time = 0.0f;
        s->rest = 0.0f;
        s->back_off = 0.0f;
        s->action = SLAPPA_HIDDEN;
    }
    slappas_placed = TRUE;
}

static void slappa_start(Slappa *s, SlappaAction action) {
    s->action = action;
    s->time = 0.0f;
}

static s32 slappa_sees(Slappa *s, Actor *player, f32 radius) {
    f32 dy = player->y_position - s->pos[1];

    return aztec_distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position) <= radius &&
           dy <= SLAPPA_SEE_HEIGHT && dy >= -SLAPPA_SEE_HEIGHT;
}

static void slappas_rumble_start(void) {
    if (slappas_rumble <= 0) {
        slappas_rumble = jiggy_bk_sfx_play(AZTEC_SFX_SAND_RUMBLE, 0.4f, 1.0f);
    }
}

static void slappas_rumble_stop(void) {
    if (slappas_rumble > 0) {
        jiggy_bk_sound_stop(slappas_rumble);
        slappas_rumble = 0;
    }
}

// Slides toward (x, z) over the sand, as far as `step`; a wall of a step or a drop stops it.
static void slappa_move_to(Slappa *s, f32 x, f32 z, f32 step) {
    f32 distance = aztec_distance_xz(x, z, s->pos[0], s->pos[2]);
    f32 nx, nz;
    f32 probe;
    f32 floor;

    if (distance < 1.0f) {
        return;
    }
    if (step > distance) {
        step = distance;
    }
    nx = s->pos[0] + (x - s->pos[0]) * step / distance;
    nz = s->pos[2] + (z - s->pos[2]) * step / distance;
    if (aztec_distance_xz(nx, nz, s->home[0], s->home[2]) > SLAPPA_LEASH) {
        return;
    }
    probe = s->pos[1] + STEP_UP;
    floor = probe;
    func_global_asm_80667110(nx, nz, &floor);
    if (floor == probe || floor > s->pos[1] + STEP_UP || floor < s->pos[1] - STEP_DOWN) {
        return;
    }
    s->pos[0] = nx;
    s->pos[1] = floor;
    s->pos[2] = nz;
}

// Out of the sand far enough to be hit.
static s32 slappa_hit(Slappa *s, Actor *player) {
    f32 body[3];

    if (s->depth > SLAPPA_SINK * 0.5f) {
        return FALSE;
    }
    body[0] = s->pos[0];
    body[1] = s->pos[1];
    body[2] = s->pos[2];
    return aztec_player_attacks(player, body, SLAPPA_BODY_RADIUS) ||
           aztec_player_shot(player, body, SLAPPA_BODY_RADIUS, SLAPPA_BODY_HEIGHT);
}

// A frame of one Slappa (chSlappa_update), `frames` game frames on.
static void slappa_update(Slappa *s, Actor *player, u32 frames) {
    f32 seconds = frames / AZTEC_GAME_FPS;
    f32 before = s->time;
    f32 distance = aztec_distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - s->pos[1];

    if (frames == 0) {
        return;
    }
    s->time += seconds;
    if (s->back_off > 0.0f) {
        s->back_off -= seconds;
    }
    if (s->action != SLAPPA_HIDDEN && s->action != SLAPPA_DYING && s->action != SLAPPA_GONE && slappa_hit(s, player)) {
        aztec_sfx_at(player, s->pos[0], s->pos[2], SLAPPA_SFX_DIE, 1.0f, 1.0f);
        slappa_start(s, SLAPPA_DYING);
        return;
    }
    // Running into the player while it's up hurts them (its collision in chSlappa).
    if (s->depth < SLAPPA_SINK * 0.3f && s->back_off <= 0.0f && s->action != SLAPPA_DYING && s->action != SLAPPA_GONE &&
        distance <= SLAPPA_CONTACT_RADIUS && dy < 40.0f && dy > -20.0f && func_global_asm_806EB400(player)) {
        aztec_hurt_player(player, s->pos[0], s->pos[2]);
        s->back_off = SLAPPA_BACK_OFF_SECONDS;
    }
    switch (s->action) {
        case SLAPPA_HIDDEN:
            // Back to its spot under the sand.
            s->pos[0] = s->home[0];
            s->pos[2] = s->home[2];
            s->pos[1] = aztec_floor(s->home[0], s->home[1], s->home[2], FLOOR_PROBE, FLOOR_MAX_STEP);
            if (s->rest > 0.0f) {
                s->rest -= seconds;
            } else if (slappa_sees(s, player, SLAPPA_SEE_RADIUS) && player->y_position - player->floor <= 40.0f) {
                s->yaw = func_global_asm_80665DE0(player->x_position, player->z_position, s->pos[0], s->pos[2]) & 0xFFF;
                slappas_rumble_start();
                slappa_start(s, SLAPPA_RISING);
            }
            break;
        case SLAPPA_RISING:
            aztec_turn_toward(&s->yaw, player->x_position, player->z_position, s->pos[0], s->pos[2], SLAPPA_TURN_RATE, frames);
            s->depth = SLAPPA_SINK * (1.0f - s->time / SLAPPA_RISE_SECONDS);
            if (s->time >= SLAPPA_RISE_SECONDS) {
                s->depth = 0.0f;
                slappas_rumble_stop();
                slappa_start(s, SLAPPA_CHASING);
            }
            break;
        case SLAPPA_CHASING:
            if (!slappa_sees(s, player, SLAPPA_LOSE_RADIUS) ||
                aztec_distance_xz(s->pos[0], s->pos[2], s->home[0], s->home[2]) >= SLAPPA_LEASH - 1.0f) {
                slappas_rumble_start();
                slappa_start(s, SLAPPA_LOWERING);
                break;
            }
            if (distance <= SLAPPA_ATTACK_RADIUS && s->back_off <= 0.0f) {
                slappa_start(s, SLAPPA_WINDUP);
                break;
            }
            aztec_turn_toward(&s->yaw, player->x_position, player->z_position, s->pos[0], s->pos[2], SLAPPA_TURN_RATE, frames);
            if (distance > SLAPPA_CONTACT_RADIUS * 0.8f) {
                slappa_move_to(s, player->x_position, player->z_position, SLAPPA_SPEED * seconds);
            }
            break;
        case SLAPPA_WINDUP:
            aztec_turn_toward(&s->yaw, player->x_position, player->z_position, s->pos[0], s->pos[2], SLAPPA_TURN_RATE, frames);
            if (s->time >= SLAPPA_WINDUP_SECONDS) {
                slappa_start(s, SLAPPA_SLAPPING);
            }
            break;
        case SLAPPA_SLAPPING:
            if (before < SLAPPA_SLAP_AT * SLAPPA_SLAP_SECONDS && s->time >= SLAPPA_SLAP_AT * SLAPPA_SLAP_SECONDS) {
                f32 x = s->pos[0] + func_global_asm_80612794((s16)s->yaw) * SLAPPA_SLAP_FORWARD;
                f32 z = s->pos[2] + func_global_asm_80612790((s16)s->yaw) * SLAPPA_SLAP_FORWARD;

                aztec_sfx_at(player, s->pos[0], s->pos[2], SLAPPA_SFX_SLAP, 1.0f, 1.0f);
                if (aztec_distance_xz(x, z, player->x_position, player->z_position) <= SLAPPA_SLAP_RADIUS && dy < 40.0f &&
                    dy > -20.0f && func_global_asm_806EB400(player)) {
                    aztec_hurt_player(player, s->pos[0], s->pos[2]);
                    s->back_off = SLAPPA_BACK_OFF_SECONDS;
                }
            }
            if (s->time >= SLAPPA_SLAP_SECONDS) {
                slappa_start(s, SLAPPA_LYING);
            }
            break;
        case SLAPPA_LYING:
            if (s->time >= SLAPPA_LIE_SECONDS) {
                slappa_start(s, SLAPPA_GETTING_UP);
            }
            break;
        case SLAPPA_GETTING_UP:
            if (s->time >= SLAPPA_GET_UP_SECONDS) {
                slappa_start(s, SLAPPA_CHASING);
            }
            break;
        case SLAPPA_LOWERING:
            s->depth = SLAPPA_SINK * (s->time / SLAPPA_LOWER_SECONDS);
            if (s->time >= SLAPPA_LOWER_SECONDS) {
                s->depth = SLAPPA_SINK;
                s->rest = SLAPPA_REST_SECONDS;
                slappas_rumble_stop();
                aztec_sfx_at(player, s->pos[0], s->pos[2], AZTEC_SFX_SAND_POP, 1.0f, 1.0f);
                slappa_start(s, SLAPPA_HIDDEN);
            }
            break;
        case SLAPPA_DYING:
            s->depth = SLAPPA_SINK * (s->time / SLAPPA_DIE_SECONDS);
            if (s->time >= SLAPPA_DIE_SECONDS) {
                s->depth = SLAPPA_SINK;
                slappa_start(s, SLAPPA_GONE);
            }
            break;
        default:
            break;
    }
}

// How far it leans forward (degrees) for what it's doing.
static f32 slappa_pitch(Slappa *s) {
    switch (s->action) {
        case SLAPPA_CHASING:
            return 8.0f + 4.0f * func_global_asm_80612794((s16)(s->time * 4096.0f));
        case SLAPPA_WINDUP:
            return -20.0f * (s->time / SLAPPA_WINDUP_SECONDS);
        case SLAPPA_SLAPPING: {
            f32 at = s->time / (SLAPPA_SLAP_AT * SLAPPA_SLAP_SECONDS);

            return at >= 1.0f ? SLAPPA_FLAT_PITCH : -20.0f + (SLAPPA_FLAT_PITCH + 20.0f) * at * at;
        }
        case SLAPPA_LYING:
            return SLAPPA_FLAT_PITCH;
        case SLAPPA_GETTING_UP:
            return SLAPPA_FLAT_PITCH * (1.0f - s->time / SLAPPA_GET_UP_SECONDS);
        default:
            return 0.0f;
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// the Slappas once a frame, with the player.
Gfx *jiggy_slappas_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    s32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): every Slappa is back.
    if ((s32)current_map != slappas_last_map || object_timer < slappas_last_timer) {
        slappas_rumble_stop();
        slappas_last_map = current_map;
        slappas_placed = FALSE;
    }
    if (current_map != MAP_AZTEC) {
        slappas_last_timer = object_timer;
        return dl;
    }
    if (slappas_state == SLAPPAS_UNLOADED) {
        slappas_load();
    }
    if (slappas_state != SLAPPAS_READY) {
        return dl;
    }
    if (!slappas_placed) {
        slappas_last_timer = object_timer;
        if (actor->y_position - actor->floor > AZTEC_AIRBORNE_HEIGHT) {
            return dl;
        }
        slappas_place();
    }
    frames = aztec_frames(&slappas_last_timer);
    // They wait while a cutscene or a talk holds the player.
    if (aztec_player_locked()) {
        frames = 0;
        slappas_rumble_stop();
    }
    for (i = 0; i < SLAPPA_MAX; i++) {
        Slappa *s = &slappas[i];
        f32 pos[3];

        if (s->action == SLAPPA_GONE) {
            continue;
        }
        slappa_update(s, actor, frames);
        if (s->action == SLAPPA_HIDDEN || s->action == SLAPPA_GONE || s->depth >= SLAPPA_SINK) {
            continue;
        }
        pos[0] = s->pos[0];
        pos[1] = s->pos[1] - s->depth;
        pos[2] = s->pos[2];
        dl = aztec_model_draw(dl, &slappa_pose, SLAPPA_MODEL, slappa_block, pos, s->yaw, SLAPPA_SCALE, 1.0f, slappa_pitch(s),
                              NULL, slappa_gfx[i][buffer], SLAPPA_GFX_MAX, slappa_mtx[i][buffer], SLAPPA_MTX_MAX);
    }
    return dl;
}
