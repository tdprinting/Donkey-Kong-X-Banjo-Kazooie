// Grublins: Mumbo's Mountain's hooded enemies, read from the player's Banjo-Kazooie ROM, as
// enemies in Jungle Japes' first area, where the two worlds have collided.
//
// Like chGrublin (src/MM/ch/grublin.c) and the humanoid baddies it runs on
// (humanoidBaddie_update, src/core2/code_53C10.c), each Grublin (ASSET_3C5_MODEL_GRUBLIN)
// wanders up and down a short path, pausing now and then; when it sees the player it cackles
// (SFX_29_GRUBLIN_NYAHAHA) through its alert animation and gives chase. Running into the
// player hurts them once, through DK64's own path for an enemy's hit: setAction(0xE), as the
// player's collision code (code_7CA80.c) sets for a touch from an attacking enemy, whose
// handler (func_global_asm_806ECDD4) takes a slice of health, knocks the player back and gives
// them a short invulnerability (PlayerAdditionalActorData unkD4, checked by
// func_global_asm_806EB400). The Grublin then backs off for a while, as func_802DAE40 turns it
// away after a hit.
//
// The player's attacks defeat it: DK64's punches (control_state 0x26 to 0x28, the states
// Banjo's Claw Swipe plays on), the rolling and running attacks (0x29, Banjo's Beak Barge), the
// aerial attacks (0x2B, 0x2E, 0x2F) and the Simian Slam (0x1C), when the player is close
// enough. A defeated Grublin plays Banjo-Kazooie's death animation and sounds (chGrublin_die
// and chGrublin_update) and is gone until the map is loaded again; nothing is saved and it
// drops nothing.
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), as
// Bottles is (jiggy_bottles.c). Without a Banjo-Kazooie ROM nothing is drawn and nothing
// happens.

#include "common_structs.h"
#include "jiggy.h"

#define GRUBLIN_MODEL 0x3C5 // ASSET_3C5_MODEL_GRUBLIN
// The Grublin converts to about 24 KiB, with 35 bones and about 80 pose commands.
#define GRUBLIN_BLOCK_MAX 0x7000
#define GRUBLIN_GFX_MAX 128
#define GRUBLIN_MTX_MAX 40

// Banjo-Kazooie animations (chGrublinAnimations) and how long each plays, in seconds.
#define GRUBLIN_ANIM_WALK 0x62  // ASSET_62_ANIM_GRUBLIN_IDLE_WALK
#define GRUBLIN_ANIM_CHASE 0x63 // ASSET_63_ANIM_GRUBLIN_CHASE
#define GRUBLIN_ANIM_ALERT 0x64 // ASSET_64_ANIM_GRUBLIN_ALERT
#define GRUBLIN_ANIM_DIE 0x94   // ASSET_94_ANIM_GRUBLIN_DIE
#define GRUBLIN_WALK_SECONDS 0.7f
#define GRUBLIN_IDLE_SECONDS 4.0f // The walk, slowed right down, while it stands about.
#define GRUBLIN_CHASE_SECONDS 0.5f
#define GRUBLIN_ALERT_SECONDS 0.9f
#define GRUBLIN_DIE_SECONDS 1.0f

// Banjo-Kazooie sounds (sfx_e) and how chGrublin plays them.
#define GRUBLIN_SFX_NYAHAHA 0x29   // SFX_29_GRUBLIN_NYAHAHA: seeing the player, 25000/32767, 1.0
#define GRUBLIN_SFX_EGH 0xC2       // SFX_C2_GRUBLIN_EGH: defeated, 32000/32767, 1.0
#define GRUBLIN_SFX_SWIPE 0x02     // SFX_2_CLAW_SWIPE: at 0.18 through dying, 28000/32767, 1.0
#define GRUBLIN_SFX_THUD 0x1F      // SFX_1F_HITTING_AN_ENEMY_3: at 0.7 through dying, 28000/32767,
                                   // 1.0; hitting the player (func_802DB440), 20000/32767, 1.0
