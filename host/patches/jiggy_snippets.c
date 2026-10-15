// Snippets: Treasure Trove Cove's and Clanker's Cavern's crabs, read from the player's
// Banjo-Kazooie ROM, as enemies in Gloomy Galleon, where the worlds have collided.
//
// Like chCrab (src/core2/ch/crab.c), each Snippet (ASSET_358_SNIPPET) idles
// (ASSET_1DA_ANIM_CRAB_IDLE) and scuttles about a short path (ASSET_2C_ANIM_CRAB_WALK, its
// SFX_3D_TICKER_WALKING at 0.4 and 0.9 through each step); when it sees the player it turns to
// them and gives chase, the walk sped up (chCrab's 0.7 second loop). Pinching the player hurts
// them once, through DK64's own path for an enemy's hit (setAction(0xE), as jiggy_grublins.c
// does), with SFX_1E_HITTING_AN_ENEMY_2, and the Snippet backs off for a while.
//
// As in Banjo-Kazooie, a Snippet's shell shrugs off a first blow: it flips onto its back
// (ASSET_97_ANIM_CRAB_FLIP_UPSIDEDOWN, SFX_6E_VILE_EGH at pitch 2.0, as __chCrab_ow plays it) and
// kicks there (ASSET_96_ANIM_CRAB_UPSIDEDOWN) for three seconds before flipping back
// (ASSET_155_ANIM_CRAB_FLIP_UPRIGHT). Hit again while it's on its back (one of the player's
// attacks: the same moves as the Grublins, or a shot), it's defeated: SFX_79_TICKER_DEATH twice
// (__chCrab_die), and it shrinks away. It's gone until the map is loaded again; nothing is saved
// and it drops nothing.
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c).
// Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "jiggy.h"

#define SNIPPET_MODEL 0x358 // ASSET_358_SNIPPET
// A Snippet converts to 20304 bytes, with 41 bones and 40 pose commands.
#define SNIPPET_BLOCK_MAX 0x5000
#define SNIPPET_GFX_MAX 64
#define SNIPPET_MTX_MAX 44

// Banjo-Kazooie animations (chCrabAnimations) and how long each plays, in seconds.
#define SNIPPET_ANIM_WALK 0x2C       // ASSET_2C_ANIM_CRAB_WALK
#define SNIPPET_ANIM_BACK 0x96       // ASSET_96_ANIM_CRAB_UPSIDEDOWN
#define SNIPPET_ANIM_FLIP_OVER 0x97  // ASSET_97_ANIM_CRAB_FLIP_UPSIDEDOWN
#define SNIPPET_ANIM_FLIP_BACK 0x155 // ASSET_155_ANIM_CRAB_FLIP_UPRIGHT
#define SNIPPET_ANIM_IDLE 0x1DA      // ASSET_1DA_ANIM_CRAB_IDLE
#define SNIPPET_IDLE_SECONDS 0.8f
#define SNIPPET_WALK_SECONDS 2.0f
#define SNIPPET_CHASE_SECONDS 0.7f
#define SNIPPET_BACK_SECONDS 0.5f
#define SNIPPET_FLIP_OVER_SECONDS 0.8f
#define SNIPPET_FLIP_BACK_SECONDS 0.55f
#define SNIPPET_DIE_SECONDS 0.4f
// How long it lies on its back (__chCrab_ow's lifetime of 3).
#define SNIPPET_ON_BACK_SECONDS 3.0f

// Banjo-Kazooie sounds (sfx_e) and how chCrab plays them.
#define SNIPPET_SFX_STEP 0x3D   // SFX_3D_TICKER_WALKING: 15000/32767, pitch 0.85 and 1.15
#define SNIPPET_SFX_PINCH 0x1E  // SFX_1E_HITTING_AN_ENEMY_2: pinching the player, 28000/32767, 1.0
#define SNIPPET_SFX_EGH 0x6E    // SFX_6E_VILE_EGH: flipped over, 26000/32767, 2.0
#define SNIPPET_SFX_DEATH 0x79  // SFX_79_TICKER_DEATH: defeated, 32750/32767 and 28000/32767, 1.0
// Banjo-Kazooie fades these out between 950 and 1900 units; scaled to DK64 units.
#define SFX_NEAR 240.0f
#define SFX_FAR 475.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f

