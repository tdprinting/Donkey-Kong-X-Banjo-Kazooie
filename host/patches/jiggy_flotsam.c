// Flotsam: Rusty Bucket Bay's angry life buoys, read from the player's Banjo-Kazooie ROM, as
// enemies in Frantic Factory's east corridor, where the two worlds have collided.
//
// Like chFlotsam (src/core2/ch/flotsam.c), each Flotsam (ASSET_401_MODEL_FLOTSAM) bounces along
// (ASSET_132_ANIM_FLOTSAM_MOVE, a boing, SFX_3F2_BOING, at each bounce), here up and down a short
// path, resting now and then; when it sees the player it bounces after them. It runs as
// jiggy_grublins.c's Flotsams do: running into the player hurts them once, through DK64's own
// path for an enemy's hit (setAction(0xE)), and it backs off after; the player's attacks (punches,
// rolls, aerial attacks, the Simian Slam, and Banjo's Rat-a-tat Rap and Beak Buster) burst it,
// with Banjo-Kazooie's death animation and sounds (ASSET_189_ANIM_FLOTSAM_DIE,
// SFX_1D_HITTING_AN_ENEMY_1 and SFX_103_FLOTSAM_DEATH). A burst Flotsam is gone until the map is
// loaded again; nothing is saved and it drops nothing.
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c). Without
// a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "jiggy.h"

#define FLOTSAM_MODEL 0x401 // ASSET_401_MODEL_FLOTSAM
// A Flotsam converts to about 12 KiB, with 4 bones and about 7 pose commands.
#define FLOTSAM_BLOCK_MAX 0x3400
#define FLOTSAM_GFX_MAX 32
#define FLOTSAM_MTX_MAX 8

// Banjo-Kazooie animations (chflotsam) and how long each plays, in seconds: a bounce is one
// loop of its move (0.7 s); chasing, it bounces faster; it sees the player with one bounce on the
// spot; it bursts over 4 seconds (cut short here: it's gone once it's flat).
#define FLOTSAM_ANIM_WALK 0x132  // ASSET_132_ANIM_FLOTSAM_MOVE
#define FLOTSAM_ANIM_CHASE 0x132
#define FLOTSAM_ANIM_ALERT 0x132
#define FLOTSAM_ANIM_DIE 0x189   // ASSET_189_ANIM_FLOTSAM_DIE
#define FLOTSAM_WALK_SECONDS 0.7f
#define FLOTSAM_IDLE_SECONDS 2.8f // The bounce, slowed right down, while it rests.
#define FLOTSAM_CHASE_SECONDS 0.5f
#define FLOTSAM_ALERT_SECONDS 0.7f
#define FLOTSAM_DIE_SECONDS 2.0f

// Banjo-Kazooie sounds (sfx_e) and how chflotsam plays them.
#define FLOTSAM_SFX_NYAHAHA 0x3F2  // SFX_3F2_BOING: each bounce (and seeing the player), 0x7530/32767,
                                   // 1.0-1.2
