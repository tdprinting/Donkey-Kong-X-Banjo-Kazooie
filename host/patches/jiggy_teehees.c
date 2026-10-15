// Tee-Hees: Mad Monster Mansion's hooded ghosts (chTeeHee, src/core2/ch/ghost.c), read from the
// player's Banjo-Kazooie ROM, haunting Creepy Castle's hill, where the two worlds have collided.
//
// They work as jiggy_grublins.c's Grublins do: each Tee-Hee (ASSET_3CB_MODEL_TEEHEE) drifts up
// and down a short path, pausing now and then, bobbing as it floats; when it sees the player it
// giggles (SFX_3F4, as chTeehee's alert and chase do) through its alert animation and gives
// chase, giggling now and then. Running into the player hurts them once, through DK64's own
// enemy hit (setAction(0xE): a slice of health, a knockback and a moment's invulnerability),
// and it then backs off for a while.
//
// The player's attacks defeat it (punches, rolls, aerial attacks, the Simian Slam and Banjo's
// Rat-a-tat Rap and Beak Buster, as for the Grublins): it plays Banjo-Kazooie's death animation
// with chTeehee_die's "awawau" (SFX_121_AWAWAU) and magic poof (SFX_30_MAGIC_POOF), and is gone
// until the map is loaded again; nothing is saved and it drops nothing.
//
// The castle's paths are at many heights, so each path has its own; a Tee-Hee follows the floor
// along it and never steps off a ledge (a drop of more than STEP_DOWN stops it).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c).
// Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "jiggy.h"

#define TEEHEE_MODEL 0x3CB // ASSET_3CB_MODEL_TEEHEE
// The Tee-Hee converts to about 14 KiB, with 21 bones and about 20 pose commands.
#define TEEHEE_BLOCK_MAX 0x3A00
#define TEEHEE_GFX_MAX 64
#define TEEHEE_MTX_MAX 24

// Banjo-Kazooie animations (chTeeheeAnimations) and how long each plays, in seconds.
#define TEEHEE_ANIM_WALK 0x9E  // ASSET_9E_ANIM_TEEHEE_IDLE: idling and roaming
#define TEEHEE_ANIM_CHASE 0xAC // ASSET_AC_ANIM_TEEHEE_CHASE
#define TEEHEE_ANIM_ALERT 0x9F // ASSET_9F_ANIM_TEEHEE_ALERTED
#define TEEHEE_ANIM_DIE 0x2AB  // ASSET_2AB_ANIM_TEEHEE_DIE
#define TEEHEE_WALK_SECONDS 2.0f
#define TEEHEE_IDLE_SECONDS 2.0f
#define TEEHEE_CHASE_SECONDS 1.0f
#define TEEHEE_ALERT_SECONDS 1.5f
#define TEEHEE_DIE_SECONDS 1.0f