// How it moves, in DK64 units per second, and how fast it turns (a share of the way a frame).
#define SNIPPET_WALK_SPEED 35.0f
#define SNIPPET_CHASE_SPEED 95.0f
#define SNIPPET_WALK_TURN 0.15f
#define SNIPPET_CHASE_TURN 0.25f
// How long it stands at each end of its path, in seconds (a random time between the two).
#define SNIPPET_PAUSE_MIN 0.6f
#define SNIPPET_PAUSE_MAX 2.0f
// Seeing the player and how far above or below; it gives up when the player gets this far off,
// or it gets this far from the middle of its path.
#define SNIPPET_SEE_RADIUS 170.0f
#define SNIPPET_SEE_HEIGHT 70.0f
#define SNIPPET_LOSE_RADIUS 280.0f
#define SNIPPET_LEASH 220.0f
// Pinching the player, and how long it backs off after; after losing the player, how long
// before it can see them again.
#define SNIPPET_CONTACT_RADIUS 26.0f
#define SNIPPET_CONTACT_HEIGHT 35.0f
#define SNIPPET_BACK_OFF_SECONDS 3.0f
#define SNIPPET_CALM_SECONDS 2.0f
// The player's attacks reach this far (the Simian Slam a little further), within this height;
// punches only hit what the player faces (within this many 12-bit angle units either side).
#define ATTACK_REACH 46.0f
#define SLAM_REACH 62.0f
#define ATTACK_HEIGHT 50.0f
#define PUNCH_ARC 0x300
// Its shell, for the player's shots.
#define SNIPPET_SHOT_RADIUS 24.0f
#define SNIPPET_SHOT_HEIGHT 30.0f
// Following the floor: a step up or down of more than these blocks the way. Placing a Snippet,
// the floor is looked for from this far above its spot and only trusted this close to it.
#define STEP_UP 20.0f
#define STEP_DOWN 30.0f
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f

// DK64 player states (Actor control_state) that hit a Snippet.
#define STATE_SIMIAN_SLAM 0x1C
#define STATE_PUNCH 0x26
#define STATE_PUNCH_2 0x27
#define STATE_PUNCH_3 0x28
#define STATE_RUNNING_ATTACK 0x29
#define STATE_AERIAL_ATTACK 0x2B
#define STATE_AERIAL_ATTACK_DIDDY 0x2E
#define STATE_AERIAL_ATTACK_CHUNKY 0x2F
// DK64's player action for a touch from an attacking enemy (see jiggy_grublins.c).
#define ACTION_ENEMY_HIT 0xE
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it): Snippets wait it out.
#define CUTSCENE_LOCK 0x10

// Where they are, map by map. Gloomy Galleon: two on the open floor by the entrance (between
// its banana trails, clear of Blubber's pacing in jiggy_blubber.c), and one on the lighthouse's
// island, east of its door. Each keeps to a box round its path.
typedef struct {
    Maps map;
    f32 ax, y, az; // One end of its path (y is the floor, if the floor isn't found).
    f32 bx, bz;    // The other.
} SnippetPath;
static const SnippetPath snippet_paths[] = {
    {MAP_GALLEON, 2300.0f, 1610.0f, 3080.0f, 2440.0f, 3110.0f},
    {MAP_GALLEON, 1960.0f, 1605.0f, 3060.0f, 2040.0f, 3120.0f},
    {MAP_GALLEON, 1990.0f, 1452.0f, 600.0f, 2100.0f, 620.0f},
};
#define SNIPPET_MAX 3
// How far a Snippet strays round its path (chasing, it never leaves this box).
#define SNIPPET_BOX_MARGIN 120.0f
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 D_global_asm_8076A0B1;
// The actors that update this frame (as jiggy_banjo_eggs.c reads them).
typedef struct {
    Actor *actor;
    s32 unk4;
} SnippetActiveActor;
extern SnippetActiveActor D_global_asm_807FB930[];
extern u16 D_global_asm_807FBB34; // How many.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y, leaving it as it was if there's none.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
// Whether the player can be hurt now (not invulnerable, not already knocked down).
s32 func_global_asm_806EB400(Actor *player);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
s32 jiggy_banjo_air_attack(Actor *player);

