// Zubbas: Click Clock Wood's bees, read from the player's Banjo-Kazooie ROM, as enemies in Fungi
// Forest's apple area, where the two worlds have collided.
//
// Like chZubba (src/CCW/code_950.c), each Zubba (ASSET_446_MODEL_ZUBBA) flies with its flying
// animation, buzzing (SFX_3FA, the loop chZubba's sound source plays), here up and down a short
// beat over the ground round the apple's mound; when it sees the player it calls out
// (SFX_102_ZUBBA_TALKING) and flies at them. Flying into the player hurts them once, through
// DK64's own path for an enemy's hit (setAction(0xE), as jiggy_grublins.c does), with chZubba's
// hit sounds (SFX_1F and SFX_3FC), and it backs off for a while.
//
// The player's attacks defeat it, as they do a Grublin (jiggy_grublins.c: punches, the rolling
// and aerial attacks, the Simian Slam, and Banjo's Rat-a-tat Rap and Beak Buster): it plays
// chZubba's dying animation and sounds (SFX_1F, and 0x66 at 1.6-1.7) falling to the ground, then
// its landing animation with its splat (SFX_A_BANJO_LANDING_05), and is gone until the map is
// loaded again; nothing is saved and it drops nothing.
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c). Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "jiggy.h"

#define ZUBBA_MODEL 0x446 // ASSET_446_MODEL_ZUBBA
// The Zubba converts to 19656 bytes, with 52 bones and 15 pose commands.
#define ZUBBA_BLOCK_MAX 0x5000
#define ZUBBA_GFX_MAX 32
#define ZUBBA_MTX_MAX 56

// Banjo-Kazooie animations (chZubba_setState's) and how long each plays, in seconds.
#define ZUBBA_ANIM_FLY 0x16F  // ASSET_16F_ANIM_ZUBBA_FLY_MOVE
#define ZUBBA_ANIM_HOVER 0x170 // ASSET_170_ANIM_ZUBBA_FLY_IDLE: attacking
#define ZUBBA_ANIM_DIE 0x171  // ASSET_171_ANIM_ZUBBA_DIE
#define ZUBBA_ANIM_LAND 0x172 // ASSET_172_ANIM_ZUBBA_LAND
#define ZUBBA_FLY_SECONDS 0.65f
#define ZUBBA_DIE_SECONDS 0.6f
#define ZUBBA_LAND_SECONDS 1.0f

// Banjo-Kazooie sounds (sfx_e) and how chZubba plays them.
#define ZUBBA_SFX_BUZZ 0x3FA  // SFX_3FA_HONEYCOMB_TALKING: its sound source's buzz, at 0.9
#define ZUBBA_SFX_TALK 0x102  // SFX_102_ZUBBA_TALKING: seeing the player
#define ZUBBA_SFX_HIT 0x1F    // SFX_1F_HITTING_AN_ENEMY_3: hitting the player, or hit: 32200/32767, 1.2
#define ZUBBA_SFX_OUCH 0x3FC  // SFX_3FC_ZUBBA_UNK: after hitting the player, 32000/32767, 0.95-1.1
#define ZUBBA_SFX_DIE 0x66    // Dying: 32000/32767, 1.6-1.7
#define ZUBBA_SFX_SPLAT 0x0A  // SFX_A_BANJO_LANDING_05: landing, 18000/32767, 0.85-0.95
// Banjo-Kazooie fades these out between 500 and 3000 units; scaled to DK64 units.
#define SFX_NEAR 125.0f
#define SFX_FAR 750.0f
// The buzz is a loop the sound player cuts at 3 seconds: played again this often while a Zubba
// is this close.
#define BUZZ_SECONDS 2.8f
#define BUZZ_NEAR 60.0f
#define BUZZ_FAR 260.0f

// Banjo-Kazooie units to DK64 units: smaller than jiggy_banjo3d.c's 0.25 (a Zubba's wings span
// 264 units, a Kong's width over again), about 48 units across.
#define ZUBBA_SCALE 0.18f
#define GAME_FPS 30.0f