// Banjo-Kazooie sounds (sfx_e) and how chTeehee plays them.
#define TEEHEE_SFX_GIGGLE 0x3F4 // SFX_3F4: alerted and now and then in a chase, 32000/32767, 1.0 to 1.2
#define TEEHEE_SFX_AWAWAU 0x121 // SFX_121_AWAWAU: defeated, 32000/32767, 1.3
#define TEEHEE_SFX_POOF 0x30    // SFX_30_MAGIC_POOF: defeated, 32000/32767, 1.0
#define TEEHEE_SFX_THUD 0x1F    // SFX_1F_HITTING_AN_ENEMY_3: hitting the player, 20000/32767, 1.0
// A giggle on about one chase frame in ninety ((global_time & 0x3F) == 7 && randf() < 0.7).
#define TEEHEE_GIGGLE_CHANCE 0.011f
// Banjo-Kazooie fades these out between 400 and 1800 units (chTeehee's, at its 0.9 scale);
// scaled to DK64 units.
#define SFX_NEAR 90.0f
#define SFX_FAR 405.0f
#define FOOTSTEP_NEAR SFX_NEAR
#define FOOTSTEP_FAR SFX_FAR

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo, times chTeehee's 0.9.
#define BK_TO_DK64 (0.25f * 0.9f)
// The float: up and down this far, this often (seconds a bob).
#define TEEHEE_BOB_HEIGHT 3.0f
#define TEEHEE_BOB_SECONDS 1.6f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// How it moves, in DK64 units per second, and how fast it turns (a share of the way a frame).
#define TEEHEE_WALK_SPEED 40.0f
#define TEEHEE_CHASE_SPEED 100.0f
#define TEEHEE_WALK_TURN 0.15f
#define TEEHEE_CHASE_TURN 0.3f
// How long it stands at each end of its path, in seconds (chTeehee's idle, 1 to 4.5).
#define TEEHEE_PAUSE_MIN 1.0f
#define TEEHEE_PAUSE_MAX 4.5f
// Seeing the player (chTeehee's 600 units, at its 0.9 scale, scaled) and how far above or
// below; it gives up when the player gets this far off, or it gets this far from the middle of
// its path. Close in it chases straight away.
#define TEEHEE_SEE_RADIUS 135.0f
#define TEEHEE_SEE_HEIGHT 60.0f
#define TEEHEE_LOSE_RADIUS 260.0f
#define TEEHEE_LEASH 220.0f
#define TEEHEE_POUNCE_RADIUS 55.0f
// Running into the player, and how long it backs off after; after losing the player, how long
// before it can see them again.
#define TEEHEE_CONTACT_RADIUS 26.0f
#define TEEHEE_CONTACT_HEIGHT 40.0f
#define TEEHEE_BACK_OFF_SECONDS 3.0f
#define TEEHEE_CALM_SECONDS 2.0f
// The player's attacks reach this far (the Simian Slam a little further), within this height;
// punches only hit what the player faces (within this many 12-bit angle units either side).
#define ATTACK_REACH 48.0f
#define SLAM_REACH 64.0f
#define ATTACK_HEIGHT 50.0f
#define PUNCH_ARC 0x300
// Following the floor: a step up or down of more than these blocks the way. Placing a Tee-Hee,
// the floor is looked for from this far above its path's height and only trusted this close.
#define STEP_UP 20.0f
#define STEP_DOWN 30.0f
#define FLOOR_PROBE 20.0f
#define FLOOR_MAX_STEP 30.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f

// DK64 player states (Actor control_state) that hit a Tee-Hee.
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
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it): Tee-Hees wait it out.
#define CUTSCENE_LOCK 0x10

// Where they are, map by map. Each path's ends are on one floor, unbroken between them (checked
// against the map's floors), on ground DK64's own bananas mark as reachable.
typedef struct {
    f32 ax, az; // One end of its path.
    f32 bx, bz; // The other.
    f32 y;      // The path's floor height, if the floor isn't found.
} TeeHeePath;
static const TeeHeePath teehee_paths_castle[] = {
    // The grass past the drawbridge, between the tree and the gate, where the path up begins.
    {1150.0f, 150.0f, 1240.0f, 400.0f, 472.0f},
    // The south side of the middle path, west of its east end.
    {1500.0f, 1745.0f, 1700.0f, 1700.0f, 890.0f},
    // The top, north of the castle door, clear of Motzand's spot on the west side.
    {1450.0f, 1100.0f, 1650.0f, 1150.0f, 1731.0f},
};
#define TEEHEE_MAX 3
typedef struct {
    Maps map;
    const TeeHeePath *paths;
    u32 count;
} TeeHeeMap;
static const TeeHeeMap teehee_maps[] = {
    {MAP_CASTLE, teehee_paths_castle, sizeof(teehee_paths_castle) / sizeof(teehee_paths_castle[0])},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 D_global_asm_8076A0B1;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
// The floor under (x, z): writes its height to *y, leaving it as it was if there's none.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// Whether the player can be hurt now (not invulnerable, not already knocked down).
s32 func_global_asm_806EB400(Actor *player);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);

typedef enum {
    TEEHEES_UNLOADED,
    TEEHEES_READY,
    TEEHEES_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} TeeHeesLoadState;

