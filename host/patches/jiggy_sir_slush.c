// Sir Slush: Freezeezy Peak's snowball-throwing snowmen, read from the player's Banjo-Kazooie
// ROM, in Crystal Caves, where the two worlds have collided.
//
// In Banjo-Kazooie each Sir Slush (ASSET_377_MODEL_SIR_SLUSH, chSnowman in snowman.c) stands in
// the snow, turns to Banjo and, from 500 to 2750 units off, throws a snowball
// (ASSET_378_MODEL_SNOWBALL, snowball.c) with his attack (ASSET_109_ANIM_SIR_SLUSH_ATTACK: swooshes
// at 0.19, 0.28 and 0.37, a grunt at 0.15, the snowball in his hand from 0.45 and away at 0.58
// with SFX_8F_SNOWBALL_FLYING). One hit beats him: he bursts into snowballs (SFX_15, SFX_3EA and
// SFX_2F) and his hat (ASSET_379_MODEL_SIR_SLUSH_HAT, snowmanhat.c) flies off. Too far from the
// player he sinks into the snow (ASSET_220_ANIM_SIR_SLUSH_DIE) and comes back up when they return.
// Beating all of Freezeezy Peak's gets a Jiggy there (maSnowy).
//
// Here three of them slide about Crystal Caves' snowy shelves, each between two spots, as
// jiggy_grublins.c's Grublins walk theirs; with the player in range one stops, turns and throws.
// A snowball that reaches the player hurts them through DK64's own enemy hit (setAction(0xE), as
// jiggy_grublins.c and jiggy_conga.c do, when func_global_asm_806EB400 says they can be hurt). The
// player's attacks (as they hit a Grublin) or their shots (as they hit Conga) beat one. Beaten
// ones stay beaten (saved); beating the last brings a Golden Banana (drawn as a Jiggy) where he
// stood, with the Jiggy jingle (COMUSIC_3D): jiggy_rewards.c's CAVES_REWARD_SIR_SLUSH.
//
// Saved (jiggy_save_get/set): which are beaten, a bit each, under
// CAVES_SAVE_KEY(CAVES_CHARACTER_SIR_SLUSH, map); where the Golden Banana is, under
// CAVES_SAVE_KEY(SLUSH_SPOT_CHARACTER, map). Drawn right after the player's own model
// (func_global_asm_80630DCC in patches_draw.c). Without a Banjo-Kazooie ROM nothing is drawn and
// nothing happens.

#include "common_structs.h"
#include "jiggy.h"
#include "jiggy_caves.h"

#define SLUSH_MODEL 0x377    // ASSET_377_MODEL_SIR_SLUSH
#define SNOWBALL_MODEL 0x378 // ASSET_378_MODEL_SNOWBALL
#define HAT_MODEL 0x379      // ASSET_379_MODEL_SIR_SLUSH_HAT
// Sir Slush converts to about 22 KiB (25 bones, 85 pose commands); a snowball and the hat to
// under 2 KiB each (no bones, 6 commands). Each buffer has some room to spare, so a pose
// can't fail (and draw nothing) for want of it.
#define SLUSH_BLOCK_MAX 0x5C00
#define SLUSH_GFX_MAX 128
#define SLUSH_MTX_MAX 32
#define SNOWBALL_BLOCK_MAX 0x800
#define HAT_BLOCK_MAX 0x900
#define SMALL_GFX_MAX 16
#define SMALL_MTX_MAX 4

#define SLUSH_ANIM_IDLE 0x108   // ASSET_108_ANIM_SIR_SLUSH_IDLE
#define SLUSH_ANIM_ATTACK 0x109 // ASSET_109_ANIM_SIR_SLUSH_ATTACK
#define SLUSH_ANIM_DIE 0x220    // ASSET_220_ANIM_SIR_SLUSH_DIE: sinking into the snow
#define SLUSH_IDLE_SECONDS 0.8f
#define SLUSH_ATTACK_SECONDS 4.0f
#define SLUSH_SINK_SECONDS 1.6f
// Through the attack: when he stops turning, the snowball shows in his hand, and it's thrown.
#define ATTACK_AIM_UNTIL 0.45f
#define ATTACK_IN_HAND 0.45f
#define ATTACK_THROW 0.58f
#define ATTACK_DONE 0.98f
// After a throw he waits this long before another (__chSnowman_enterIdle's 0.4 seconds).
#define SLUSH_REST_SECONDS 0.4f