typedef enum {
    SNIPPETS_UNLOADED,
    SNIPPETS_READY,
    SNIPPETS_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model didn't convert.
} SnippetsLoadState;

typedef enum {
    SNIPPET_PAUSED,      // Standing at one end of its path.
    SNIPPET_WALKING,     // To the other end.
    SNIPPET_CHASING,
    SNIPPET_BACKING_OFF, // After pinching the player, or losing them: back to its path.
    SNIPPET_FLIPPING,    // Over onto its back.
    SNIPPET_ON_BACK,
    SNIPPET_RIGHTING,    // Back onto its feet.
    SNIPPET_DYING,
    SNIPPET_GONE,
} SnippetAction;

typedef struct {
    const SnippetPath *path;
    f32 pos[3];
    f32 yaw; // 12-bit angle units, as a float so the turn eases.
    SnippetAction action;
    s32 animation;
    f32 progress;   // 0 to 1 through the animation.
    f32 timer;      // Seconds left in a pause, on its back, or before it can see the player again.
    s32 to_b;       // Walking to its path's b end.
    s32 floor_found; // The floor was found under it, so it follows it.
    s32 hit_held;   // The player's attack that flipped it is still going: it doesn't count again.
} Snippet;

static u8 snippet_block[SNIPPET_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx snippet_gfx[SNIPPET_MAX][2][SNIPPET_GFX_MAX];
static Mtx snippet_mtx[SNIPPET_MAX][2][SNIPPET_MTX_MAX];
static JiggyBkPose snippet_pose;
static SnippetsLoadState snippets_state = SNIPPETS_UNLOADED;

static s32 snippets_last_map = -1;
static u32 snippets_last_timer;
static s32 snippets_here; // This map has Snippets.
static s32 snippets_placed;
static Snippet snippets[SNIPPET_MAX];
static u32 snippets_random_state = 0x534E4950;

static f32 snippets_random(void) {
    snippets_random_state = snippets_random_state * 1664525 + 1013904223;
    return (snippets_random_state >> 8) * (1.0f / 16777216.0f);
}

static void snippets_load(void) {
    s32 size;
    s32 i;

    snippets_state = SNIPPETS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(SNIPPET_MODEL, snippet_block, sizeof(snippet_block));
    if (size <= 0 || size > (s32)sizeof(snippet_block)) {
        return;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        snippet_pose.selectors[i] = 0;
    }
    snippet_pose.selectors[1] = 1;
    snippets_state = SNIPPETS_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// A Banjo-Kazooie sound from where the Snippet is, fading out with distance from the player.
static void snippet_sfx(Snippet *s, Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static void snippet_play(Snippet *s, s32 animation) {
    if (s->animation != animation) {
        s->animation = animation;
        s->progress = 0.0f;
    }
}

// True when `at` is passed going from `from` to `to` through an animation.
static s32 snippet_passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

// Advances a looping animation; walking, with its steps at 0.4 and 0.9 (__chCrab_walkSfxUpdate).
static void snippet_step_animation(Snippet *s, Actor *player, f32 seconds, f32 loop_seconds, s32 steps) {
    f32 before = s->progress;

    s->progress += seconds / loop_seconds;
    if (steps && snippet_passed(before, s->progress, 0.4f)) {
        snippet_sfx(s, player, SNIPPET_SFX_STEP, 15000.0f / 32767.0f, 0.85f);
    }
    if (steps && snippet_passed(before, s->progress, 0.9f)) {
        snippet_sfx(s, player, SNIPPET_SFX_STEP, 15000.0f / 32767.0f, 1.15f);
    }
    while (s->progress >= 1.0f) {
        s->progress -= 1.0f;
    }
}

// Turns a share of the way to face (x, z), over `frames` frames.
static void snippet_turn_to(Snippet *s, f32 x, f32 z, f32 rate, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, z, s->pos[0], s->pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - s->yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        s->yaw += turn * rate;
        if (s->yaw < 0.0f) {
            s->yaw += 4096.0f;
        } else if (s->yaw >= 4096.0f) {
            s->yaw -= 4096.0f;
        }
    }
}

// Moves up to `step` toward (x, z), over the floor; a wall of a step, a drop or the edge of its
// box stops it. Returns TRUE once it's there.
static s32 snippet_move_to(Snippet *s, f32 x, f32 z, f32 step) {
    const SnippetPath *p = s->path;
    f32 distance = distance_xz(x, z, s->pos[0], s->pos[2]);
    f32 min_x = (p->ax < p->bx ? p->ax : p->bx) - SNIPPET_BOX_MARGIN;
    f32 max_x = (p->ax > p->bx ? p->ax : p->bx) + SNIPPET_BOX_MARGIN;
    f32 min_z = (p->az < p->bz ? p->az : p->bz) - SNIPPET_BOX_MARGIN;
    f32 max_z = (p->az > p->bz ? p->az : p->bz) + SNIPPET_BOX_MARGIN;
    f32 nx, nz;
    f32 y = s->pos[1];

    if (distance <= step || distance < 1.0f) {
        nx = x;
        nz = z;
    } else {
        nx = s->pos[0] + (x - s->pos[0]) * step / distance;
        nz = s->pos[2] + (z - s->pos[2]) * step / distance;
    }
    if (nx < min_x || nx > max_x || nz < min_z || nz > max_z) {
        return FALSE;
    }
    if (s->floor_found) {
        f32 probe = s->pos[1] + STEP_UP;
        f32 floor = probe;

        func_global_asm_80667110(nx, nz, &floor);
        if (floor == probe || floor > s->pos[1] + STEP_UP || floor < s->pos[1] - STEP_DOWN) {
            return FALSE;
        }
        y = floor;
    }
    s->pos[0] = nx;
    s->pos[1] = y;
    s->pos[2] = nz;
    return distance <= step;
}

// Stands each Snippet over the floor at the start of its path (or at the path's height if no
// floor is found near: it then stays at that height).
static void snippets_place(void) {
    u32 i;
    u32 n = 0;

    for (i = 0; i < SNIPPET_MAX; i++) {
        snippets[i].action = SNIPPET_GONE;
    }
    for (i = 0; i < ARRAY_COUNT_OF(snippet_paths) && n < SNIPPET_MAX; i++) {
        Snippet *s;
        f32 probe;
        f32 floor;

        if (snippet_paths[i].map != current_map) {
            continue;
        }
        s = &snippets[n++];
        s->path = &snippet_paths[i];
        s->pos[0] = s->path->ax;
        s->pos[1] = s->path->y;
        s->pos[2] = s->path->az;
        probe = s->path->y + FLOOR_PROBE;
        floor = probe;
        func_global_asm_80667110(s->pos[0], s->pos[2], &floor);
        s->floor_found = floor != probe && floor - s->path->y < FLOOR_MAX_STEP && s->path->y - floor < FLOOR_MAX_STEP;
        if (s->floor_found) {
            s->pos[1] = floor;
        }
        s->yaw = func_global_asm_80665DE0(s->path->bx, s->path->bz, s->pos[0], s->pos[2]) & 0xFFF;
        s->action = SNIPPET_PAUSED;
        s->animation = SNIPPET_ANIM_IDLE;
        s->progress = snippets_random();
        s->timer = SNIPPET_PAUSE_MIN + snippets_random() * (SNIPPET_PAUSE_MAX - SNIPPET_PAUSE_MIN);
        s->to_b = TRUE;
        s->hit_held = FALSE;
    }
    snippets_placed = TRUE;
}

// Whether the player is where a Snippet can see them.
static s32 snippet_sees(Snippet *s, Actor *player, f32 radius) {
    f32 dy = player->y_position - s->pos[1];

    return distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position) <= radius &&
           dy <= SNIPPET_SEE_HEIGHT && dy >= -SNIPPET_SEE_HEIGHT;
}

// Whether the player's attack hits this Snippet this frame (as jiggy_grublins.c checks).
static s32 snippet_attacked(Snippet *s, Actor *player) {
    f32 distance = distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - s->pos[1];
    s32 to_snippet;
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
            if (distance <= SNIPPET_CONTACT_RADIUS) {
                return TRUE;
            }
            to_snippet = func_global_asm_80665DE0(s->pos[0], s->pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_snippet - (player->y_rotation & 0xFFF)) & 0xFFF;
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

// Whether one of the player's own shots (a gun's projectile, or one of Banjo's eggs) is in it.
static s32 snippet_shot(Snippet *s, Actor *player) {
    s32 i;

    for (i = 0; i < D_global_asm_807FBB34; i++) {
        Actor *shot = D_global_asm_807FB930[i].actor;
        f32 dy;

        if (shot == NULL || shot->unk11C != player) {
            continue;
        }
        switch (shot->unk58) {
            case ACTOR_PROJECTILE_COCONUTS:
            case ACTOR_PROJECTILE_PEANUT:
            case ACTOR_PROJECTILE_GRAPE:
            case ACTOR_PROJECTILE_FEATHER:
            case ACTOR_PROJECTILE_PINEAPPLE:
            case ACTOR_PROJECTILE_ORANGE:
                break;
            default:
                continue;
        }
        dy = shot->y_position - s->pos[1];
        if (dy >= -5.0f && dy <= SNIPPET_SHOT_HEIGHT &&
            distance_xz(s->pos[0], s->pos[2], shot->x_position, shot->z_position) <= SNIPPET_SHOT_RADIUS) {
            return TRUE;
        }
    }
    return FALSE;
}

// Pinches the player through DK64's own enemy hit, knocking them away from the Snippet
// (__chCrab_touch), and backs off.
static void snippet_pinch(Snippet *s, Actor *player) {
    // The knockback turns the player's heading round, so it's set toward the Snippet first.
    player->unkEE = func_global_asm_80665DE0(s->pos[0], s->pos[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
    snippet_sfx(s, player, SNIPPET_SFX_PINCH, 28000.0f / 32767.0f, 1.0f);
    s->action = SNIPPET_BACKING_OFF;
    s->timer = SNIPPET_BACK_OFF_SECONDS;
}

// A frame of one Snippet's behavior (chCrab_update), `frames` game frames on.
static void snippet_update(Snippet *s, Actor *player, u32 frames) {
    const SnippetPath *p = s->path;
    f32 seconds = frames / GAME_FPS;
    f32 mid_x = (p->ax + p->bx) * 0.5f;
    f32 mid_z = (p->az + p->bz) * 0.5f;
    f32 target_x = s->to_b ? p->bx : p->ax;
    f32 target_z = s->to_b ? p->bz : p->az;
    s32 upright = s->action == SNIPPET_PAUSED || s->action == SNIPPET_WALKING || s->action == SNIPPET_CHASING ||
                  s->action == SNIPPET_BACKING_OFF;
    s32 hit;

    if (frames == 0) {
        return;
    }
    // The player's attacks come first: a Snippet hit as it pinches doesn't hurt them. An attack
    // that's still going after flipping it doesn't count again.
    hit = s->action != SNIPPET_DYING && s->action != SNIPPET_GONE && (snippet_attacked(s, player) || snippet_shot(s, player));
    if (!hit) {
        s->hit_held = FALSE;
    } else if (!s->hit_held) {
        s->hit_held = TRUE;
        if (upright || s->action == SNIPPET_RIGHTING) {
            // Over onto its back (__chCrab_ow).
            s->action = SNIPPET_FLIPPING;
            snippet_play(s, SNIPPET_ANIM_FLIP_OVER);
            s->progress = 0.0f;
            snippet_sfx(s, player, SNIPPET_SFX_EGH, 26000.0f / 32767.0f, 2.0f);
            return;
        }
        if (s->action == SNIPPET_ON_BACK || s->action == SNIPPET_FLIPPING) {
            // Defeated (__chCrab_die).
            s->action = SNIPPET_DYING;
            s->progress = 0.0f;
            snippet_sfx(s, player, SNIPPET_SFX_DEATH, 32750.0f / 32767.0f, 1.0f);
            snippet_sfx(s, player, SNIPPET_SFX_DEATH, 28000.0f / 32767.0f, 1.0f);
            return;
        }
    }
    if (upright && s->action != SNIPPET_BACKING_OFF &&
        distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position) <= SNIPPET_CONTACT_RADIUS &&
        player->y_position - s->pos[1] <= SNIPPET_CONTACT_HEIGHT && s->pos[1] - player->y_position <= SNIPPET_CONTACT_HEIGHT &&
        func_global_asm_806EB400(player)) {
        snippet_pinch(s, player);
    }

    switch (s->action) {
        case SNIPPET_PAUSED:
        case SNIPPET_WALKING:
            if (snippet_sees(s, player, SNIPPET_SEE_RADIUS)) {
                s->action = SNIPPET_CHASING;
                snippet_play(s, SNIPPET_ANIM_WALK);
                break;
            }
            if (s->action == SNIPPET_PAUSED) {
                snippet_play(s, SNIPPET_ANIM_IDLE);
                snippet_step_animation(s, player, seconds, SNIPPET_IDLE_SECONDS, FALSE);
                s->timer -= seconds;
                if (s->timer <= 0.0f) {
                    s->to_b = !s->to_b;
                    s->action = SNIPPET_WALKING;
                    snippet_play(s, SNIPPET_ANIM_WALK);
                }
                break;
            }
            snippet_turn_to(s, target_x, target_z, SNIPPET_WALK_TURN, frames);
            snippet_step_animation(s, player, seconds, SNIPPET_WALK_SECONDS, TRUE);
            if (snippet_move_to(s, target_x, target_z, SNIPPET_WALK_SPEED * seconds)) {
                s->action = SNIPPET_PAUSED;
                s->timer = SNIPPET_PAUSE_MIN + snippets_random() * (SNIPPET_PAUSE_MAX - SNIPPET_PAUSE_MIN);
            }
            break;
        case SNIPPET_CHASING:
            if (!snippet_sees(s, player, SNIPPET_LOSE_RADIUS) || distance_xz(s->pos[0], s->pos[2], mid_x, mid_z) > SNIPPET_LEASH) {
                s->action = SNIPPET_BACKING_OFF;
                s->timer = SNIPPET_CALM_SECONDS;
                break;
            }
            snippet_play(s, SNIPPET_ANIM_WALK);
            snippet_turn_to(s, player->x_position, player->z_position, SNIPPET_CHASE_TURN, frames);
            snippet_step_animation(s, player, seconds, SNIPPET_CHASE_SECONDS, TRUE);
            // It stops short of the player rather than walking through them.
            if (distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position) > SNIPPET_CONTACT_RADIUS * 0.6f) {
                snippet_move_to(s, player->x_position, player->z_position, SNIPPET_CHASE_SPEED * seconds);
            }
            break;
        case SNIPPET_BACKING_OFF: {
            // Back to whichever end of its path is further from the player.
            f32 to_a = distance_xz(p->ax, p->az, player->x_position, player->z_position);
            f32 to_b = distance_xz(p->bx, p->bz, player->x_position, player->z_position);

            s->to_b = to_b > to_a;
            target_x = s->to_b ? p->bx : p->ax;
            target_z = s->to_b ? p->bz : p->az;
            s->timer -= seconds;
            if (distance_xz(s->pos[0], s->pos[2], target_x, target_z) > 1.0f) {
                snippet_play(s, SNIPPET_ANIM_WALK);
                snippet_turn_to(s, target_x, target_z, SNIPPET_WALK_TURN, frames);
                snippet_step_animation(s, player, seconds, SNIPPET_WALK_SECONDS * 0.6f, TRUE);
                snippet_move_to(s, target_x, target_z, SNIPPET_WALK_SPEED * 1.5f * seconds);
            } else {
                snippet_play(s, SNIPPET_ANIM_IDLE);
                snippet_step_animation(s, player, seconds, SNIPPET_IDLE_SECONDS, FALSE);
            }
            if (s->timer <= 0.0f) {
                s->action = SNIPPET_PAUSED;
                s->timer = SNIPPET_PAUSE_MIN;
            }
            break;
        }
        case SNIPPET_FLIPPING:
            s->progress += seconds / SNIPPET_FLIP_OVER_SECONDS;
            if (s->progress >= 1.0f) {
                s->action = SNIPPET_ON_BACK;
                snippet_play(s, SNIPPET_ANIM_BACK);
                s->timer = SNIPPET_ON_BACK_SECONDS;
            }
            break;
        case SNIPPET_ON_BACK:
            snippet_step_animation(s, player, seconds, SNIPPET_BACK_SECONDS, FALSE);
            s->timer -= seconds;
            if (s->timer <= 0.0f) {
                s->action = SNIPPET_RIGHTING;
                snippet_play(s, SNIPPET_ANIM_FLIP_BACK);
            }
            break;
        case SNIPPET_RIGHTING:
            s->progress += seconds / SNIPPET_FLIP_BACK_SECONDS;
            if (s->progress >= 1.0f) {
                s->action = SNIPPET_BACKING_OFF;
                s->timer = SNIPPET_CALM_SECONDS;
                snippet_play(s, SNIPPET_ANIM_IDLE);
            }
            break;
        case SNIPPET_DYING:
            s->progress += seconds / SNIPPET_DIE_SECONDS;
            if (s->progress >= 1.0f) {
                s->action = SNIPPET_GONE;
            }
            break;
        default:
            break;
    }
}

static Gfx *snippet_draw(Gfx *dl, Snippet *s, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 size = BK_TO_DK64;
    f32 progress = s->progress;
    s32 animation = s->animation;

    // Dying, it shrinks away on its back.
    if (s->action == SNIPPET_DYING) {
        size *= 1.0f - (progress < 1.0f ? progress : 1.0f);
        animation = SNIPPET_ANIM_BACK;
        progress = 0.0f;
        if (size <= 0.001f) {
            return dl;
        }
    }
    guScaleF(scale, size, size, size);
    guRotateF(rotate, s->yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, snippet_pose.world);
    snippet_pose.world[3][0] = s->pos[0];
    snippet_pose.world[3][1] = s->pos[1];
    snippet_pose.world[3][2] = s->pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    snippet_pose.model = SNIPPET_MODEL;
    snippet_pose.block = snippet_block;
    snippet_pose.animation = animation;
    snippet_pose.progress = progress;
    snippet_pose.gfx = gfx;
    snippet_pose.gfx_capacity = SNIPPET_GFX_MAX;
    snippet_pose.mtx = mtx;
    snippet_pose.mtx_capacity = SNIPPET_MTX_MAX;
    if (jiggy_bk_model_pose(&snippet_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws the Snippets once a frame, with the player.
Gfx *jiggy_snippets_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over): every Snippet is back.
    if ((s32)current_map != snippets_last_map || object_timer < snippets_last_timer) {
        snippets_last_map = current_map;
        snippets_placed = FALSE;
        snippets_here = FALSE;
        for (i = 0; i < ARRAY_COUNT_OF(snippet_paths); i++) {
            if (snippet_paths[i].map == current_map) {
                snippets_here = TRUE;
            }
        }
    }
    if (!snippets_here) {
        snippets_last_timer = object_timer;
        return dl;
    }
    if (snippets_state == SNIPPETS_UNLOADED) {
        snippets_load();
    }
    if (snippets_state != SNIPPETS_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!snippets_placed) {
        snippets_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        snippets_place();
    }
    frames = object_timer - snippets_last_timer;
    snippets_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // They wait while a cutscene holds the player (a character's talk, for one).
    if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        frames = 0;
    }
    for (i = 0; i < SNIPPET_MAX; i++) {
        Snippet *s = &snippets[i];

        if (s->action == SNIPPET_GONE) {
            continue;
        }
        snippet_update(s, actor, frames);
        if (s->action == SNIPPET_GONE) {
            continue;
        }
        dl = snippet_draw(dl, s, snippet_gfx[i][buffer], snippet_mtx[i][buffer]);
    }
    return dl;
}