typedef enum {
    TEEHEE_PAUSED,   // Standing at one end of its path.
    TEEHEE_WALKING,  // To the other end.
    TEEHEE_ALERTED,  // Seen the player: cackling.
    TEEHEE_CHASING,
    TEEHEE_BACKING_OFF, // After hurting the player, or losing them: back to its path.
    TEEHEE_DYING,
    TEEHEE_GONE,
} TeeHeeAction;

typedef struct {
    const TeeHeePath *path;
    f32 pos[3];
    f32 yaw; // 12-bit angle units, as a float so the turn eases.
    TeeHeeAction action;
    s32 animation;
    f32 progress;  // 0 to 1 through the animation.
    f32 timer;     // Seconds left in a pause, or before it can see the player again.
    s32 to_b;      // Walking to its path's b end.
    s32 floor_found; // The floor was found under it, so it follows it.
    f32 bob;       // 0 to 1 through its float up and down.
} TeeHee;

static u8 teehee_block[TEEHEE_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx teehee_gfx[TEEHEE_MAX][2][TEEHEE_GFX_MAX];
static Mtx teehee_mtx[TEEHEE_MAX][2][TEEHEE_MTX_MAX];
static JiggyBkPose teehee_pose;
static TeeHeesLoadState teehees_state = TEEHEES_UNLOADED;

static s32 teehees_last_map = -1;
static u32 teehees_last_timer;
static const TeeHeeMap *teehees_map; // This map's, or NULL.
static s32 teehees_placed;
static TeeHee teehees[TEEHEE_MAX];
static u32 teehees_random_state = 0x47525542;

static f32 teehees_random(void) {
    teehees_random_state = teehees_random_state * 1664525 + 1013904223;
    return (teehees_random_state >> 8) * (1.0f / 16777216.0f);
}

static void teehees_load(void) {
    s32 size;
    s32 i;

    teehees_state = TEEHEES_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(TEEHEE_MODEL, teehee_block, sizeof(teehee_block));
    if (size <= 0 || size > (s32)sizeof(teehee_block)) {
        return;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        teehee_pose.selectors[i] = 0;
    }
    teehee_pose.selectors[1] = 1;
    teehees_state = TEEHEES_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// A Banjo-Kazooie sound from where the TeeHee is, fading out between `near` and `far` from
// the player as Banjo-Kazooie's positional sounds do.
static void teehee_sfx(TeeHee *g, Actor *player, u32 sfx, f32 volume, f32 pitch, f32 near, f32 far) {
    f32 distance = distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position);

    if (distance >= far) {
        return;
    }
    if (distance > near) {
        volume *= (far - distance) / (far - near);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static void teehee_play(TeeHee *g, s32 animation) {
    if (g->animation != animation) {
        g->animation = animation;
        g->progress = 0.0f;
    }
}

// Advances a looping animation; with `giggles`, a giggle now and then (a chase).
static void teehee_step_animation(TeeHee *g, Actor *player, f32 seconds, f32 loop_seconds, s32 giggles) {
    g->progress += seconds / loop_seconds;
    // It floats, so there are no footsteps; a chase has a giggle now and then instead.
    if (giggles && teehees_random() < TEEHEE_GIGGLE_CHANCE * seconds * GAME_FPS) {
        teehee_sfx(g, player, TEEHEE_SFX_GIGGLE, 32000.0f / 32767.0f, 1.0f + teehees_random() * 0.2f, FOOTSTEP_NEAR,
                   FOOTSTEP_FAR);
    }
    while (g->progress >= 1.0f) {
        g->progress -= 1.0f;
    }
}

// Turns a share of the way to face (x, z), over `frames` frames.
static void teehee_turn_to(TeeHee *g, f32 x, f32 z, f32 rate, u32 frames) {
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

// Moves up to `step` toward (x, z), over the floor; a wall of a step or a drop stops it.
// Returns TRUE once it's there.
static s32 teehee_move_to(TeeHee *g, f32 x, f32 z, f32 step) {
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
    if (g->floor_found) {
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

// Stands the Tee-Hees over the floor at the starts of their paths (or at the path's height if
// no floor is found near, where they then keep to it).
static void teehees_place(void) {
    u32 i;

    for (i = 0; i < TEEHEE_MAX; i++) {
        TeeHee *g = &teehees[i];

        if (i >= teehees_map->count) {
            g->action = TEEHEE_GONE;
            continue;
        }
        g->path = &teehees_map->paths[i];
        g->pos[0] = g->path->ax;
        g->pos[1] = g->path->y;
        g->pos[2] = g->path->az;
        g->floor_found = FALSE;
        {
            f32 probe = g->path->y + FLOOR_PROBE;
            f32 floor = probe;

            func_global_asm_80667110(g->pos[0], g->pos[2], &floor);
            if (floor != probe && floor - g->path->y < FLOOR_MAX_STEP && g->path->y - floor < FLOOR_MAX_STEP) {
                g->pos[1] = floor;
                g->floor_found = TRUE;
            }
        }
        g->bob = teehees_random();
        g->yaw = func_global_asm_80665DE0(g->path->bx, g->path->bz, g->pos[0], g->pos[2]) & 0xFFF;
        g->action = TEEHEE_PAUSED;
        g->animation = TEEHEE_ANIM_WALK;
        g->progress = teehees_random();
        g->timer = TEEHEE_PAUSE_MIN + teehees_random() * (TEEHEE_PAUSE_MAX - TEEHEE_PAUSE_MIN);
        g->to_b = TRUE;
    }
    teehees_placed = TRUE;
}

// Whether the player is where a TeeHee can see them.
static s32 teehee_sees(TeeHee *g, Actor *player, f32 radius) {
    f32 dy = player->y_position - g->pos[1];

    return distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) <= radius &&
           dy <= TEEHEE_SEE_HEIGHT && dy >= -TEEHEE_SEE_HEIGHT;
}

s32 jiggy_banjo_air_attack(Actor *player);

// Whether the player's attack hits this TeeHee this frame.
static s32 teehee_attacked(TeeHee *g, Actor *player) {
    f32 distance = distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - g->pos[1];
    s32 to_teehee;
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
            if (distance <= TEEHEE_CONTACT_RADIUS) {
                return TRUE;
            }
            to_teehee = func_global_asm_80665DE0(g->pos[0], g->pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_teehee - (player->y_rotation & 0xFFF)) & 0xFFF;
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

// Defeated (chTeeHee_die).
static void teehee_die(TeeHee *g, Actor *player) {
    g->action = TEEHEE_DYING;
    teehee_play(g, TEEHEE_ANIM_DIE);
    g->progress = 0.0f;
    teehee_sfx(g, player, TEEHEE_SFX_AWAWAU, 32000.0f / 32767.0f, 1.3f, SFX_NEAR, SFX_FAR);
    teehee_sfx(g, player, TEEHEE_SFX_POOF, 32000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
}

// Hurts the player through DK64's own enemy hit (see the top of this file), knocking them
// away from the TeeHee.
static void teehee_hit_player(TeeHee *g, Actor *player) {
    // The knockback turns the player's heading round (func_global_asm_806EEAF8 with no enemy
    // actor), so it's set toward the TeeHee first.
    player->unkEE = func_global_asm_80665DE0(g->pos[0], g->pos[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
    teehee_sfx(g, player, TEEHEE_SFX_THUD, 20000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
    g->action = TEEHEE_BACKING_OFF;
    g->timer = TEEHEE_BACK_OFF_SECONDS;
}

static void teehee_start_walking(TeeHee *g) {
    g->action = TEEHEE_WALKING;
    teehee_play(g, TEEHEE_ANIM_WALK);
}

// A frame of one TeeHee's behavior (humanoidBaddie_update), `frames` game frames on.
static void teehee_update(TeeHee *g, Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 mid_x = (g->path->ax + g->path->bx) * 0.5f;
    f32 mid_z = (g->path->az + g->path->bz) * 0.5f;
    f32 target_x = g->to_b ? g->path->bx : g->path->ax;
    f32 target_z = g->to_b ? g->path->bz : g->path->az;
    s32 live = g->action != TEEHEE_DYING && g->action != TEEHEE_GONE;

    if (frames == 0) {
        return;
    }
    g->bob += seconds / TEEHEE_BOB_SECONDS;
    while (g->bob >= 1.0f) {
        g->bob -= 1.0f;
    }
    // The player's attacks come first: a TeeHee hit as it touches the player doesn't hurt them.
    if (live && teehee_attacked(g, player)) {
        teehee_die(g, player);
        return;
    }
    if (live && g->action != TEEHEE_BACKING_OFF &&
        distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) <= TEEHEE_CONTACT_RADIUS &&
        player->y_position - g->pos[1] <= TEEHEE_CONTACT_HEIGHT && g->pos[1] - player->y_position <= TEEHEE_CONTACT_HEIGHT &&
        func_global_asm_806EB400(player)) {
        teehee_hit_player(g, player);
    }

    switch (g->action) {
        case TEEHEE_PAUSED:
        case TEEHEE_WALKING:
            if (teehee_sees(g, player, TEEHEE_SEE_RADIUS)) {
                g->action = TEEHEE_ALERTED;
                teehee_play(g, TEEHEE_ANIM_ALERT);
                g->progress = 0.0f;
                teehee_sfx(g, player, TEEHEE_SFX_GIGGLE, 32000.0f / 32767.0f, 1.0f + teehees_random() * 0.2f, SFX_NEAR,
                           SFX_FAR);
                break;
            }
            if (g->action == TEEHEE_PAUSED) {
                teehee_play(g, TEEHEE_ANIM_WALK);
                teehee_step_animation(g, player, seconds, TEEHEE_IDLE_SECONDS, FALSE);
                g->timer -= seconds;
                if (g->timer <= 0.0f) {
                    g->to_b = !g->to_b;
                    teehee_start_walking(g);
                }
                break;
            }
            teehee_turn_to(g, target_x, target_z, TEEHEE_WALK_TURN, frames);
            teehee_step_animation(g, player, seconds, TEEHEE_WALK_SECONDS, FALSE);
            if (teehee_move_to(g, target_x, target_z, TEEHEE_WALK_SPEED * seconds)) {
                g->action = TEEHEE_PAUSED;
                g->timer = TEEHEE_PAUSE_MIN + teehees_random() * (TEEHEE_PAUSE_MAX - TEEHEE_PAUSE_MIN);
            }
            break;
        case TEEHEE_ALERTED:
            teehee_turn_to(g, player->x_position, player->z_position, TEEHEE_CHASE_TURN, frames);
            g->progress += seconds / TEEHEE_ALERT_SECONDS;
            if (!teehee_sees(g, player, TEEHEE_LOSE_RADIUS)) {
                g->action = TEEHEE_BACKING_OFF;
                g->timer = TEEHEE_CALM_SECONDS;
                teehee_play(g, TEEHEE_ANIM_WALK);
            } else if (g->progress >= 1.0f || teehee_sees(g, player, TEEHEE_POUNCE_RADIUS)) {
                g->action = TEEHEE_CHASING;
                teehee_play(g, TEEHEE_ANIM_CHASE);
            }
            break;
        case TEEHEE_CHASING:
            if (!teehee_sees(g, player, TEEHEE_LOSE_RADIUS) ||
                distance_xz(g->pos[0], g->pos[2], mid_x, mid_z) > TEEHEE_LEASH) {
                g->action = TEEHEE_BACKING_OFF;
                g->timer = TEEHEE_CALM_SECONDS;
                teehee_play(g, TEEHEE_ANIM_WALK);
                break;
            }
            teehee_turn_to(g, player->x_position, player->z_position, TEEHEE_CHASE_TURN, frames);
            teehee_step_animation(g, player, seconds, TEEHEE_CHASE_SECONDS, TRUE);
            // It stops short of the player rather than walking through them.
            if (distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) > TEEHEE_CONTACT_RADIUS * 0.6f) {
                teehee_move_to(g, player->x_position, player->z_position, TEEHEE_CHASE_SPEED * seconds);
            }
            break;
        case TEEHEE_BACKING_OFF: {
            // Back to whichever end of its path is further from the player.
            f32 to_a = distance_xz(g->path->ax, g->path->az, player->x_position, player->z_position);
            f32 to_b = distance_xz(g->path->bx, g->path->bz, player->x_position, player->z_position);

            g->to_b = to_b > to_a;
            target_x = g->to_b ? g->path->bx : g->path->ax;
            target_z = g->to_b ? g->path->bz : g->path->az;
            teehee_play(g, TEEHEE_ANIM_WALK);
            g->timer -= seconds;
            if (distance_xz(g->pos[0], g->pos[2], target_x, target_z) > 1.0f) {
                teehee_turn_to(g, target_x, target_z, TEEHEE_WALK_TURN, frames);
                teehee_step_animation(g, player, seconds, TEEHEE_WALK_SECONDS, FALSE);
                teehee_move_to(g, target_x, target_z, TEEHEE_WALK_SPEED * seconds);
            } else {
                teehee_step_animation(g, player, seconds, TEEHEE_IDLE_SECONDS, FALSE);
            }
            if (g->timer <= 0.0f) {
                g->action = TEEHEE_PAUSED;
                g->timer = TEEHEE_PAUSE_MIN;
            }
            break;
        }
        case TEEHEE_DYING:
            g->progress += seconds / TEEHEE_DIE_SECONDS;
            if (g->progress >= 1.0f) {
                g->action = TEEHEE_GONE;
            }
            break;
        default:
            break;
    }
}

static Gfx *teehee_draw(Gfx *dl, TeeHee *g, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 progress = g->progress;

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, g->yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, teehee_pose.world);
    teehee_pose.world[3][0] = g->pos[0];
    // Floating: a gentle bob over the floor (not while it fades away).
    teehee_pose.world[3][1] = g->pos[1];
    if (g->action != TEEHEE_DYING) {
        teehee_pose.world[3][1] += TEEHEE_BOB_HEIGHT * (1.0f + func_global_asm_80612794((s16)(g->bob * 4096.0f)));
    }
    teehee_pose.world[3][2] = g->pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    teehee_pose.model = TEEHEE_MODEL;
    teehee_pose.block = teehee_block;
    teehee_pose.animation = g->animation;
    teehee_pose.progress = progress;
    teehee_pose.gfx = gfx;
    teehee_pose.gfx_capacity = TEEHEE_GFX_MAX;
    teehee_pose.mtx = mtx;
    teehee_pose.mtx_capacity = TEEHEE_MTX_MAX;
    if (jiggy_bk_model_pose(&teehee_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws the TeeHees once a frame, with the player.
Gfx *jiggy_teehees_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): every TeeHee is back.
    if ((s32)current_map != teehees_last_map || object_timer < teehees_last_timer) {
        teehees_last_map = current_map;
        teehees_placed = FALSE;
        teehees_map = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(teehee_maps); i++) {
            if (teehee_maps[i].map == current_map) {
                teehees_map = &teehee_maps[i];
            }
        }
    }
    if (teehees_map == NULL) {
        teehees_last_timer = object_timer;
        return dl;
    }
    if (teehees_state == TEEHEES_UNLOADED) {
        teehees_load();
    }
    if (teehees_state != TEEHEES_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!teehees_placed) {
        teehees_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        teehees_place();
    }
    frames = object_timer - teehees_last_timer;
    teehees_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // They wait while a cutscene holds the player (Motzand's or Tumblar's talk, for one).
    if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        frames = 0;
    }
    for (i = 0; i < TEEHEE_MAX; i++) {
        TeeHee *g = &teehees[i];

        if (g->action == TEEHEE_GONE) {
            continue;
        }
        teehee_update(g, actor, frames);
        if (g->action == TEEHEE_GONE) {
            continue;
        }
        dl = teehee_draw(dl, g, teehee_gfx[i][buffer], teehee_mtx[i][buffer]);
    }
    return dl;
}