// How it flies, in DK64 units per second, how high over the floor, and its bob.
#define ZUBBA_PATROL_SPEED 50.0f
#define ZUBBA_CHASE_SPEED 100.0f
#define ZUBBA_TURN 0.15f
#define ZUBBA_CHASE_TURN 0.3f
#define ZUBBA_HOVER 22.0f
#define ZUBBA_CHASE_HEIGHT 12.0f // Over the player's feet.
#define ZUBBA_BOB 4.0f
#define ZUBBA_BOB_SECONDS 1.3f
#define ZUBBA_CLIMB 60.0f // Units a second it rises or sinks toward its height.
#define ZUBBA_FALL 120.0f // Units a second it falls, dying.
// Seeing the player, losing them, and how far it strays from the middle of its beat.
#define ZUBBA_SEE_RADIUS 180.0f
#define ZUBBA_SEE_HEIGHT 90.0f
#define ZUBBA_LOSE_RADIUS 300.0f
#define ZUBBA_LEASH 280.0f
#define ZUBBA_PAUSE_SECONDS 1.0f
// Flying into the player, and how long it backs off after; after losing them, how long before
// it can see them again.
#define ZUBBA_CONTACT_RADIUS 24.0f
#define ZUBBA_CONTACT_HEIGHT 40.0f
#define ZUBBA_BACK_OFF_SECONDS 3.0f
#define ZUBBA_CALM_SECONDS 2.0f
// The player's attacks reach this far (the Simian Slam a little further), within this height;
// punches only hit what the player faces (as jiggy_grublins.c).
#define ATTACK_REACH 48.0f
#define SLAM_REACH 64.0f
#define ATTACK_HEIGHT 55.0f
#define PUNCH_ARC 0x300
#define FLOOR_PROBE 60.0f
#define FLOOR_MAX_STEP 80.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f

#define STATE_SIMIAN_SLAM 0x1C
#define STATE_PUNCH 0x26
#define STATE_PUNCH_2 0x27
#define STATE_PUNCH_3 0x28
#define STATE_RUNNING_ATTACK 0x29
#define STATE_AERIAL_ATTACK 0x2B
#define STATE_AERIAL_ATTACK_DIDDY 0x2E
#define STATE_AERIAL_ATTACK_CHUNKY 0x2F
#define ACTION_ENEMY_HIT 0xE
#define CUTSCENE_LOCK 0x10

// Where they are, map by map. Fungi Forest's apple area: three beats over the open ground round
// the apple's mound (DK64's floor, 188 to 202), inside the area's edges.
typedef struct {
    f32 ax, az;
    f32 bx, bz;
} ZubbaPath;
static const ZubbaPath zubba_paths_fungi[] = {
    {2800.0f, 600.0f, 2850.0f, 850.0f}, // West of the mound.
    {3400.0f, 500.0f, 3450.0f, 800.0f}, // East of the mound.
    {3100.0f, 300.0f, 3300.0f, 350.0f}, // North of the mound.
};
#define ZUBBA_MAX 3
typedef struct {
    Maps map;
    f32 floor_y;                    // The area's floor height, if the floor isn't found.
    f32 min_x, max_x, min_z, max_z; // They never leave this.
    const ZubbaPath *paths;
    u32 count;
} ZubbaMap;
static const ZubbaMap zubba_maps[] = {
    {MAP_FUNGI, 195.0f, 2680.0f, 3620.0f, 200.0f, 1150.0f, zubba_paths_fungi,
     sizeof(zubba_paths_fungi) / sizeof(zubba_paths_fungi[0])},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 D_global_asm_8076A0B1;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z);
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
s32 func_global_asm_806EB400(Actor *player);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
s32 jiggy_banjo_air_attack(Actor *player);

typedef enum {
    ZUBBAS_UNLOADED,
    ZUBBAS_READY,
    ZUBBAS_UNAVAILABLE,
} ZubbasLoadState;

typedef enum {
    ZUBBA_PATROL,
    ZUBBA_PAUSED,
    ZUBBA_CHASING,
    ZUBBA_BACKING_OFF,
    ZUBBA_DYING,
    ZUBBA_LANDED,
    ZUBBA_GONE,
} ZubbaAction;

typedef struct {
    const ZubbaPath *path;
    f32 pos[3];
    f32 floor; // The floor under it.
    f32 yaw;
    ZubbaAction action;
    s32 animation;
    f32 progress;
    f32 bob;
    f32 timer;
    s32 to_b;
} Zubba;