#define FLOTSAM_SFX_EGH 0x1D       // SFX_1D_HITTING_AN_ENEMY_1: burst, 25000/32767, 1.0
#define FLOTSAM_SFX_SWIPE 0x103    // SFX_103_FLOTSAM_DEATH: 0.2 s into bursting, 32000/32767, 1.0
#define FLOTSAM_SFX_THUD 0x1D      // SFX_1D_HITTING_AN_ENEMY_1: hitting the player, 20000/32767, 1.0
#define FLOTSAM_SFX_FOOTSTEP 0x3F2 // SFX_3F2_BOING: each bounce
// Banjo-Kazooie's Grublins fade out between 1250 and 2500 units (500 and 1500 for footsteps);
// scaled to DK64 units, the bounces as the footsteps.
#define SFX_NEAR 310.0f
#define SFX_FAR 625.0f
#define FOOTSTEP_NEAR 125.0f
#define FOOTSTEP_FAR 375.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// How it moves, in DK64 units per second, and how fast it turns (a share of the way a frame).
#define FLOTSAM_WALK_SPEED 45.0f
#define FLOTSAM_CHASE_SPEED 110.0f
#define FLOTSAM_WALK_TURN 0.15f
#define FLOTSAM_CHASE_TURN 0.3f
// How long it stands at each end of its path, in seconds (a random time between the two).
#define FLOTSAM_PAUSE_MIN 0.8f
#define FLOTSAM_PAUSE_MAX 2.5f
// Seeing the player (as jiggy_grublins.c's Grublins do) and how far above or below; it gives up
// when the player gets this far off, or it gets this far from the middle of its path. Close in
// it chases straight away.
#define FLOTSAM_SEE_RADIUS 200.0f
#define FLOTSAM_SEE_HEIGHT 80.0f
#define FLOTSAM_LOSE_RADIUS 320.0f
#define FLOTSAM_LEASH 260.0f
#define FLOTSAM_POUNCE_RADIUS 62.0f
// Running into the player, and how long it backs off after (func_802DAE40's 90 frames); after
// losing the player, how long before it can see them again.
#define FLOTSAM_CONTACT_RADIUS 28.0f
#define FLOTSAM_CONTACT_HEIGHT 40.0f
#define FLOTSAM_BACK_OFF_SECONDS 3.0f
#define FLOTSAM_CALM_SECONDS 2.0f
// The player's attacks reach this far (the Simian Slam a little further), within this height;
// punches only hit what the player faces (within this many 12-bit angle units either side).
#define ATTACK_REACH 48.0f
#define SLAM_REACH 64.0f
#define ATTACK_HEIGHT 50.0f
#define PUNCH_ARC 0x300
// Following the floor: a step up or down of more than these blocks the way. Placing a Flotsam,
// the floor is looked for from this far above its spot and only trusted this close to it.
#define STEP_UP 20.0f
#define STEP_DOWN 30.0f
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f

// DK64 player states (Actor control_state) that hit a Flotsam.
#define STATE_SIMIAN_SLAM 0x1C
#define STATE_PUNCH 0x26
#define STATE_PUNCH_2 0x27
#define STATE_PUNCH_3 0x28
#define STATE_RUNNING_ATTACK 0x29
#define STATE_AERIAL_ATTACK 0x2B
#define STATE_AERIAL_ATTACK_DIDDY 0x2E
#define STATE_AERIAL_ATTACK_CHUNKY 0x2F
// DK64's player action for a touch from an attacking enemy (see the top of this file).
#define ACTION_ENEMY_HIT 0xE
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it): Flotsams wait it out.
#define CUTSCENE_LOCK 0x10

// Where they are, map by map. Frantic Factory's east corridor (off the Foyer's east side, its
// floor at 836): one up and down its south-west leg, one up and down its north leg, inside its
// walls and away from the drops of the west walkway and the round platform.
typedef struct {
    f32 ax, az; // One end of its path.
    f32 bx, bz; // The other.
} FlotsamPath;
static const FlotsamPath flotsam_paths_factory[] = {
    {1520.0f, 2480.0f, 1630.0f, 2530.0f}, // The south-west leg, along the notes.
    {1740.0f, 2410.0f, 1700.0f, 2300.0f}, // The north leg, toward the round room.
};
#define FLOTSAM_MAX 2
typedef struct {
    Maps map;
    f32 floor_y;                // The area's floor height, if the floor isn't found.
    f32 min_x, max_x, min_z, max_z; // They never leave this.
    f32 avoid_x, avoid_z, avoid_radius; // Or come this close to this spot (none here).
    const FlotsamPath *paths;
    u32 count;
} FlotsamMap;
static const FlotsamMap flotsam_maps[] = {
    {MAP_FACTORY, 836.0f, 1480.0f, 1780.0f, 2280.0f, 2545.0f, 0.0f, 0.0f, 0.0f, flotsam_paths_factory,
     sizeof(flotsam_paths_factory) / sizeof(flotsam_paths_factory[0])},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 D_global_asm_8076A0B1;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y, leaving it as it was if there's none.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// Whether the player can be hurt now (not invulnerable, not already knocked down).
s32 func_global_asm_806EB400(Actor *player);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);