#define GRUBLIN_SFX_FOOTSTEP 0x08  // SFX_8_BANJO_LANDING_04: each step (func_802DB3B0), 8000/32767, 1.8
// Banjo-Kazooie fades these out between 1250 and 2500 units (500 and 1500 for footsteps);
// scaled to DK64 units.
#define SFX_NEAR 310.0f
#define SFX_FAR 625.0f
#define FOOTSTEP_NEAR 125.0f
#define FOOTSTEP_FAR 375.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// How it moves, in DK64 units per second, and how fast it turns (a share of the way a frame).
#define GRUBLIN_WALK_SPEED 45.0f
#define GRUBLIN_CHASE_SPEED 110.0f
#define GRUBLIN_WALK_TURN 0.15f
#define GRUBLIN_CHASE_TURN 0.3f
// How long it stands at each end of its path, in seconds (a random time between the two).
#define GRUBLIN_PAUSE_MIN 0.8f
#define GRUBLIN_PAUSE_MAX 2.5f
// Seeing the player (humanoidBaddie's 900 units, scaled) and how far above or below; it gives up
// when the player gets this far off, or it gets this far from the middle of its path. Close in
// (humanoidBaddie's 250 units, scaled) it chases straight away.
#define GRUBLIN_SEE_RADIUS 200.0f
#define GRUBLIN_SEE_HEIGHT 80.0f
#define GRUBLIN_LOSE_RADIUS 320.0f
#define GRUBLIN_LEASH 260.0f
#define GRUBLIN_POUNCE_RADIUS 62.0f
// Running into the player, and how long it backs off after (func_802DAE40's 90 frames); after
// losing the player, how long before it can see them again.
#define GRUBLIN_CONTACT_RADIUS 28.0f
#define GRUBLIN_CONTACT_HEIGHT 40.0f
#define GRUBLIN_BACK_OFF_SECONDS 3.0f
#define GRUBLIN_CALM_SECONDS 2.0f
// The player's attacks reach this far (the Simian Slam a little further), within this height;
// punches only hit what the player faces (within this many 12-bit angle units either side).
#define ATTACK_REACH 48.0f
#define SLAM_REACH 64.0f
#define ATTACK_HEIGHT 50.0f
#define PUNCH_ARC 0x300
// Following the floor: a step up or down of more than these blocks the way. Placing a Grublin,
// the floor is looked for from this far above its spot and only trusted this close to it.
#define STEP_UP 20.0f
#define STEP_DOWN 30.0f
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f

// DK64 player states (Actor control_state) that hit a Grublin.
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
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it): Grublins wait it out.
#define CUTSCENE_LOCK 0x10

// Where they are, map by map. Jungle Japes' first area: three short paths on the open grass,
// clear of Bottles' molehill (1120, 560), the entrance (931, 280, 721) and the collectibles.
typedef struct {
    f32 ax, az; // One end of its path.
    f32 bx, bz; // The other.
} GrublinPath;
static const GrublinPath grublin_paths_japes[] = {
    {1070.0f, 340.0f, 1180.0f, 335.0f}, // Along the notes' trail, between the Mumbo Token and the bend.
    // West of Bottles, round the red feather: not by Chimpy, where a hit from the west knocked the
    // player into the out of bounds north-east corner.
    {990.0f, 460.0f, 1070.0f, 530.0f},
    {1220.0f, 245.0f, 1330.0f, 225.0f}, // North, between the Jinjos and the trail.
};
#define GRUBLIN_MAX 3
typedef struct {
    Maps map;
    f32 floor_y;                // The area's floor height, if the floor isn't found.
    f32 min_x, max_x, min_z, max_z; // They never leave this.
    f32 avoid_x, avoid_z, avoid_radius; // Or come this close to Bottles' molehill.
    const GrublinPath *paths;
    u32 count;
} GrublinMap;
static const GrublinMap grublin_maps[] = {
    {MAP_JAPES, 280.0f, 930.0f, 1460.0f, 150.0f, 720.0f, 1120.0f, 560.0f, 50.0f, grublin_paths_japes,
     sizeof(grublin_paths_japes) / sizeof(grublin_paths_japes[0])},
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
    GRUBLINS_UNLOADED,
    GRUBLINS_READY,
    GRUBLINS_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} GrublinsLoadState;