static u8 zubba_block[ZUBBA_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx zubba_gfx[ZUBBA_MAX][2][ZUBBA_GFX_MAX];
static Mtx zubba_mtx[ZUBBA_MAX][2][ZUBBA_MTX_MAX];
static JiggyBkPose zubba_pose;
static ZubbasLoadState zubbas_state = ZUBBAS_UNLOADED;
static s32 zubbas_last_map = -1;
static u32 zubbas_last_timer;
static const ZubbaMap *zubbas_map;
static s32 zubbas_placed;
static Zubba zubbas[ZUBBA_MAX];
static s32 buzz_handle;
static f32 buzz_timer;
static u32 zubbas_random_state = 0x5A554242;

static f32 zubbas_random(void) {
    zubbas_random_state = zubbas_random_state * 1664525 + 1013904223;
    return (zubbas_random_state >> 8) * (1.0f / 16777216.0f);
}

static void zubbas_load(void) {
    s32 size;
    s32 i;

    zubbas_state = ZUBBAS_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(ZUBBA_MODEL, zubba_block, sizeof(zubba_block));
    if (size <= 0 || size > (s32)sizeof(zubba_block)) {
        return;
    }
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        zubba_pose.selectors[i] = 0;
    }
    zubba_pose.selectors[1] = 1;
    zubbas_state = ZUBBAS_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

static void zubba_sfx(Zubba *z, Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(z->pos[0], z->pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static void zubba_play(Zubba *z, s32 animation) {
    if (z->animation != animation) {
        z->animation = animation;
        z->progress = 0.0f;
    }
}

static s32 zubba_passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

static void zubba_turn_to(Zubba *z, f32 x, f32 zz, f32 rate, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, zz, z->pos[0], z->pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - z->yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        z->yaw += turn * rate;
        if (z->yaw < 0.0f) {
            z->yaw += 4096.0f;
        } else if (z->yaw >= 4096.0f) {
            z->yaw -= 4096.0f;
        }
    }
}

// The floor under (x, z) near `y`, or `y` if there's none near.
static f32 zubba_floor_at(f32 x, f32 z, f32 y) {
    f32 probe = y + FLOOR_PROBE;
    f32 floor = probe;

    func_global_asm_80667110(x, z, &floor);
    if (floor == probe || floor - y > FLOOR_MAX_STEP || y - floor > FLOOR_MAX_STEP) {
        return y;
    }
    return floor;
}

// Flies up to `step` toward (x, z), staying in the area. Returns TRUE once it's there.
static s32 zubba_move_to(Zubba *z, f32 x, f32 zz, f32 step) {
    f32 distance = distance_xz(x, zz, z->pos[0], z->pos[2]);
    f32 nx, nz;

    if (distance <= step || distance < 1.0f) {
        nx = x;
        nz = zz;
    } else {
        nx = z->pos[0] + (x - z->pos[0]) * step / distance;
        nz = z->pos[2] + (zz - z->pos[2]) * step / distance;
    }
    if (nx < zubbas_map->min_x || nx > zubbas_map->max_x || nz < zubbas_map->min_z || nz > zubbas_map->max_z) {
        return FALSE;
    }
    z->pos[0] = nx;
    z->pos[2] = nz;
    z->floor = zubba_floor_at(nx, nz, z->floor);
    return distance <= step;
}

// Rises or sinks toward `height`.
static void zubba_climb(Zubba *z, f32 height, f32 seconds) {
    f32 step = ZUBBA_CLIMB * seconds;

    if (z->pos[1] < height - step) {
        z->pos[1] += step;
    } else if (z->pos[1] > height + step) {
        z->pos[1] -= step;
    } else {
        z->pos[1] = height;
    }
}

static void zubbas_place(void) {
    u32 i;

    for (i = 0; i < ZUBBA_MAX; i++) {
        Zubba *z = &zubbas[i];

        if (i >= zubbas_map->count) {
            z->action = ZUBBA_GONE;
            continue;
        }
        z->path = &zubbas_map->paths[i];
        z->pos[0] = z->path->ax;
        z->pos[2] = z->path->az;
        z->floor = zubba_floor_at(z->pos[0], z->pos[2], zubbas_map->floor_y);
        z->pos[1] = z->floor + ZUBBA_HOVER;
        z->yaw = func_global_asm_80665DE0(z->path->bx, z->path->bz, z->pos[0], z->pos[2]) & 0xFFF;
        z->action = ZUBBA_PATROL;
        z->animation = ZUBBA_ANIM_FLY;
        z->progress = zubbas_random();
        z->bob = zubbas_random();
        z->timer = 0.0f;
        z->to_b = TRUE;
    }
    zubbas_placed = TRUE;
}

static s32 zubba_sees(Zubba *z, Actor *player, f32 radius) {
    f32 dy = player->y_position - z->floor;

    return distance_xz(z->pos[0], z->pos[2], player->x_position, player->z_position) <= radius &&
           dy <= ZUBBA_SEE_HEIGHT && dy >= -ZUBBA_SEE_HEIGHT;
}

static s32 zubba_attacked(Zubba *z, Actor *player) {
    f32 distance = distance_xz(z->pos[0], z->pos[2], player->x_position, player->z_position);
    f32 dy = z->pos[1] - player->y_position;
    s32 to_zubba;
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
            if (distance <= ZUBBA_CONTACT_RADIUS) {
                return TRUE;
            }
            to_zubba = func_global_asm_80665DE0(z->pos[0], z->pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_zubba - (player->y_rotation & 0xFFF)) & 0xFFF;
            return off <= PUNCH_ARC || off >= 0x1000 - PUNCH_ARC;
        case STATE_RUNNING_ATTACK:
        case STATE_AERIAL_ATTACK:
        case STATE_AERIAL_ATTACK_DIDDY:
        case STATE_AERIAL_ATTACK_CHUNKY:
            return distance <= ATTACK_REACH;
        default:
            return jiggy_banjo_air_attack(player) && distance <= ATTACK_REACH;
    }
}

static void zubba_die(Zubba *z, Actor *player) {
    z->action = ZUBBA_DYING;
    zubba_play(z, ZUBBA_ANIM_DIE);
    zubba_sfx(z, player, ZUBBA_SFX_HIT, 32200.0f / 32767.0f, 1.2f);
    zubba_sfx(z, player, ZUBBA_SFX_DIE, 32000.0f / 32767.0f, 1.6f + zubbas_random() * 0.1f);
}

static void zubba_hit_player(Zubba *z, Actor *player) {
    player->unkEE = func_global_asm_80665DE0(z->pos[0], z->pos[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
    zubba_sfx(z, player, ZUBBA_SFX_HIT, 32200.0f / 32767.0f, 1.2f);
    zubba_sfx(z, player, ZUBBA_SFX_OUCH, 32000.0f / 32767.0f, 0.95f + zubbas_random() * 0.15f);
    z->action = ZUBBA_BACKING_OFF;
    z->timer = ZUBBA_BACK_OFF_SECONDS;
}

static void zubba_update(Zubba *z, Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 mid_x = (z->path->ax + z->path->bx) * 0.5f;
    f32 mid_z = (z->path->az + z->path->bz) * 0.5f;
    f32 target_x = z->to_b ? z->path->bx : z->path->ax;
    f32 target_z = z->to_b ? z->path->bz : z->path->az;
    s32 live = z->action != ZUBBA_DYING && z->action != ZUBBA_LANDED && z->action != ZUBBA_GONE;
    f32 hover;

    if (frames == 0) {
        return;
    }
    if (live && zubba_attacked(z, player)) {
        zubba_die(z, player);
        return;
    }
    if (live && z->action != ZUBBA_BACKING_OFF &&
        distance_xz(z->pos[0], z->pos[2], player->x_position, player->z_position) <= ZUBBA_CONTACT_RADIUS &&
        z->pos[1] - player->y_position <= ZUBBA_CONTACT_HEIGHT && player->y_position - z->pos[1] <= ZUBBA_CONTACT_HEIGHT &&
        func_global_asm_806EB400(player)) {
        zubba_hit_player(z, player);
    }
    if (live) {
        z->bob += seconds / ZUBBA_BOB_SECONDS;
        while (z->bob >= 1.0f) {
            z->bob -= 1.0f;
        }
    }
    hover = z->floor + ZUBBA_HOVER + ZUBBA_BOB * func_global_asm_80612794((s16)(z->bob * 4096.0f));

    switch (z->action) {
        case ZUBBA_PATROL:
        case ZUBBA_PAUSED:
            zubba_play(z, ZUBBA_ANIM_FLY);
            z->progress += seconds / ZUBBA_FLY_SECONDS;
            while (z->progress >= 1.0f) {
                z->progress -= 1.0f;
            }
            zubba_climb(z, hover, seconds);
            if (zubba_sees(z, player, ZUBBA_SEE_RADIUS)) {
                z->action = ZUBBA_CHASING;
                zubba_sfx(z, player, ZUBBA_SFX_TALK, 20000.0f / 32767.0f, 1.0f);
                break;
            }
            if (z->action == ZUBBA_PAUSED) {
                z->timer -= seconds;
                if (z->timer <= 0.0f) {
                    z->to_b = !z->to_b;
                    z->action = ZUBBA_PATROL;
                }
                break;
            }
            zubba_turn_to(z, target_x, target_z, ZUBBA_TURN, frames);
            if (zubba_move_to(z, target_x, target_z, ZUBBA_PATROL_SPEED * seconds)) {
                z->action = ZUBBA_PAUSED;
                z->timer = ZUBBA_PAUSE_SECONDS;
            }
            break;
        case ZUBBA_CHASING:
            zubba_play(z, ZUBBA_ANIM_HOVER);
            z->progress += seconds / ZUBBA_FLY_SECONDS;
            while (z->progress >= 1.0f) {
                z->progress -= 1.0f;
            }
            if (!zubba_sees(z, player, ZUBBA_LOSE_RADIUS) || distance_xz(z->pos[0], z->pos[2], mid_x, mid_z) > ZUBBA_LEASH) {
                z->action = ZUBBA_BACKING_OFF;
                z->timer = ZUBBA_CALM_SECONDS;
                break;
            }
            zubba_turn_to(z, player->x_position, player->z_position, ZUBBA_CHASE_TURN, frames);
            zubba_climb(z, player->y_position + ZUBBA_CHASE_HEIGHT, seconds);
            if (distance_xz(z->pos[0], z->pos[2], player->x_position, player->z_position) > ZUBBA_CONTACT_RADIUS * 0.5f) {
                zubba_move_to(z, player->x_position, player->z_position, ZUBBA_CHASE_SPEED * seconds);
            }
            break;
        case ZUBBA_BACKING_OFF: {
            f32 to_a = distance_xz(z->path->ax, z->path->az, player->x_position, player->z_position);
            f32 to_b = distance_xz(z->path->bx, z->path->bz, player->x_position, player->z_position);

            zubba_play(z, ZUBBA_ANIM_FLY);
            z->progress += seconds / ZUBBA_FLY_SECONDS;
            while (z->progress >= 1.0f) {
                z->progress -= 1.0f;
            }
            z->to_b = to_b > to_a;
            target_x = z->to_b ? z->path->bx : z->path->ax;
            target_z = z->to_b ? z->path->bz : z->path->az;
            zubba_climb(z, hover, seconds);
            if (distance_xz(z->pos[0], z->pos[2], target_x, target_z) > 1.0f) {
                zubba_turn_to(z, target_x, target_z, ZUBBA_TURN, frames);
                zubba_move_to(z, target_x, target_z, ZUBBA_PATROL_SPEED * seconds);
            }
            z->timer -= seconds;
            if (z->timer <= 0.0f) {
                z->action = ZUBBA_PAUSED;
                z->timer = ZUBBA_PAUSE_SECONDS;
            }
            break;
        }
        case ZUBBA_DYING:
            z->progress += seconds / ZUBBA_DIE_SECONDS;
            if (z->progress > 1.0f) {
                z->progress = 1.0f;
            }
            z->pos[1] -= ZUBBA_FALL * seconds;
            if (z->pos[1] <= z->floor) {
                z->pos[1] = z->floor;
                z->action = ZUBBA_LANDED;
                zubba_play(z, ZUBBA_ANIM_LAND);
                zubba_sfx(z, player, ZUBBA_SFX_SPLAT, 18000.0f / 32767.0f, 0.85f + zubbas_random() * 0.1f);
            }
            break;
        case ZUBBA_LANDED: {
            f32 before = z->progress;

            z->progress += seconds / ZUBBA_LAND_SECONDS;
            if (zubba_passed(before, z->progress, 1.0f)) {
                z->action = ZUBBA_GONE;
            }
            break;
        }
        default:
            break;
    }
}

static Gfx *zubba_draw(Gfx *dl, Zubba *z, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 progress = z->progress;

    guScaleF(scale, ZUBBA_SCALE, ZUBBA_SCALE, ZUBBA_SCALE);
    guRotateF(rotate, z->yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, zubba_pose.world);
    zubba_pose.world[3][0] = z->pos[0];
    zubba_pose.world[3][1] = z->pos[1];
    zubba_pose.world[3][2] = z->pos[2];
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    zubba_pose.model = ZUBBA_MODEL;
    zubba_pose.block = zubba_block;
    zubba_pose.animation = z->animation;
    zubba_pose.progress = progress;
    zubba_pose.gfx = gfx;
    zubba_pose.gfx_capacity = ZUBBA_GFX_MAX;
    zubba_pose.mtx = mtx;
    zubba_pose.mtx_capacity = ZUBBA_MTX_MAX;
    if (jiggy_bk_model_pose(&zubba_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// The buzz of the nearest live Zubba, played again before the sound player cuts it.
static void zubbas_buzz(Actor *player, u32 frames) {
    f32 nearest = BUZZ_FAR;
    u32 i;

    for (i = 0; i < ZUBBA_MAX; i++) {
        Zubba *z = &zubbas[i];
        f32 distance;

        if (z->action == ZUBBA_GONE || z->action == ZUBBA_DYING || z->action == ZUBBA_LANDED) {
            continue;
        }
        distance = distance_xz(z->pos[0], z->pos[2], player->x_position, player->z_position);
        if (distance < nearest) {
            nearest = distance;
        }
    }
    buzz_timer -= frames / GAME_FPS;
    if (nearest >= BUZZ_FAR) {
        if (buzz_handle > 0) {
            jiggy_bk_sound_stop(buzz_handle);
            buzz_handle = 0;
        }
        buzz_timer = 0.0f;
        return;
    }
    if (buzz_handle > 0 && buzz_timer > 0.0f) {
        return;
    }
    if (buzz_handle > 0) {
        jiggy_bk_sound_stop(buzz_handle);
    }
    {
        f32 volume = 0.9f * 0.5f;

        if (nearest > BUZZ_NEAR) {
            volume *= (BUZZ_FAR - nearest) / (BUZZ_FAR - BUZZ_NEAR);
        }
        buzz_handle = jiggy_bk_sfx_play(ZUBBA_SFX_BUZZ, volume, 1.0f);
    }
    buzz_timer = BUZZ_SECONDS;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// the Zubbas once a frame, with the player.
Gfx *jiggy_zubbas_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != zubbas_last_map || object_timer < zubbas_last_timer) {
        zubbas_last_map = current_map;
        zubbas_placed = FALSE;
        zubbas_map = NULL;
        if (buzz_handle > 0) {
            jiggy_bk_sound_stop(buzz_handle);
            buzz_handle = 0;
        }
        for (i = 0; i < ARRAY_COUNT_OF(zubba_maps); i++) {
            if (zubba_maps[i].map == current_map) {
                zubbas_map = &zubba_maps[i];
            }
        }
    }
    if (zubbas_map == NULL) {
        zubbas_last_timer = object_timer;
        return dl;
    }
    if (zubbas_state == ZUBBAS_UNLOADED) {
        zubbas_load();
    }
    if (zubbas_state != ZUBBAS_READY) {
        return dl;
    }
    if (!zubbas_placed) {
        zubbas_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        zubbas_place();
    }
    frames = object_timer - zubbas_last_timer;
    zubbas_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        frames = 0;
    }
    for (i = 0; i < ZUBBA_MAX; i++) {
        Zubba *z = &zubbas[i];

        if (z->action == ZUBBA_GONE) {
            continue;
        }
        zubba_update(z, actor, frames);
        if (z->action == ZUBBA_GONE) {
            continue;
        }
        dl = zubba_draw(dl, z, zubba_gfx[i][buffer], zubba_mtx[i][buffer]);
    }
    if (frames > 0) {
        zubbas_buzz(actor, frames);
    }
    return dl;
}