// Banjo-Kazooie sounds.
#define SFX_SWOOSH 0xA7        // SFX_A7_WOODEN_SWOSH: at 0.19, 0.28 and 0.37 through the attack, 18000/32767, 1.3
#define SFX_GRUNT 0x3F5        // SFX_3F5_UNKNOWN: at 0.15 through it, 32000/32767, 1.35-1.5
#define SFX_SNOWBALL 0x8F      // SFX_8F_SNOWBALL_FLYING: the throw, 30000/32767, 0.95-1.05
#define SFX_SPLAT 0x2F         // SFX_2F_ORANGE_SPLAT: a snowball landing (10000/32767) or hitting (32000)
#define SFX_METAL_HIT 0x15     // SFX_15_METALLIC_HIT_2: beaten, 30000/32767
#define SFX_GUH_HUH 0x3EA      // SFX_3EA_BANJO_GUH_HUH: beaten, 30000/32767
#define SFX_HAT_LANDS 0x1D     // SFX_1D_HITTING_AN_ENEMY_1: the hat landing, 32750/32767
#define JINGLE_JIGGY_SPAWN 0x3D // COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767
// Heard from this close (Banjo-Kazooie's 800 to 3050 units, scaled).
#define SFX_NEAR 200.0f
#define SFX_FAR 760.0f

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo; he's a bit smaller than
// his 1.0, so he stands about twice a Kong's height.
#define BK_TO_DK64 0.25f
#define SLUSH_SCALE (BK_TO_DK64 * 0.8f)
#define SNOWBALL_SCALE (BK_TO_DK64 * 1.0f)
#define HAT_SCALE SLUSH_SCALE
#define GAME_FPS 30.0f
// Sliding about: this fast (DK64 units a second), stopping this long at each end; turning a share
// of the way each frame (chSnowman turns 6 degrees a frame: about 68 12-bit units).
#define SLIDE_SPEED 25.0f
#define SLIDE_PAUSE_SECONDS 1.5f
#define SLIDE_TURN 0.15f
#define AIM_TURN_UNITS 68.0f
#define AIM_NEAR_UNITS 40.0f
// He throws at the player from this close to this far (Banjo-Kazooie's 500 to 2750 units, the far
// end brought in for DK64's closer camera), within this height; past this he sinks into the snow.
#define THROW_MIN 110.0f
#define THROW_MAX 400.0f
#define THROW_HEIGHT 125.0f
#define SINK_RADIUS 790.0f
// His body, for the player's attacks and shots.
#define BODY_RADIUS 30.0f
#define BODY_HEIGHT 95.0f
#define ATTACK_REACH 58.0f
#define SLAM_REACH 72.0f
#define ATTACK_HEIGHT 60.0f
#define PUNCH_CLOSE 36.0f
#define PUNCH_ARC 0x300
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f

// Snowballs, in DK64 units and game frames: they leave his hand this high and this far in front;
// they reach the player's spot in about SNOWBALL_FLIGHT frames (snowball.c's 32), going up at
// that and SNOWBALL_UP (its 32 units a frame, scaled) and falling at SNOWBALL_GRAVITY (its 2,
// scaled); they hit within this distance of the player's middle.
#define SNOWBALL_HAND_HEIGHT 58.0f
#define SNOWBALL_HAND_FORWARD 10.0f
#define SNOWBALL_FLIGHT 32.0f
#define SNOWBALL_UP (32.0f * BK_TO_DK64)
#define SNOWBALL_GRAVITY (2.0f * BK_TO_DK64)
#define SNOWBALL_HIT_RADIUS 16.0f
#define SNOWBALL_HIT_HEIGHT 35.0f
#define SNOWBALL_MAX_FRAMES 120
// Bursting when beaten (__chSnowman_spawnSnowballParticles, scaled): this many pieces, flying
// out and falling, each gone after a second or so.
#define BURST_PIECES 5
#define BURST_GRAVITY (1200.0f * BK_TO_DK64 / (GAME_FPS * GAME_FPS))
#define BURST_FRAMES 30
// The hat (chSnowmanHat, scaled): flung back off his head this fast and this high, falling at
// this much a frame, tumbling this much (12-bit units a frame), lying a while once down.
#define HAT_HEIGHT 88.0f
#define HAT_SPEED (34.0f * BK_TO_DK64)
#define HAT_UP (30.0f * BK_TO_DK64)
#define HAT_GRAVITY (3.0f * BK_TO_DK64)
#define HAT_TUMBLE 114.0f
#define HAT_LIE_FRAMES 90
#define REWARD_HEIGHT 6.0f
#define REWARD_DELAY_SECONDS 1.0f

// DK64 player states (Actor control_state) that hit him.
#define STATE_SIMIAN_SLAM 0x1C
#define STATE_PUNCH 0x26
#define STATE_PUNCH_2 0x27
#define STATE_PUNCH_3 0x28
#define STATE_RUNNING_ATTACK 0x29
#define STATE_AERIAL_ATTACK 0x2B
#define STATE_AERIAL_ATTACK_DIDDY 0x2E
#define STATE_AERIAL_ATTACK_CHUNKY 0x2F
#define ACTION_ENEMY_HIT 0xE