typedef enum {
    FLOTSAMS_UNLOADED,
    FLOTSAMS_READY,
    FLOTSAMS_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} FlotsamsLoadState;

typedef enum {
    FLOTSAM_PAUSED,   // Standing at one end of its path.
    FLOTSAM_WALKING,  // To the other end.
    FLOTSAM_ALERTED,  // Seen the player: a bounce on the spot.
    FLOTSAM_CHASING,
    FLOTSAM_BACKING_OFF, // After hurting the player, or losing them: back to its path.
    FLOTSAM_DYING,
    FLOTSAM_GONE,
} FlotsamAction;

typedef struct {
    const FlotsamPath *path;
    f32 pos[3];
    f32 yaw; // 12-bit angle units, as a float so the turn eases.
    FlotsamAction action;
    s32 animation;
    f32 progress;  // 0 to 1 through the animation.
    f32 timer;     // Seconds left in a pause, or before it can see the player again.
    s32 to_b;      // Walking to its path's b end.
} Flotsam;

static u8 flotsam_block[FLOTSAM_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx flotsam_gfx[FLOTSAM_MAX][2][FLOTSAM_GFX_MAX];
static Mtx flotsam_mtx[FLOTSAM_MAX][2][FLOTSAM_MTX_MAX];
static JiggyBkPose flotsam_pose;
static FlotsamsLoadState flotsams_state = FLOTSAMS_UNLOADED;

static s32 flotsams_last_map = -1;
static u32 flotsams_last_timer;
static const FlotsamMap *flotsams_map; // This map's, or NULL.
static s32 flotsams_placed;
static s32 flotsams_floor_found; // The floor was found under them, so they follow it.
static Flotsam flotsams[FLOTSAM_MAX];
static u32 flotsams_random_state = 0x464C4F54;

static f32 flotsams_random(void) {
    flotsams_random_state = flotsams_random_state * 1664525 + 1013904223;
    return (flotsams_random_state >> 8) * (1.0f / 16777216.0f);
}

static void flotsams_load(void) {
    s32 size;
    s32 i;

    flotsams_state = FLOTSAMS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(FLOTSAM_MODEL, flotsam_block, sizeof(flotsam_block));
    if (size <= 0 || size > (s32)sizeof(flotsam_block)) {
        return;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        flotsam_pose.selectors[i] = 0;
    }
    flotsam_pose.selectors[1] = 1;
    flotsams_state = FLOTSAMS_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// A Banjo-Kazooie sound from where the Flotsam is, fading out between `near` and `far` from
// the player as Banjo-Kazooie's positional sounds do.
static void flotsam_sfx(Flotsam *g, Actor *player, u32 sfx, f32 volume, f32 pitch, f32 near, f32 far) {
    f32 distance = distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position);

    if (distance >= far) {
        return;
    }
    if (distance > near) {
        volume *= (far - distance) / (far - near);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static void flotsam_play(Flotsam *g, s32 animation) {
    if (g->animation != animation) {
        g->animation = animation;
        g->progress = 0.0f;
    }
}

// True when `at` is passed going from `from` to `to` through an animation.
static s32 flotsam_passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

// Advances a looping animation, with a boing at each bounce.
static void flotsam_step_animation(Flotsam *g, Actor *player, f32 seconds, f32 loop_seconds, s32 footsteps) {
    f32 before = g->progress;

    g->progress += seconds / loop_seconds;
    if (footsteps && g->progress >= 1.0f) {
        flotsam_sfx(g, player, FLOTSAM_SFX_FOOTSTEP, 0x7530 / 32767.0f, 1.0f + flotsams_random() * 0.2f, FOOTSTEP_NEAR,
                    FOOTSTEP_FAR);
    }
    while (g->progress >= 1.0f) {
        g->progress -= 1.0f;
    }
}

// Turns a share of the way to face (x, z), over `frames` frames.
static void flotsam_turn_to(Flotsam *g, f32 x, f32 z, f32 rate, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, z, g->pos[0], g->pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - g->yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        g->yaw += turn * rate;
        if (g->yaw < 0.0f) {
            g->yaw += 4096.0f;
        } else if (g->yaw >= 4096.0f) {
            g->yaw -= 4096.0f;
        }
    }
}

// Moves up to `step` toward (x, z), over the floor; a wall of a step, a drop, the edge of the
// area or the map's avoided spot stops it. Returns TRUE once it's there.
static s32 flotsam_move_to(Flotsam *g, f32 x, f32 z, f32 step) {
    f32 distance = distance_xz(x, z, g->pos[0], g->pos[2]);
    f32 nx, nz;
    f32 y = g->pos[1];

    if (distance <= step || distance < 1.0f) {
        nx = x;
        nz = z;
    } else {
        nx = g->pos[0] + (x - g->pos[0]) * step / distance;
        nz = g->pos[2] + (z - g->pos[2]) * step / distance;
    }
    if (nx < flotsams_map->min_x || nx > flotsams_map->max_x || nz < flotsams_map->min_z || nz > flotsams_map->max_z ||
        distance_xz(nx, nz, flotsams_map->avoid_x, flotsams_map->avoid_z) < flotsams_map->avoid_radius) {
        return FALSE;
    }
    if (flotsams_floor_found) {
        f32 probe = g->pos[1] + STEP_UP;
        f32 floor = probe;

        func_global_asm_80667110(nx, nz, &floor);
        if (floor == probe || floor > g->pos[1] + STEP_UP || floor < g->pos[1] - STEP_DOWN) {
            return FALSE;
        }
        y = floor;
    }
    g->pos[0] = nx;
    g->pos[1] = y;
    g->pos[2] = nz;
    return distance <= step;
}

// Stands the Flotsams over the floor at the starts of their paths (or at the area's floor
// height if no floor is found near).
static void flotsams_place(void) {
    u32 i;

    flotsams_floor_found = TRUE;
    for (i = 0; i < FLOTSAM_MAX; i++) {
        Flotsam *g = &flotsams[i];

        if (i >= flotsams_map->count) {
            g->action = FLOTSAM_GONE;
            continue;
        }
        g->path = &flotsams_map->paths[i];
        g->pos[0] = g->path->ax;
        g->pos[1] = flotsams_map->floor_y;
        g->pos[2] = g->path->az;
        {
            f32 probe = flotsams_map->floor_y + FLOOR_PROBE;
            f32 floor = probe;

            func_global_asm_80667110(g->pos[0], g->pos[2], &floor);
            if (floor != probe && floor - flotsams_map->floor_y < FLOOR_MAX_STEP &&
                flotsams_map->floor_y - floor < FLOOR_MAX_STEP) {
                g->pos[1] = floor;
            } else {
                // The floor isn't where it should be: they keep to the area's height instead.
                flotsams_floor_found = FALSE;
            }
        }
        g->yaw = func_global_asm_80665DE0(g->path->bx, g->path->bz, g->pos[0], g->pos[2]) & 0xFFF;
        g->action = FLOTSAM_PAUSED;
        g->animation = FLOTSAM_ANIM_WALK;
        g->progress = flotsams_random();
        g->timer = FLOTSAM_PAUSE_MIN + flotsams_random() * (FLOTSAM_PAUSE_MAX - FLOTSAM_PAUSE_MIN);
        g->to_b = TRUE;
    }
    if (!flotsams_floor_found) {
        for (i = 0; i < FLOTSAM_MAX && i < flotsams_map->count; i++) {
            flotsams[i].pos[1] = flotsams_map->floor_y;
        }
    }
    flotsams_placed = TRUE;
}

// Whether the player is where a Flotsam can see them.
static s32 flotsam_sees(Flotsam *g, Actor *player, f32 radius) {
    f32 dy = player->y_position - g->pos[1];

    return distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) <= radius &&
           dy <= FLOTSAM_SEE_HEIGHT && dy >= -FLOTSAM_SEE_HEIGHT;
}

s32 jiggy_banjo_air_attack(Actor *player);

// Whether the player's attack hits this Flotsam this frame.
static s32 flotsam_attacked(Flotsam *g, Actor *player) {
    f32 distance = distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - g->pos[1];
    s32 to_flotsam;
    s32 off;

    if (dy > ATTACK_HEIGHT || dy < -ATTACK_HEIGHT) {
        return FALSE;
    }
    switch (player->control_state) {
        case STATE_SIMIAN_SLAM:
            return distance <= SLAM_REACH;
        case STATE_PUNCH:
        case STATE_PUNCH_2:
        case STATE_PUNCH_3:
            if (distance > ATTACK_REACH) {
                return FALSE;
            }
            // Close in, a punch always lands; further out, only in front of the player.
            if (distance <= FLOTSAM_CONTACT_RADIUS) {
                return TRUE;
            }
            to_flotsam = func_global_asm_80665DE0(g->pos[0], g->pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_flotsam - (player->y_rotation & 0xFFF)) & 0xFFF;
            return off <= PUNCH_ARC || off >= 0x1000 - PUNCH_ARC;
        case STATE_RUNNING_ATTACK:
        case STATE_AERIAL_ATTACK:
        case STATE_AERIAL_ATTACK_DIDDY:
        case STATE_AERIAL_ATTACK_CHUNKY:
            return distance <= ATTACK_REACH;
        default:
            // Banjo's Rat-a-tat Rap and Beak Buster, which DK64's states don't show.
            return jiggy_banjo_air_attack(player) && distance <= ATTACK_REACH;
    }
}

// Defeated (chFlotsam_die).
static void flotsam_die(Flotsam *g, Actor *player) {
    g->action = FLOTSAM_DYING;
    flotsam_play(g, FLOTSAM_ANIM_DIE);
    g->progress = 0.0f;
    flotsam_sfx(g, player, FLOTSAM_SFX_EGH, 25000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
}

// Hurts the player through DK64's own enemy hit (see the top of this file), knocking them
// away from the Flotsam.
static void flotsam_hit_player(Flotsam *g, Actor *player) {
    // The knockback turns the player's heading round (func_global_asm_806EEAF8 with no enemy
    // actor), so it's set toward the Flotsam first.
    player->unkEE = func_global_asm_80665DE0(g->pos[0], g->pos[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
    flotsam_sfx(g, player, FLOTSAM_SFX_THUD, 20000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
    g->action = FLOTSAM_BACKING_OFF;
    g->timer = FLOTSAM_BACK_OFF_SECONDS;
}

static void flotsam_start_walking(Flotsam *g) {
    g->action = FLOTSAM_WALKING;
    flotsam_play(g, FLOTSAM_ANIM_WALK);
}

// A frame of one Flotsam's behavior (as jiggy_grublins.c runs a Grublin), `frames` game frames on.
static void flotsam_update(Flotsam *g, Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = g->progress;
    f32 mid_x = (g->path->ax + g->path->bx) * 0.5f;
    f32 mid_z = (g->path->az + g->path->bz) * 0.5f;
    f32 target_x = g->to_b ? g->path->bx : g->path->ax;
    f32 target_z = g->to_b ? g->path->bz : g->path->az;
    s32 live = g->action != FLOTSAM_DYING && g->action != FLOTSAM_GONE;

    if (frames == 0) {
        return;
    }
    // The player's attacks come first: a Flotsam hit as it touches the player doesn't hurt them.
    if (live && flotsam_attacked(g, player)) {
        flotsam_die(g, player);
        return;
    }
    if (live && g->action != FLOTSAM_BACKING_OFF &&
        distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) <= FLOTSAM_CONTACT_RADIUS &&
        player->y_position - g->pos[1] <= FLOTSAM_CONTACT_HEIGHT && g->pos[1] - player->y_position <= FLOTSAM_CONTACT_HEIGHT &&
        func_global_asm_806EB400(player)) {
        flotsam_hit_player(g, player);
    }

    switch (g->action) {
        case FLOTSAM_PAUSED:
        case FLOTSAM_WALKING:
            if (flotsam_sees(g, player, FLOTSAM_SEE_RADIUS)) {
                g->action = FLOTSAM_ALERTED;
                flotsam_play(g, FLOTSAM_ANIM_ALERT);
                g->progress = 0.0f;
                flotsam_sfx(g, player, FLOTSAM_SFX_NYAHAHA, 0x7530 / 32767.0f, 1.2f, SFX_NEAR, SFX_FAR);
                break;
            }
            if (g->action == FLOTSAM_PAUSED) {
                flotsam_play(g, FLOTSAM_ANIM_WALK);
                flotsam_step_animation(g, player, seconds, FLOTSAM_IDLE_SECONDS, FALSE);
                g->timer -= seconds;
                if (g->timer <= 0.0f) {
                    g->to_b = !g->to_b;
                    flotsam_start_walking(g);
                }
                break;
            }
            flotsam_turn_to(g, target_x, target_z, FLOTSAM_WALK_TURN, frames);
            flotsam_step_animation(g, player, seconds, FLOTSAM_WALK_SECONDS, TRUE);
            if (flotsam_move_to(g, target_x, target_z, FLOTSAM_WALK_SPEED * seconds)) {
                g->action = FLOTSAM_PAUSED;
                g->timer = FLOTSAM_PAUSE_MIN + flotsams_random() * (FLOTSAM_PAUSE_MAX - FLOTSAM_PAUSE_MIN);
            }
            break;
        case FLOTSAM_ALERTED:
            flotsam_turn_to(g, player->x_position, player->z_position, FLOTSAM_CHASE_TURN, frames);
            g->progress += seconds / FLOTSAM_ALERT_SECONDS;
            if (!flotsam_sees(g, player, FLOTSAM_LOSE_RADIUS)) {
                g->action = FLOTSAM_BACKING_OFF;
                g->timer = FLOTSAM_CALM_SECONDS;
                flotsam_play(g, FLOTSAM_ANIM_WALK);
            } else if (g->progress >= 1.0f || flotsam_sees(g, player, FLOTSAM_POUNCE_RADIUS)) {
                g->action = FLOTSAM_CHASING;
                flotsam_play(g, FLOTSAM_ANIM_CHASE);
            }
            break;
        case FLOTSAM_CHASING:
            if (!flotsam_sees(g, player, FLOTSAM_LOSE_RADIUS) ||
                distance_xz(g->pos[0], g->pos[2], mid_x, mid_z) > FLOTSAM_LEASH) {
                g->action = FLOTSAM_BACKING_OFF;
                g->timer = FLOTSAM_CALM_SECONDS;
                flotsam_play(g, FLOTSAM_ANIM_WALK);
                break;
            }
            flotsam_turn_to(g, player->x_position, player->z_position, FLOTSAM_CHASE_TURN, frames);
            flotsam_step_animation(g, player, seconds, FLOTSAM_CHASE_SECONDS, TRUE);
            // It stops short of the player rather than walking through them.
            if (distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) > FLOTSAM_CONTACT_RADIUS * 0.6f) {
                flotsam_move_to(g, player->x_position, player->z_position, FLOTSAM_CHASE_SPEED * seconds);
            }
            break;
        case FLOTSAM_BACKING_OFF: {
            // Back to whichever end of its path is further from the player.
            f32 to_a = distance_xz(g->path->ax, g->path->az, player->x_position, player->z_position);
            f32 to_b = distance_xz(g->path->bx, g->path->bz, player->x_position, player->z_position);

            g->to_b = to_b > to_a;
            target_x = g->to_b ? g->path->bx : g->path->ax;
            target_z = g->to_b ? g->path->bz : g->path->az;
            flotsam_play(g, FLOTSAM_ANIM_WALK);
            g->timer -= seconds;
            if (distance_xz(g->pos[0], g->pos[2], target_x, target_z) > 1.0f) {
                flotsam_turn_to(g, target_x, target_z, FLOTSAM_WALK_TURN, frames);
                flotsam_step_animation(g, player, seconds, FLOTSAM_WALK_SECONDS, TRUE);
                flotsam_move_to(g, target_x, target_z, FLOTSAM_WALK_SPEED * seconds);
            } else {
                flotsam_step_animation(g, player, seconds, FLOTSAM_IDLE_SECONDS, FALSE);
            }
            if (g->timer <= 0.0f) {
                g->action = FLOTSAM_PAUSED;
                g->timer = FLOTSAM_PAUSE_MIN;
            }
            break;
        }
        case FLOTSAM_DYING:
            g->progress += seconds / FLOTSAM_DIE_SECONDS;
            if (flotsam_passed(before, g->progress, 0.1f)) {
                flotsam_sfx(g, player, FLOTSAM_SFX_SWIPE, 32000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
            }
            if (g->progress >= 1.0f) {
                g->action = FLOTSAM_GONE;
            }
            break;
        default:
            break;
    }
}

static Gfx *flotsam_draw(Gfx *dl, Flotsam *g, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 progress = g->progress;

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, g->yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, flotsam_pose.world);
    flotsam_pose.world[3][0] = g->pos[0];
    flotsam_pose.world[3][1] = g->pos[1];
    flotsam_pose.world[3][2] = g->pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    flotsam_pose.model = FLOTSAM_MODEL;
    flotsam_pose.block = flotsam_block;
    flotsam_pose.animation = g->animation;
    flotsam_pose.progress = progress;
    flotsam_pose.gfx = gfx;
    flotsam_pose.gfx_capacity = FLOTSAM_GFX_MAX;
    flotsam_pose.mtx = mtx;
    flotsam_pose.mtx_capacity = FLOTSAM_MTX_MAX;
    if (jiggy_bk_model_pose(&flotsam_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws the Flotsams once a frame, with the player.
Gfx *jiggy_flotsam_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): every Flotsam is back.
    if ((s32)current_map != flotsams_last_map || object_timer < flotsams_last_timer) {
        flotsams_last_map = current_map;
        flotsams_placed = FALSE;
        flotsams_map = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(flotsam_maps); i++) {
            if (flotsam_maps[i].map == current_map) {
                flotsams_map = &flotsam_maps[i];
            }
        }
    }
    if (flotsams_map == NULL) {
        flotsams_last_timer = object_timer;
        return dl;
    }
    if (flotsams_state == FLOTSAMS_UNLOADED) {
        flotsams_load();
    }
    if (flotsams_state != FLOTSAMS_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!flotsams_placed) {
        flotsams_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        flotsams_place();
    }
    frames = object_timer - flotsams_last_timer;
    flotsams_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // They wait while a cutscene holds the player (Boss Boom Box's talk, for one).
    if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        frames = 0;
    }
    for (i = 0; i < FLOTSAM_MAX; i++) {
        Flotsam *g = &flotsams[i];

        if (g->action == FLOTSAM_GONE) {
            continue;
        }
        flotsam_update(g, actor, frames);
        if (g->action == FLOTSAM_GONE) {
            continue;
        }
        dl = flotsam_draw(dl, g, flotsam_gfx[i][buffer], flotsam_mtx[i][buffer]);
    }
    return dl;
}