typedef enum {
    GRUBLIN_PAUSED,   // Standing at one end of its path.
    GRUBLIN_WALKING,  // To the other end.
    GRUBLIN_ALERTED,  // Seen the player: cackling.
    GRUBLIN_CHASING,
    GRUBLIN_BACKING_OFF, // After hurting the player, or losing them: back to its path.
    GRUBLIN_DYING,
    GRUBLIN_GONE,
} GrublinAction;

typedef struct {
    const GrublinPath *path;
    f32 pos[3];
    f32 yaw; // 12-bit angle units, as a float so the turn eases.
    GrublinAction action;
    s32 animation;
    f32 progress;  // 0 to 1 through the animation.
    f32 timer;     // Seconds left in a pause, or before it can see the player again.
    s32 to_b;      // Walking to its path's b end.
} Grublin;

static u8 grublin_block[GRUBLIN_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx grublin_gfx[GRUBLIN_MAX][2][GRUBLIN_GFX_MAX];
static Mtx grublin_mtx[GRUBLIN_MAX][2][GRUBLIN_MTX_MAX];
static JiggyBkPose grublin_pose;
static GrublinsLoadState grublins_state = GRUBLINS_UNLOADED;

static s32 grublins_last_map = -1;
static u32 grublins_last_timer;
static const GrublinMap *grublins_map; // This map's, or NULL.
static s32 grublins_placed;
static s32 grublins_floor_found; // The floor was found under them, so they follow it.
static Grublin grublins[GRUBLIN_MAX];
static u32 grublins_random_state = 0x47525542;

static f32 grublins_random(void) {
    grublins_random_state = grublins_random_state * 1664525 + 1013904223;
    return (grublins_random_state >> 8) * (1.0f / 16777216.0f);
}

static void grublins_load(void) {
    s32 size;
    s32 i;

    grublins_state = GRUBLINS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(GRUBLIN_MODEL, grublin_block, sizeof(grublin_block));
    if (size <= 0 || size > (s32)sizeof(grublin_block)) {
        return;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        grublin_pose.selectors[i] = 0;
    }
    grublin_pose.selectors[1] = 1;
    grublins_state = GRUBLINS_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// A Banjo-Kazooie sound from where the Grublin is, fading out between `near` and `far` from
// the player as Banjo-Kazooie's positional sounds do.
static void grublin_sfx(Grublin *g, Actor *player, u32 sfx, f32 volume, f32 pitch, f32 near, f32 far) {
    f32 distance = distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position);

    if (distance >= far) {
        return;
    }
    if (distance > near) {
        volume *= (far - distance) / (far - near);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static void grublin_play(Grublin *g, s32 animation) {
    if (g->animation != animation) {
        g->animation = animation;
        g->progress = 0.0f;
    }
}

// True when `at` is passed going from `from` to `to` through an animation.
static s32 grublin_passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

// Advances a looping animation, with a footstep at the start and middle of each loop.
static void grublin_step_animation(Grublin *g, Actor *player, f32 seconds, f32 loop_seconds, s32 footsteps) {
    f32 before = g->progress;

    g->progress += seconds / loop_seconds;
    if (footsteps && (grublin_passed(before, g->progress, 0.5f) || g->progress >= 1.0f)) {
        grublin_sfx(g, player, GRUBLIN_SFX_FOOTSTEP, 8000.0f / 32767.0f, 1.8f, FOOTSTEP_NEAR, FOOTSTEP_FAR);
    }
    while (g->progress >= 1.0f) {
        g->progress -= 1.0f;
    }
}

// Turns a share of the way to face (x, z), over `frames` frames.
static void grublin_turn_to(Grublin *g, f32 x, f32 z, f32 rate, u32 frames) {
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
// area or Bottles' molehill stops it. Returns TRUE once it's there.
static s32 grublin_move_to(Grublin *g, f32 x, f32 z, f32 step) {
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
    if (nx < grublins_map->min_x || nx > grublins_map->max_x || nz < grublins_map->min_z || nz > grublins_map->max_z ||
        distance_xz(nx, nz, grublins_map->avoid_x, grublins_map->avoid_z) < grublins_map->avoid_radius) {
        return FALSE;
    }
    if (grublins_floor_found) {
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

// Stands the Grublins over the floor at the starts of their paths (or at the area's floor
// height if no floor is found near).
static void grublins_place(void) {
    u32 i;

    grublins_floor_found = TRUE;
    for (i = 0; i < GRUBLIN_MAX; i++) {
        Grublin *g = &grublins[i];

        if (i >= grublins_map->count) {
            g->action = GRUBLIN_GONE;
            continue;
        }
        g->path = &grublins_map->paths[i];
        g->pos[0] = g->path->ax;
        g->pos[1] = grublins_map->floor_y;
        g->pos[2] = g->path->az;
        {
            f32 probe = grublins_map->floor_y + FLOOR_PROBE;
            f32 floor = probe;

            func_global_asm_80667110(g->pos[0], g->pos[2], &floor);
            if (floor != probe && floor - grublins_map->floor_y < FLOOR_MAX_STEP &&
                grublins_map->floor_y - floor < FLOOR_MAX_STEP) {
                g->pos[1] = floor;
            } else {
                // The floor isn't where it should be: they keep to the area's height instead.
                grublins_floor_found = FALSE;
            }
        }
        g->yaw = func_global_asm_80665DE0(g->path->bx, g->path->bz, g->pos[0], g->pos[2]) & 0xFFF;
        g->action = GRUBLIN_PAUSED;
        g->animation = GRUBLIN_ANIM_WALK;
        g->progress = grublins_random();
        g->timer = GRUBLIN_PAUSE_MIN + grublins_random() * (GRUBLIN_PAUSE_MAX - GRUBLIN_PAUSE_MIN);
        g->to_b = TRUE;
    }
    if (!grublins_floor_found) {
        for (i = 0; i < GRUBLIN_MAX && i < grublins_map->count; i++) {
            grublins[i].pos[1] = grublins_map->floor_y;
        }
    }
    grublins_placed = TRUE;
}

// Whether the player is where a Grublin can see them.
static s32 grublin_sees(Grublin *g, Actor *player, f32 radius) {
    f32 dy = player->y_position - g->pos[1];

    return distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) <= radius &&
           dy <= GRUBLIN_SEE_HEIGHT && dy >= -GRUBLIN_SEE_HEIGHT;
}

s32 jiggy_banjo_air_attack(Actor *player);

// Whether the player's attack hits this Grublin this frame.
static s32 grublin_attacked(Grublin *g, Actor *player) {
    f32 distance = distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - g->pos[1];
    s32 to_grublin;
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
            if (distance <= GRUBLIN_CONTACT_RADIUS) {
                return TRUE;
            }
            to_grublin = func_global_asm_80665DE0(g->pos[0], g->pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_grublin - (player->y_rotation & 0xFFF)) & 0xFFF;
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

// Defeated (chGrublin_die).
static void grublin_die(Grublin *g, Actor *player) {
    g->action = GRUBLIN_DYING;
    grublin_play(g, GRUBLIN_ANIM_DIE);
    g->progress = 0.0f;
    grublin_sfx(g, player, GRUBLIN_SFX_EGH, 32000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
}

// Hurts the player through DK64's own enemy hit (see the top of this file), knocking them
// away from the Grublin.
static void grublin_hit_player(Grublin *g, Actor *player) {
    // The knockback turns the player's heading round (func_global_asm_806EEAF8 with no enemy
    // actor), so it's set toward the Grublin first.
    player->unkEE = func_global_asm_80665DE0(g->pos[0], g->pos[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
    grublin_sfx(g, player, GRUBLIN_SFX_THUD, 20000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
    g->action = GRUBLIN_BACKING_OFF;
    g->timer = GRUBLIN_BACK_OFF_SECONDS;
}

static void grublin_start_walking(Grublin *g) {
    g->action = GRUBLIN_WALKING;
    grublin_play(g, GRUBLIN_ANIM_WALK);
}

// A frame of one Grublin's behavior (humanoidBaddie_update), `frames` game frames on.
static void grublin_update(Grublin *g, Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = g->progress;
    f32 mid_x = (g->path->ax + g->path->bx) * 0.5f;
    f32 mid_z = (g->path->az + g->path->bz) * 0.5f;
    f32 target_x = g->to_b ? g->path->bx : g->path->ax;
    f32 target_z = g->to_b ? g->path->bz : g->path->az;
    s32 live = g->action != GRUBLIN_DYING && g->action != GRUBLIN_GONE;

    if (frames == 0) {
        return;
    }
    // The player's attacks come first: a Grublin hit as it touches the player doesn't hurt them.
    if (live && grublin_attacked(g, player)) {
        grublin_die(g, player);
        return;
    }
    if (live && g->action != GRUBLIN_BACKING_OFF &&
        distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) <= GRUBLIN_CONTACT_RADIUS &&
        player->y_position - g->pos[1] <= GRUBLIN_CONTACT_HEIGHT && g->pos[1] - player->y_position <= GRUBLIN_CONTACT_HEIGHT &&
        func_global_asm_806EB400(player)) {
        grublin_hit_player(g, player);
    }

    switch (g->action) {
        case GRUBLIN_PAUSED:
        case GRUBLIN_WALKING:
            if (grublin_sees(g, player, GRUBLIN_SEE_RADIUS)) {
                g->action = GRUBLIN_ALERTED;
                grublin_play(g, GRUBLIN_ANIM_ALERT);
                g->progress = 0.0f;
                grublin_sfx(g, player, GRUBLIN_SFX_NYAHAHA, 25000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
                break;
            }
            if (g->action == GRUBLIN_PAUSED) {
                grublin_play(g, GRUBLIN_ANIM_WALK);
                grublin_step_animation(g, player, seconds, GRUBLIN_IDLE_SECONDS, FALSE);
                g->timer -= seconds;
                if (g->timer <= 0.0f) {
                    g->to_b = !g->to_b;
                    grublin_start_walking(g);
                }
                break;
            }
            grublin_turn_to(g, target_x, target_z, GRUBLIN_WALK_TURN, frames);
            grublin_step_animation(g, player, seconds, GRUBLIN_WALK_SECONDS, TRUE);
            if (grublin_move_to(g, target_x, target_z, GRUBLIN_WALK_SPEED * seconds)) {
                g->action = GRUBLIN_PAUSED;
                g->timer = GRUBLIN_PAUSE_MIN + grublins_random() * (GRUBLIN_PAUSE_MAX - GRUBLIN_PAUSE_MIN);
            }
            break;
        case GRUBLIN_ALERTED:
            grublin_turn_to(g, player->x_position, player->z_position, GRUBLIN_CHASE_TURN, frames);
            g->progress += seconds / GRUBLIN_ALERT_SECONDS;
            if (!grublin_sees(g, player, GRUBLIN_LOSE_RADIUS)) {
                g->action = GRUBLIN_BACKING_OFF;
                g->timer = GRUBLIN_CALM_SECONDS;
                grublin_play(g, GRUBLIN_ANIM_WALK);
            } else if (g->progress >= 1.0f || grublin_sees(g, player, GRUBLIN_POUNCE_RADIUS)) {
                g->action = GRUBLIN_CHASING;
                grublin_play(g, GRUBLIN_ANIM_CHASE);
            }
            break;
        case GRUBLIN_CHASING:
            if (!grublin_sees(g, player, GRUBLIN_LOSE_RADIUS) ||
                distance_xz(g->pos[0], g->pos[2], mid_x, mid_z) > GRUBLIN_LEASH) {
                g->action = GRUBLIN_BACKING_OFF;
                g->timer = GRUBLIN_CALM_SECONDS;
                grublin_play(g, GRUBLIN_ANIM_WALK);
                break;
            }
            grublin_turn_to(g, player->x_position, player->z_position, GRUBLIN_CHASE_TURN, frames);
            grublin_step_animation(g, player, seconds, GRUBLIN_CHASE_SECONDS, TRUE);
            // It stops short of the player rather than walking through them.
            if (distance_xz(g->pos[0], g->pos[2], player->x_position, player->z_position) > GRUBLIN_CONTACT_RADIUS * 0.6f) {
                grublin_move_to(g, player->x_position, player->z_position, GRUBLIN_CHASE_SPEED * seconds);
            }
            break;
        case GRUBLIN_BACKING_OFF: {
            // Back to whichever end of its path is further from the player.
            f32 to_a = distance_xz(g->path->ax, g->path->az, player->x_position, player->z_position);
            f32 to_b = distance_xz(g->path->bx, g->path->bz, player->x_position, player->z_position);

            g->to_b = to_b > to_a;
            target_x = g->to_b ? g->path->bx : g->path->ax;
            target_z = g->to_b ? g->path->bz : g->path->az;
            grublin_play(g, GRUBLIN_ANIM_WALK);
            g->timer -= seconds;
            if (distance_xz(g->pos[0], g->pos[2], target_x, target_z) > 1.0f) {
                grublin_turn_to(g, target_x, target_z, GRUBLIN_WALK_TURN, frames);
                grublin_step_animation(g, player, seconds, GRUBLIN_WALK_SECONDS, TRUE);
                grublin_move_to(g, target_x, target_z, GRUBLIN_WALK_SPEED * seconds);
            } else {
                grublin_step_animation(g, player, seconds, GRUBLIN_IDLE_SECONDS, FALSE);
            }
            if (g->timer <= 0.0f) {
                g->action = GRUBLIN_PAUSED;
                g->timer = GRUBLIN_PAUSE_MIN;
            }
            break;
        }
        case GRUBLIN_DYING:
            g->progress += seconds / GRUBLIN_DIE_SECONDS;
            if (grublin_passed(before, g->progress, 0.18f)) {
                grublin_sfx(g, player, GRUBLIN_SFX_SWIPE, 28000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
            }
            if (grublin_passed(before, g->progress, 0.7f)) {
                grublin_sfx(g, player, GRUBLIN_SFX_THUD, 28000.0f / 32767.0f, 1.0f, SFX_NEAR, SFX_FAR);
            }
            if (g->progress >= 1.0f) {
                g->action = GRUBLIN_GONE;
            }
            break;
        default:
            break;
    }
}

static Gfx *grublin_draw(Gfx *dl, Grublin *g, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 progress = g->progress;

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, g->yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, grublin_pose.world);
    grublin_pose.world[3][0] = g->pos[0];
    grublin_pose.world[3][1] = g->pos[1];
    grublin_pose.world[3][2] = g->pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    grublin_pose.model = GRUBLIN_MODEL;
    grublin_pose.block = grublin_block;
    grublin_pose.animation = g->animation;
    grublin_pose.progress = progress;
    grublin_pose.gfx = gfx;
    grublin_pose.gfx_capacity = GRUBLIN_GFX_MAX;
    grublin_pose.mtx = mtx;
    grublin_pose.mtx_capacity = GRUBLIN_MTX_MAX;
    if (jiggy_bk_model_pose(&grublin_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws the Grublins once a frame, with the player.
Gfx *jiggy_grublins_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): every Grublin is back.
    if ((s32)current_map != grublins_last_map || object_timer < grublins_last_timer) {
        grublins_last_map = current_map;
        grublins_placed = FALSE;
        grublins_map = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(grublin_maps); i++) {
            if (grublin_maps[i].map == current_map) {
                grublins_map = &grublin_maps[i];
            }
        }
    }
    if (grublins_map == NULL) {
        grublins_last_timer = object_timer;
        return dl;
    }
    if (grublins_state == GRUBLINS_UNLOADED) {
        grublins_load();
    }
    if (grublins_state != GRUBLINS_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!grublins_placed) {
        grublins_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        grublins_place();
    }
    frames = object_timer - grublins_last_timer;
    grublins_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // They wait while a cutscene holds the player (Bottles' talk, for one).
    if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        frames = 0;
    }
    for (i = 0; i < GRUBLIN_MAX; i++) {
        Grublin *g = &grublins[i];

        if (g->action == GRUBLIN_GONE) {
            continue;
        }
        grublin_update(g, actor, frames);
        if (g->action == GRUBLIN_GONE) {
            continue;
        }
        dl = grublin_draw(dl, g, grublin_gfx[i][buffer], grublin_mtx[i][buffer]);
    }
    return dl;
}