#define SLUSH_MAX 3
// Where they slide, map by map: between (ax, az) and (bx, bz), starting from a. Crystal Caves:
// along the north shelf's note trail (y 66 to 101), on the east shelf (y 108 to 119) and on the
// south-east shelf (y 168 to 178), beside theirs; every point between was checked for floor
// against DK64's own collision.
typedef struct {
    f32 ax, ay, az;
    f32 bx, bz;
} SlushPath;
typedef struct {
    Maps map;
    SlushPath path[SLUSH_MAX];
} SlushMap;
static const SlushMap slush_maps[] = {
    {MAP_CAVES,
     {{1750.0f, 101.0f, 800.0f, 1950.0f, 740.0f},
      {3150.0f, 108.0f, 2030.0f, 3330.0f, 1880.0f},
      {2480.0f, 178.0f, 2250.0f, 2650.0f, 2200.0f}}},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

#define SLUSH_SAVE_KEY(map) CAVES_SAVE_KEY(CAVES_CHARACTER_SIR_SLUSH, map)
#define SLUSH_SPOT_CHARACTER 0x12
#define SLUSH_SPOT_KEY(map) CAVES_SAVE_KEY(SLUSH_SPOT_CHARACTER, map)
#define SLUSH_SAVE_BEATEN(i) (1u << (i))
#define SLUSH_SAVE_ALL ((1u << SLUSH_MAX) - 1)

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u8 D_global_asm_8076A0B1;
// The actors that update this frame (as jiggy_conga.c reads them).
typedef struct {
    Actor *actor;
    s32 unk4;
} SlushActiveActor;
extern SlushActiveActor D_global_asm_807FB930[];
extern u16 D_global_asm_807FBB34; // How many.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
s32 func_global_asm_806EB400(Actor *player);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
s32 jiggy_banjo_air_attack(Actor *player);

typedef enum {
    SLUSH_UNLOADED,
    SLUSH_READY,
    SLUSH_UNAVAILABLE,
} SlushLoadState;

typedef enum {
    SLUSH_SLIDING,
    SLUSH_PAUSED,
    SLUSH_AIMING, // Stopped, turning to the player, between throws.
    SLUSH_ATTACKING,
    SLUSH_SINKING, // The player's far off: down into the snow,
    SLUSH_SUNK,
    SLUSH_RISING, // and back up.
    SLUSH_GONE,
} SlushAction;

typedef struct {
    const SlushPath *path;
    SlushAction action;
    f32 pos[3];
    f32 yaw;
    f32 progress;
    f32 timer;
    s32 to_b;
    s32 in_hand; // The snowball shows in his hand.
} Slush;

typedef struct {
    f32 pos[3];
    f32 vel[3];
    f32 scale;
    s32 frames;
    s32 harmless; // A piece of a beaten one.
    s32 live;
} Snowball;
#define SNOWBALL_MAX 8

typedef struct {
    f32 pos[3];
    f32 vel[3];
    f32 yaw;
    f32 tumble;
    s32 frames; // Lying, once down.
    s32 down;
    s32 live;
} Hat;

static u8 slush_block[SLUSH_BLOCK_MAX] __attribute__((aligned(16)));
static u8 snowball_block[SNOWBALL_BLOCK_MAX] __attribute__((aligned(16)));
static u8 hat_block[HAT_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx slush_gfx[SLUSH_MAX][2][SLUSH_GFX_MAX];
static Mtx slush_mtx[SLUSH_MAX][2][SLUSH_MTX_MAX];
static Gfx snowball_gfx[SNOWBALL_MAX][2][SMALL_GFX_MAX];
static Mtx snowball_mtx[SNOWBALL_MAX][2][SMALL_MTX_MAX];
static Gfx hat_gfx[SLUSH_MAX][2][SMALL_GFX_MAX];
static Mtx hat_mtx[SLUSH_MAX][2][SMALL_MTX_MAX];
static JiggyBkPose slush_pose;
static SlushLoadState slush_state = SLUSH_UNLOADED;

static s32 slush_last_map = -1;
static u32 slush_last_timer;
static const SlushMap *slush_map;
static s32 slush_placed;
static u32 slush_saved;
static Slush slushes[SLUSH_MAX];
static Snowball snowballs[SNOWBALL_MAX];
static Hat hats[SLUSH_MAX];
static f32 reward_spot[3];
static s32 reward_due;
static f32 reward_delay;
static u32 slush_random_state = 0x534C5553;

static f32 slush_random(void) {
    slush_random_state = slush_random_state * 1664525 + 1013904223;
    return (slush_random_state >> 8) * (1.0f / 16777216.0f);
}

static f32 slush_random_between(f32 low, f32 high) {
    return low + (high - low) * slush_random();
}

static s32 slush_build(u32 model, u8 *block, u32 capacity) {
    s32 size = jiggy_bk_model_build(model, block, capacity);

    return size > 0 && size <= (s32)capacity;
}

static void slush_load(void) {
    slush_state = SLUSH_UNAVAILABLE;
    if (jiggy_bk_available() && slush_build(SLUSH_MODEL, slush_block, sizeof(slush_block)) &&
        slush_build(SNOWBALL_MODEL, snowball_block, sizeof(snowball_block)) &&
        slush_build(HAT_MODEL, hat_block, sizeof(hat_block))) {
        slush_state = SLUSH_READY;
    }
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// The floor under (x, z) near `y`, or `y` if there's none near.
static f32 slush_floor(f32 x, f32 y, f32 z, s32 *found) {
    f32 probe = y + FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < FLOOR_MAX_STEP && y - floor < FLOOR_MAX_STEP) {
        if (found != NULL) {
            *found = TRUE;
        }
        return floor;
    }
    if (found != NULL) {
        *found = FALSE;
    }
    return y;
}

static void slush_sfx(const f32 pos[3], Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = distance_xz(pos[0], pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

static void slush_save(u32 bits) {
    slush_saved |= bits;
    jiggy_save_set(current_file, SLUSH_SAVE_KEY(current_map), slush_saved);
}

static void slush_place(void) {
    s32 i;

    for (i = 0; i < SLUSH_MAX; i++) {
        Slush *s = &slushes[i];

        s->path = &slush_map->path[i];
        s->pos[0] = s->path->ax;
        s->pos[1] = slush_floor(s->path->ax, s->path->ay, s->path->az, NULL);
        s->pos[2] = s->path->az;
        s->yaw = func_global_asm_80665DE0(s->path->bx, s->path->bz, s->pos[0], s->pos[2]) & 0xFFF;
        s->progress = slush_random();
        s->timer = slush_random_between(0.0f, SLIDE_PAUSE_SECONDS);
        s->to_b = TRUE;
        s->in_hand = FALSE;
        s->action = (slush_saved & SLUSH_SAVE_BEATEN(i)) ? SLUSH_GONE : SLUSH_PAUSED;
        hats[i].live = FALSE;
    }
    for (i = 0; i < SNOWBALL_MAX; i++) {
        snowballs[i].live = FALSE;
    }
    // A Golden Banana left behind is where it was.
    reward_due = (slush_saved & SLUSH_SAVE_ALL) == SLUSH_SAVE_ALL;
    reward_delay = 0.0f;
    if (reward_due) {
        u32 spot = jiggy_save_get(current_file, SLUSH_SPOT_KEY(current_map));

        reward_spot[0] = (s16)(spot >> 16);
        reward_spot[2] = (s16)(spot & 0xFFFF);
        reward_spot[1] = slush_floor(reward_spot[0], slush_map->path[SLUSH_MAX - 1].ay, reward_spot[2], NULL);
        if (spot == 0) {
            // Not saved (it shouldn't happen): at the last one's first spot.
            reward_spot[0] = slush_map->path[SLUSH_MAX - 1].ax;
            reward_spot[1] = slush_map->path[SLUSH_MAX - 1].ay;
            reward_spot[2] = slush_map->path[SLUSH_MAX - 1].az;
        }
    }
    slush_placed = TRUE;
}

// Turns a share of the way to face (x, z), at most `most` 12-bit units a frame (0: no limit).
static void slush_turn_to(Slush *s, f32 x, f32 z, f32 rate, f32 most, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, z, s->pos[0], s->pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - s->yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        turn *= rate;
        if (most > 0.0f && turn > most) {
            turn = most;
        } else if (most > 0.0f && turn < -most) {
            turn = -most;
        }
        s->yaw += turn;
        if (s->yaw < 0.0f) {
            s->yaw += 4096.0f;
        } else if (s->yaw >= 4096.0f) {
            s->yaw -= 4096.0f;
        }
    }
}

static s32 slush_facing(Slush *s, Actor *player) {
    s32 target = func_global_asm_80665DE0(player->x_position, player->z_position, s->pos[0], s->pos[2]) & 0xFFF;
    s32 off = (target - (s32)s->yaw) & 0xFFF;

    return off <= AIM_NEAR_UNITS || off >= 0x1000 - AIM_NEAR_UNITS;
}

static s32 slush_in_range(Slush *s, Actor *player) {
    f32 distance = distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - s->pos[1];

    return distance >= THROW_MIN && distance <= THROW_MAX && dy <= THROW_HEIGHT && dy >= -THROW_HEIGHT;
}

// Whether the player's attack hits him this frame (as one hits a Grublin).
static s32 slush_attacked(Slush *s, Actor *player) {
    f32 distance = distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - s->pos[1];
    s32 to_slush;
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
            if (distance <= PUNCH_CLOSE) {
                return TRUE;
            }
            to_slush = func_global_asm_80665DE0(s->pos[0], s->pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_slush - (player->y_rotation & 0xFFF)) & 0xFFF;
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

// Whether one of the player's own shots (a gun's projectile, or one of Banjo's eggs) is in him
// (as jiggy_conga.c checks).
static s32 slush_shot(Slush *s, Actor *player) {
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
        if (dy >= 0.0f && dy <= BODY_HEIGHT &&
            distance_xz(s->pos[0], s->pos[2], shot->x_position, shot->z_position) <= BODY_RADIUS) {
            return TRUE;
        }
    }
    return FALSE;
}

static Snowball *snowball_free(void) {
    s32 i;

    for (i = 0; i < SNOWBALL_MAX; i++) {
        if (!snowballs[i].live) {
            return &snowballs[i];
        }
    }
    return NULL;
}

// Throws a snowball at the player (__chSnowman_spawnSnowball, chSnowball_update).
static void slush_throw(Slush *s, Actor *player) {
    Snowball *b = snowball_free();

    if (b == NULL) {
        return;
    }
    b->pos[0] = s->pos[0] + func_global_asm_80612794((s16)s->yaw) * SNOWBALL_HAND_FORWARD;
    b->pos[1] = s->pos[1] + SNOWBALL_HAND_HEIGHT;
    b->pos[2] = s->pos[2] + func_global_asm_80612790((s16)s->yaw) * SNOWBALL_HAND_FORWARD;
    b->vel[0] = (player->x_position - b->pos[0]) / SNOWBALL_FLIGHT;
    b->vel[1] = (player->y_position + SNOWBALL_HIT_HEIGHT * 0.5f - b->pos[1]) / SNOWBALL_FLIGHT + SNOWBALL_UP;
    b->vel[2] = (player->z_position - b->pos[2]) / SNOWBALL_FLIGHT;
    b->scale = SNOWBALL_SCALE;
    b->frames = 0;
    b->harmless = FALSE;
    b->live = TRUE;
    slush_sfx(s->pos, player, SFX_SNOWBALL, 30000.0f / 32767.0f, slush_random_between(0.95f, 1.05f));
}

// Beaten (__chSnowman_deathCallback): he bursts into snowballs and his hat flies off.
static void slush_beaten(Slush *s, s32 index, Actor *player) {
    Hat *h = &hats[index];
    f32 yaw;
    s32 i;

    s->action = SLUSH_GONE;
    s->in_hand = FALSE;
    slush_sfx(s->pos, player, SFX_METAL_HIT, 30000.0f / 32767.0f, 1.0f);
    slush_sfx(s->pos, player, SFX_GUH_HUH, 30000.0f / 32767.0f, 1.0f);
    slush_sfx(s->pos, player, SFX_SPLAT, 30000.0f / 32767.0f, 1.0f);
    for (i = 0; i < BURST_PIECES; i++) {
        Snowball *b = snowball_free();

        if (b == NULL) {
            break;
        }
        b->pos[0] = s->pos[0] + slush_random_between(-80.0f, 80.0f) * SLUSH_SCALE;
        b->pos[1] = s->pos[1] + slush_random_between(0.0f, 200.0f) * SLUSH_SCALE;
        b->pos[2] = s->pos[2] + slush_random_between(-80.0f, 80.0f) * SLUSH_SCALE;
        b->vel[0] = slush_random_between(-250.0f, 350.0f) * BK_TO_DK64 / GAME_FPS;
        b->vel[1] = slush_random_between(600.0f, 960.0f) * BK_TO_DK64 / GAME_FPS;
        b->vel[2] = slush_random_between(-250.0f, 350.0f) * BK_TO_DK64 / GAME_FPS;
        b->scale = SNOWBALL_SCALE * slush_random_between(0.4f, 1.55f);
        b->frames = 0;
        b->harmless = TRUE;
        b->live = TRUE;
    }
    // Flung back off his head, to one side or the other.
    yaw = s->yaw + 2048.0f - ((object_timer & 1) ? 683.0f : 1365.0f);
    h->pos[0] = s->pos[0];
    h->pos[1] = s->pos[1] + HAT_HEIGHT;
    h->pos[2] = s->pos[2];
    h->vel[0] = func_global_asm_80612794((s16)yaw) * HAT_SPEED;
    h->vel[1] = HAT_UP;
    h->vel[2] = func_global_asm_80612790((s16)yaw) * HAT_SPEED;
    h->yaw = s->yaw;
    h->tumble = 0.0f;
    h->frames = 0;
    h->down = FALSE;
    h->live = TRUE;

    slush_save(SLUSH_SAVE_BEATEN(index));
    if ((slush_saved & SLUSH_SAVE_ALL) == SLUSH_SAVE_ALL) {
        // The last one: his Golden Banana comes where he stood.
        reward_spot[0] = s->pos[0];
        reward_spot[1] = s->pos[1];
        reward_spot[2] = s->pos[2];
        jiggy_save_set(current_file, SLUSH_SPOT_KEY(current_map),
                       ((u32)(u16)(s16)reward_spot[0] << 16) | (u16)(s16)reward_spot[2]);
        reward_due = TRUE;
        reward_delay = REWARD_DELAY_SECONDS;
    }
}

static s32 passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

// Slides toward the end he's going to; TRUE once there (or stopped by a step he can't take).
static s32 slush_slide(Slush *s, f32 seconds) {
    f32 target_x = s->to_b ? s->path->bx : s->path->ax;
    f32 target_z = s->to_b ? s->path->bz : s->path->az;
    f32 distance = distance_xz(s->pos[0], s->pos[2], target_x, target_z);
    f32 step = SLIDE_SPEED * seconds;
    f32 x, z, y;
    s32 found;

    if (distance <= step) {
        x = target_x;
        z = target_z;
    } else {
        x = s->pos[0] + (target_x - s->pos[0]) * step / distance;
        z = s->pos[2] + (target_z - s->pos[2]) * step / distance;
    }
    y = slush_floor(x, s->pos[1], z, &found);
    if (!found) {
        return TRUE;
    }
    s->pos[0] = x;
    s->pos[1] = y;
    s->pos[2] = z;
    return distance <= step;
}

// A frame of one Sir Slush (chSnowman_update), `frames` game frames on.
static void slush_update(Slush *s, s32 index, Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = s->progress;
    f32 distance = distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position);
    s32 up = s->action != SLUSH_GONE && s->action != SLUSH_SUNK && s->action != SLUSH_SINKING;

    if (frames == 0 || s->action == SLUSH_GONE) {
        return;
    }
    if (up && (slush_attacked(s, player) || slush_shot(s, player))) {
        slush_beaten(s, index, player);
        return;
    }
    if (up && s->action != SLUSH_RISING && distance > SINK_RADIUS) {
        s->action = SLUSH_SINKING;
        s->progress = 0.0f;
        s->in_hand = FALSE;
        return;
    }
    switch (s->action) {
        case SLUSH_SLIDING:
        case SLUSH_PAUSED:
            s->progress += seconds / SLUSH_IDLE_SECONDS;
            if (slush_in_range(s, player)) {
                s->action = SLUSH_AIMING;
                s->timer = 0.0f;
                break;
            }
            if (s->action == SLUSH_PAUSED) {
                s->timer -= seconds;
                if (s->timer <= 0.0f) {
                    s->action = SLUSH_SLIDING;
                    s->to_b = !s->to_b;
                }
                break;
            }
            slush_turn_to(s, s->to_b ? s->path->bx : s->path->ax, s->to_b ? s->path->bz : s->path->az, SLIDE_TURN, 0.0f,
                          frames);
            if (slush_slide(s, seconds)) {
                s->action = SLUSH_PAUSED;
                s->timer = SLIDE_PAUSE_SECONDS;
            }
            break;
        case SLUSH_AIMING:
            s->progress += seconds / SLUSH_IDLE_SECONDS;
            slush_turn_to(s, player->x_position, player->z_position, 1.0f, AIM_TURN_UNITS, frames);
            if (!slush_in_range(s, player)) {
                s->action = SLUSH_PAUSED;
                s->timer = SLIDE_PAUSE_SECONDS;
            } else if (s->timer > 0.0f) {
                s->timer -= seconds;
            } else if (slush_facing(s, player)) {
                s->action = SLUSH_ATTACKING;
                s->progress = 0.0f;
                before = 0.0f;
            }
            break;
        case SLUSH_ATTACKING:
            if (s->progress < ATTACK_AIM_UNTIL) {
                slush_turn_to(s, player->x_position, player->z_position, 1.0f, AIM_TURN_UNITS, frames);
            }
            s->progress += seconds / SLUSH_ATTACK_SECONDS;
            if (s->progress > ATTACK_DONE || !slush_in_range(s, player)) {
                s->action = SLUSH_AIMING;
                s->timer = SLUSH_REST_SECONDS;
                s->in_hand = FALSE;
                s->progress = 0.0f;
                break;
            }
            if (passed(before, s->progress, 0.15f)) {
                slush_sfx(s->pos, player, SFX_GRUNT, 32000.0f / 32767.0f, slush_random_between(1.35f, 1.5f));
            }
            if (passed(before, s->progress, 0.19f) || passed(before, s->progress, 0.28f) ||
                passed(before, s->progress, 0.37f)) {
                slush_sfx(s->pos, player, SFX_SWOOSH, 18000.0f / 32767.0f, 1.3f);
            }
            if (passed(before, s->progress, ATTACK_IN_HAND)) {
                s->in_hand = TRUE;
            } else if (passed(before, s->progress, ATTACK_THROW)) {
                s->in_hand = FALSE;
                slush_throw(s, player);
            }
            break;
        case SLUSH_SINKING:
            s->progress += seconds / SLUSH_SINK_SECONDS;
            if (s->progress >= 1.0f) {
                s->action = SLUSH_SUNK;
            }
            break;
        case SLUSH_SUNK:
            if (distance <= SINK_RADIUS) {
                s->action = SLUSH_RISING;
                s->progress = 1.0f;
            }
            break;
        case SLUSH_RISING:
            s->progress -= seconds / SLUSH_SINK_SECONDS;
            if (s->progress <= 0.0f) {
                s->action = SLUSH_PAUSED;
                s->timer = SLIDE_PAUSE_SECONDS;
                s->progress = 0.0f;
            }
            break;
        default:
            break;
    }
    if (s->action != SLUSH_SINKING && s->action != SLUSH_RISING && s->action != SLUSH_SUNK) {
        while (s->progress >= 1.0f) {
            s->progress -= 1.0f;
        }
    }
}

// Hurts the player through DK64's own enemy hit, knocking them away from where it came from.
static void slush_hit_player(const f32 from[3], Actor *player) {
    player->unkEE = func_global_asm_80665DE0(from[0], from[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
}

// The snowballs in flight, and the pieces of beaten ones.
static void snowballs_update(Actor *player, u32 frames) {
    s32 i;
    u32 f;

    for (i = 0; i < SNOWBALL_MAX; i++) {
        Snowball *b = &snowballs[i];

        for (f = 0; f < frames && b->live; f++) {
            f32 floor;
            f32 dy;
            f32 back[3];

            back[0] = b->pos[0] - b->vel[0];
            back[1] = b->pos[1];
            back[2] = b->pos[2] - b->vel[2];
            b->pos[0] += b->vel[0];
            b->vel[1] -= b->harmless ? BURST_GRAVITY : SNOWBALL_GRAVITY;
            b->pos[1] += b->vel[1];
            b->pos[2] += b->vel[2];
            b->frames++;
            if (b->harmless) {
                if (b->frames >= BURST_FRAMES) {
                    b->live = FALSE;
                }
                continue;
            }
            dy = b->pos[1] - player->y_position;
            if (dy >= 0.0f && dy <= SNOWBALL_HIT_HEIGHT &&
                distance_xz(b->pos[0], b->pos[2], player->x_position, player->z_position) <= SNOWBALL_HIT_RADIUS) {
                b->live = FALSE;
                jiggy_bk_sfx_play(SFX_SPLAT, 32000.0f / 32767.0f, 1.0f);
                if (func_global_asm_806EB400(player)) {
                    slush_hit_player(back, player);
                }
                break;
            }
            floor = b->pos[1] + FLOOR_PROBE * 0.5f;
            if (b->vel[1] < 0.0f && func_global_asm_80667110(b->pos[0], b->pos[2], &floor) &&
                floor <= b->pos[1] + FLOOR_PROBE * 0.5f && b->pos[1] <= floor) {
                b->live = FALSE;
                slush_sfx(b->pos, player, SFX_SPLAT, 10000.0f / 32767.0f, 1.0f);
                break;
            }
            if (b->frames >= SNOWBALL_MAX_FRAMES) {
                b->live = FALSE;
            }
        }
    }
}

// The hats of beaten ones, flying off and lying a while (chSnowmanHat).
static void hats_update(Actor *player, u32 frames) {
    s32 i;
    u32 f;

    for (i = 0; i < SLUSH_MAX; i++) {
        Hat *h = &hats[i];

        for (f = 0; f < frames && h->live; f++) {
            f32 floor;

            if (h->down) {
                if (++h->frames >= HAT_LIE_FRAMES) {
                    h->live = FALSE;
                }
                continue;
            }
            h->pos[0] += h->vel[0];
            h->vel[1] -= HAT_GRAVITY;
            h->pos[1] += h->vel[1];
            h->pos[2] += h->vel[2];
            h->tumble += HAT_TUMBLE;
            floor = h->pos[1] + FLOOR_PROBE * 0.5f;
            if (h->vel[1] < 0.0f && func_global_asm_80667110(h->pos[0], h->pos[2], &floor) &&
                floor <= h->pos[1] + FLOOR_PROBE * 0.5f && h->pos[1] <= floor) {
                h->pos[1] = floor;
                h->tumble = 0.0f;
                h->down = TRUE;
                slush_sfx(h->pos, player, SFX_HAT_LANDS, 32750.0f / 32767.0f, 1.0f);
            } else if (++h->frames >= SNOWBALL_MAX_FRAMES) {
                h->live = FALSE;
            }
        }
    }
}

// Draws a built model at `pos`, turned to `yaw` and tipped forward by `pitch` (12-bit units).
static Gfx *slush_pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, const f32 pos[3], f32 yaw,
                            f32 pitch, f32 scale, s32 in_hand, Gfx *gfx, u32 gfx_capacity, Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 tip[4][4];
    f32 size[4][4];
    f32 both[4][4];
    s32 i;

    guScaleF(size, scale, scale, scale);
    guRotateF(tip, pitch * (360.0f / 4096.0f), 1.0f, 0.0f, 0.0f);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(size, tip, both);
    guMtxCatF(both, rotate, slush_pose.world);
    slush_pose.world[3][0] = pos[0];
    slush_pose.world[3][1] = pos[1];
    slush_pose.world[3][2] = pos[2];
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    // chSnowman_draw's appendages: 1 the snowball in his hand, 2 shown.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        slush_pose.selectors[i] = 0;
    }
    slush_pose.selectors[1] = in_hand ? 1 : 0;
    slush_pose.selectors[2] = 1;
    slush_pose.model = model;
    slush_pose.block = block;
    slush_pose.animation = animation;
    slush_pose.progress = progress;
    slush_pose.gfx = gfx;
    slush_pose.gfx_capacity = gfx_capacity;
    slush_pose.mtx = mtx;
    slush_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&slush_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// the Sir Slushes, their snowballs and hats once a frame, with the player.
Gfx *jiggy_sir_slush_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    s32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != slush_last_map || object_timer < slush_last_timer) {
        u32 m;

        slush_last_map = current_map;
        slush_placed = FALSE;
        slush_map = NULL;
        for (m = 0; m < ARRAY_COUNT_OF(slush_maps); m++) {
            if (slush_maps[m].map == current_map) {
                slush_map = &slush_maps[m];
            }
        }
        slush_saved = slush_map != NULL ? jiggy_save_get(current_file, SLUSH_SAVE_KEY(current_map)) : 0;
    }
    if (slush_map == NULL) {
        slush_last_timer = object_timer;
        return dl;
    }
    if (slush_state == SLUSH_UNLOADED) {
        slush_load();
    }
    if (slush_state != SLUSH_READY) {
        return dl;
    }
    if (!slush_placed) {
        slush_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        slush_place();
    }
    frames = object_timer - slush_last_timer;
    slush_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    // They wait while a cutscene holds the player (a talk, for one).
    if (D_global_asm_8076A0B1 & CAVES_CUTSCENE_LOCK) {
        frames = 0;
    }
    for (i = 0; i < SLUSH_MAX; i++) {
        slush_update(&slushes[i], i, actor, frames);
    }
    snowballs_update(actor, frames);
    hats_update(actor, frames);
    if (reward_due && frames > 0) {
        if (reward_delay > 0.0f) {
            reward_delay -= frames / GAME_FPS;
            if (reward_delay <= 0.0f) {
                jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
            }
        } else if (jiggy_reward_update(CAVES_REWARD_SIR_SLUSH, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2],
                                       actor, frames)) {
            reward_due = FALSE;
        }
    }

    for (i = 0; i < SLUSH_MAX; i++) {
        Slush *s = &slushes[i];
        s32 animation;

        switch (s->action) {
            case SLUSH_GONE:
            case SLUSH_SUNK:
                continue;
            case SLUSH_ATTACKING:
                animation = SLUSH_ANIM_ATTACK;
                break;
            case SLUSH_SINKING:
            case SLUSH_RISING:
                animation = SLUSH_ANIM_DIE;
                break;
            default:
                animation = SLUSH_ANIM_IDLE;
                break;
        }
        dl = slush_pose_draw(dl, SLUSH_MODEL, slush_block, animation, s->progress, s->pos, s->yaw, 0.0f, SLUSH_SCALE,
                             s->in_hand, slush_gfx[i][buffer], SLUSH_GFX_MAX, slush_mtx[i][buffer], SLUSH_MTX_MAX);
    }
    for (i = 0; i < SNOWBALL_MAX; i++) {
        Snowball *b = &snowballs[i];

        if (!b->live) {
            continue;
        }
        dl = slush_pose_draw(dl, SNOWBALL_MODEL, snowball_block, -1, 0.0f, b->pos, (f32)((b->frames * 200) & 0xFFF),
                             (f32)((b->frames * 150) & 0xFFF), b->scale, FALSE, snowball_gfx[i][buffer], SMALL_GFX_MAX,
                             snowball_mtx[i][buffer], SMALL_MTX_MAX);
    }
    for (i = 0; i < SLUSH_MAX; i++) {
        Hat *h = &hats[i];

        if (!h->live) {
            continue;
        }
        dl = slush_pose_draw(dl, HAT_MODEL, hat_block, -1, 0.0f, h->pos, h->yaw, h->tumble, HAT_SCALE, FALSE,
                             hat_gfx[i][buffer], SMALL_GFX_MAX, hat_mtx[i][buffer], SMALL_MTX_MAX);
    }
    return dl;
}
