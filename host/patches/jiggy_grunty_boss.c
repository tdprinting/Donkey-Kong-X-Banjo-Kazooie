// Gruntilda's last stand: once the Blast-O-Matic is shut down (PERMFLAG_PROGRESS_HELM_SHUTDOWN,
// so Helm's timer has stopped), she's waiting on her broomstick in the hall past it, before the
// player takes Key 8 to K. Rool. A short version of Banjo-Kazooie's final fight
// (src/fight/chfinalboss.c, phases 1 and 2), read from the player's Banjo-Kazooie ROM.
//
// When the player first walks into the hall she taunts them in rhyme (with Banjo-Kazooie's
// menacing Gruntilda jingle), then the fight's on. She circles the hall on her broomstick out of
// reach (chfinalboss's travel state, ASSET_1C5 flying), now one way, now the other; then either
//  - drops low and casts (ASSET_25A, the spell glowing in her hand: appendage selector 5) a green
//    spell at the player (ASSET_6C9_SPRITE_GREEN_SPELL_ATTACK, with SFX_146/147_GRUNTY_SPELL_ATTACK,
//    bursting with SFX_148_GRUNTY_SPELL_LANDING), three in a fan once she's been hit three times,
//    and hovers there a moment before climbing back up, or
//  - swoops through where the player stands (ASSET_25C, chfinalboss phase 1's swoop) and back up.
// A spell or her broom hurts the player through DK64's own enemy hit (setAction(0xE), as
// jiggy_grublins.c does), and she cackles. She's hit by the player's attacks within reach (the
// punches, rolls, aerial attacks and the Simian Slam, as jiggy_conga.c checks) or by the player's
// shots (DK64's guns, and so Banjo's eggs, jiggy_banjo_eggs.c), which reach her while she's low
// casting or swooping, or any time a jump does. Each hit is her "ow" (ASSET_258, SFX_132_GRUNTY_YOW
// or SFX_133_GRUNTY_OHW with SFX_1F_HITTING_AN_ENEMY_3, as phase 2's hit plays them) and a moment
// she can't be hit; five beat her.
//
// Beaten, the ending is a short cutscene with the player held (the cutscene lock): her WEEEGH
// (ASSET_259, SFX_131), her broomstick snapping (SFX_D9, its appendages hidden), her fall
// (ASSET_263 with COMUSIC_91_GRUNTY_FALLING) and crash (ASSET_25D, SFX_5B), a last rhyme in her
// text box, and she vanishes with her echoing cry (SFX_130). Her Jiggy (a Golden Banana drawn as
// a Jiggy, jiggy_rewards.c reward 0x81) appears in the hall. DK64's own ending is untouched:
// K. Rool still waits after Key 8.
//
// Saved with the file (jiggy_save key 0x58010000 | map): whether she's taunted the player and
// whether she's beaten. Leaving or dying mid-fight starts it over next time.
//
// Drawn right after the player's own model (patches_draw.c); her text box is
// jiggy_gruntilda_hud_draw. Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "jiggy.h"
#include "jiggy_gruntilda.h"

// Where the fight is: Hideout Helm's hall past the Blast-O-Matic (floor 64 with low steps up to
// 74, from x 300 to 780 and z 4940 to 5420, found in the map's floors; pillars at its corners,
// a ceiling about 150 up), around its middle. She circles at this radius, clear of the
// pillars; the fight starts when the player is this far in. Her Jiggy appears on the hall's
// raised floor south of the middle.
typedef struct {
    Maps map;
    f32 x, y, z;           // The middle of the hall, on its floor.
    f32 radius;            // Her circle.
    f32 trigger_radius;    // The player has walked in.
    f32 reward_x, reward_z;
} BossArena;
static const BossArena boss_arenas[] = {
    {MAP_HELM, 540.0f, 64.0f, 5200.0f, 150.0f, 210.0f, 540.0f, 5060.0f},
};
#define ARENA_KEEP_RADIUS 215.0f // She doesn't swoop out past this.
#define ARENA_HEIGHT 45.0f       // The player is in the hall within this of its floor.
#define PERMFLAG_HELM_SHUTDOWN 0x302 // PERMFLAG_PROGRESS_HELM_SHUTDOWN
#define FLAG_TYPE_PERMANENT 0
#define REWARD_HEIGHT 12.5f
#define FLOOR_PROBE 30.0f
#define FLOOR_MAX_STEP 40.0f

// Her flight, in DK64 units above the hall's floor and 12-bit angle units a second.
#define FLY_HEIGHT 55.0f
#define HOVER_HEIGHT 8.0f
#define SWOOP_LOW 4.0f
#define FLY_TURN_SPEED 560.0f
#define FLY_TURN_SPEED_ANGRY 780.0f
#define FLY_BOB 4.0f
#define FLY_MIN_SECONDS 2.2f
#define FLY_MAX_SECONDS 3.6f
#define DESCEND_SECONDS 0.7f
#define ASCEND_SECONDS 0.7f
#define HOVER_SECONDS 1.4f
#define HOVER_SECONDS_ANGRY 1.0f
#define SWOOP_SPEED 210.0f      // A second.
#define SWOOP_OVERSHOOT 120.0f
#define SWOOP_CHANCE 0.4f
#define TURN_RATE 0.15f
// Hits: five beat her, each followed by this long she can't be hit; three make her angry.
#define BOSS_HITS 5
#define BOSS_ANGRY_HITS 3
#define INVULNERABLE_SECONDS 1.6f
// Her body, for hits both ways: this wide round her, from her broom up GRUNTY_HEIGHT.
#define BODY_RADIUS 34.0f
#define BROOM_HIT_RADIUS 30.0f
// The player's attacks (as jiggy_conga.c checks them): their reach, and the punches' arc.
#define ATTACK_REACH 60.0f
#define SLAM_REACH 76.0f
#define ATTACK_HEIGHT 40.0f
#define PUNCH_ARC 0x300
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// The ending: how fast she falls, how long she vanishes for.
#define FALL_GRAVITY 0.5f // A frame, each frame.
#define VANISH_SECONDS 1.5f
#define LAND_HOLD_AT 0.97f

// Her spells: Banjo-Kazooie's green spell sprite, flying straight at where the player was.
#define SPELL_SPRITE 0x6C9 // ASSET_6C9_SPRITE_GREEN_SPELL_ATTACK
#define SPELL_MAX_FRAMES 10
#define SPELL_MAX_TEXELS (32 * 32)
#define SPELL_TICKS_PER_FRAME 2
#define SPELL_SIZE 24.0f
#define SPELL_MAX 6
#define SPELL_SPEED 5.5f       // A frame.
#define SPELL_HAND_HEIGHT 55.0f
#define SPELL_HAND_FORWARD 14.0f
#define SPELL_AIM_HEIGHT 18.0f // At the player's middle.
#define SPELL_FAN 0x110        // The angry fan's spread either side.
#define SPELL_HIT_RADIUS 18.0f
#define SPELL_HIT_BELOW 5.0f
#define SPELL_HIT_ABOVE 45.0f
#define SPELL_LIFE 150
#define SPELL_FLOOR_PROBE 20.0f

// DK64 player states (Actor control_state) that hit her.
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
#define CUTSCENE_LOCK 0x10
// RGBA16 texels that fit in TMEM at once.
#define TMEM_RGBA16_TEXELS 2048

#define BOSS_SAVE_TAUNTED 0x1
#define BOSS_SAVE_BEATEN 0x2

static const char *const boss_taunt_first[] = {
    "SO YOU SHUT DOWN K. ROOL'S MACHINE? THE NASTIEST TRICK I'VE EVER SEEN!",
    "BUT GRUNTY'S HERE WITH BROOM AND SPELL, TO BLAST YOU HAIRY APES PELL-MELL!",
    "NO KEY FOR YOU, NO HAPPY END, I'LL ZAP YOU TILL YOU CAN'T DEFEND!",
};
static const char *const boss_taunt_again[] = {
    "BACK FOR MORE? HOW VERY BRAVE! THIS HALL SHALL BE YOUR BANANA GRAVE!",
};
static const char *const boss_beaten[] = {
    "OOOH! MY BROOM IS SNAPPED IN TWO, BEATEN BY A HAIRY CREW!",
    "TAKE YOUR JIGGY, TAKE YOUR KEY, BUT K. ROOL WON'T GO DOWN LIKE ME!",
    "I'LL BE BACK, JUST WAIT AND SEE, WITH SPELLS TO SET MY BEAUTY FREE!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

typedef enum {
    BOSS_WAITING,  // Hovering in the middle until the player walks in.
    BOSS_TAUNT,
    BOSS_FLY,      // Circling.
    BOSS_DESCEND,
    BOSS_CAST,
    BOSS_HOVER,    // Low after a cast.
    BOSS_ASCEND,
    BOSS_SWOOP,
    BOSS_HIT,
    BOSS_BEATEN,   // The ending: WEEEGH,
    BOSS_FALL,     // off her broom,
    BOSS_LANDED,   // crash and rhyme,
    BOSS_VANISH,   // and gone.
    BOSS_GONE,
} BossAction;

typedef struct {
    s32 live;
    f32 pos[3];
    f32 vel[3];
    s32 frames;
} Spell;

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 cc_player_index;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u8 D_global_asm_8076A0B1;
// The actors that update this frame (as jiggy_banjo_eggs.c reads them).
typedef struct {
    Actor *actor;
    s32 unk4;
} BossActiveActor;
extern BossActiveActor D_global_asm_807FB930[];
extern u16 D_global_asm_807FBB34; // How many.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
s32 func_global_asm_806EB400(Actor *player); // Whether the player can be hurt now.
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle
u8 isFlagSet(s16 flagIndex, u8 flagType);

static s32 boss_last_map = -1;
static u32 boss_last_timer;
static const BossArena *boss_arena;
static s32 boss_placed;
static u32 boss_saved;
static BossAction boss_action;
static f32 boss_pos[3];
static f32 boss_height;     // Above the hall's floor.
static f32 boss_yaw;
static f32 boss_angle;      // Round the middle, 12-bit angle units.
static f32 boss_radius;     // From the middle.
static f32 boss_direction;  // 1 or -1 round the circle.
static f32 boss_timer;      // Seconds left in the action (or into it).
static f32 boss_from_height;
static s32 boss_anim;
static f32 boss_anim_seconds;
static s32 boss_anim_loops;
static f32 boss_progress;
static s32 boss_hits;
static f32 boss_invulnerable;
static s32 boss_broom;
static f32 boss_scale;
static f32 boss_fall_speed;
static s32 boss_talked;      // The ending's rhyme is done.
static s32 boss_cast_done;
static s32 boss_swoop_hurt;
static f32 boss_swoop_from[3];
static f32 boss_swoop_dir[2];
static f32 boss_swoop_low_at;  // Distance along the swoop where she's lowest.
static f32 boss_swoop_length;
static f32 boss_swoop_travelled;
static Spell spells[SPELL_MAX];
static f32 reward_spot[3];
static s32 reward_jingle;

// The spell sprite: every frame, and a quad for each.
typedef enum {
    SPELL_SPRITE_UNLOADED,
    SPELL_SPRITE_READY,
    SPELL_SPRITE_UNAVAILABLE,
} SpellSpriteState;
static SpellSpriteState spell_state = SPELL_SPRITE_UNLOADED;
static u16 spell_texels[SPELL_MAX_FRAMES][SPELL_MAX_TEXELS] __attribute__((aligned(8)));
static s16 spell_width[SPELL_MAX_FRAMES];
static s16 spell_height[SPELL_MAX_FRAMES];
static Vtx spell_vtx[SPELL_MAX_FRAMES][4] __attribute__((aligned(8)));
static Mtx spell_mtx[2][SPELL_MAX];
static s32 spell_frames;

static void boss_play(s32 animation, f32 seconds, s32 loops) {
    boss_anim = animation;
    boss_anim_seconds = seconds;
    boss_anim_loops = loops;
    boss_progress = 0.0f;
}

static void boss_save(u32 bits) {
    boss_saved |= bits;
    jiggy_save_set(current_file, GRUNTY_SAVE_KEY(GRUNTY_SAVE_BOSS, current_map), boss_saved);
}

static void spell_set_vtx(Vtx *v, s32 x, s32 y, s32 s, s32 t) {
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

static void spells_load(void) {
    s32 count;
    s32 i;

    spell_state = SPELL_SPRITE_UNAVAILABLE;
    count = jiggy_bk_sprite_frame_count(SPELL_SPRITE);
    if (count <= 0) {
        return;
    }
    if (count > SPELL_MAX_FRAMES) {
        count = SPELL_MAX_FRAMES;
    }
    spell_frames = 0;
    for (i = 0; i < count; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(SPELL_SPRITE, i, spell_texels[spell_frames], sizeof(spell_texels[0]));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;
        Vtx *v = spell_vtx[spell_frames];

        if (got <= 0 || width == 0 || height == 0 || width * height > SPELL_MAX_TEXELS || width * height > TMEM_RGBA16_TEXELS) {
            continue;
        }
        spell_width[spell_frames] = width;
        spell_height[spell_frames] = height;
        spell_set_vtx(&v[0], -width / 2, height / 2, 0, 0);
        spell_set_vtx(&v[1], width / 2, height / 2, width, 0);
        spell_set_vtx(&v[2], width / 2, -height / 2, width, height);
        spell_set_vtx(&v[3], -width / 2, -height / 2, 0, height);
        spell_frames++;
    }
    if (spell_frames > 0) {
        spell_state = SPELL_SPRITE_READY;
    }
}

static void spells_clear(void) {
    s32 i;

    for (i = 0; i < SPELL_MAX; i++) {
        spells[i].live = FALSE;
    }
}

// Turns a share of the way to `target` (12-bit angle units) over `frames` frames.
static void boss_turn_to(s16 target, u32 frames) {
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = (target & 0xFFF) - boss_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        boss_yaw += turn * TURN_RATE;
        if (boss_yaw < 0.0f) {
            boss_yaw += 4096.0f;
        } else if (boss_yaw >= 4096.0f) {
            boss_yaw -= 4096.0f;
        }
    }
}

static void boss_face_player(Actor *player, u32 frames) {
    boss_turn_to(func_global_asm_80665DE0(player->x_position, player->z_position, boss_pos[0], boss_pos[2]), frames);
}

static void boss_start_flying(void) {
    boss_action = BOSS_FLY;
    boss_angle = func_global_asm_80665DE0(boss_pos[0], boss_pos[2], boss_arena->x, boss_arena->z) & 0xFFF;
    boss_radius = grunty_distance_xz(boss_pos[0], boss_pos[2], boss_arena->x, boss_arena->z);
    boss_direction = grunty_random() < 0.5f ? 1.0f : -1.0f;
    boss_timer = grunty_random_between(FLY_MIN_SECONDS, FLY_MAX_SECONDS);
    boss_play(GRUNTY_ANIM_FLY, GRUNTY_FLY_SECONDS, TRUE);
}

static void boss_place(Actor *player) {
    boss_pos[0] = boss_arena->x;
    boss_pos[2] = boss_arena->z;
    boss_height = FLY_HEIGHT;
    boss_pos[1] = boss_arena->y + boss_height;
    boss_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, boss_pos[0], boss_pos[2]) & 0xFFF;
    boss_hits = 0;
    boss_invulnerable = 0.0f;
    boss_broom = TRUE;
    boss_scale = 1.0f;
    boss_talked = FALSE;
    reward_spot[0] = boss_arena->reward_x;
    reward_spot[1] = grunty_floor(boss_arena->reward_x, boss_arena->y, boss_arena->reward_z, FLOOR_PROBE, FLOOR_MAX_STEP);
    reward_spot[2] = boss_arena->reward_z;
    reward_jingle = FALSE;
    spells_clear();
    boss_action = (boss_saved & BOSS_SAVE_BEATEN) ? BOSS_GONE : BOSS_WAITING;
    boss_play(GRUNTY_ANIM_FLY, GRUNTY_FLY_SECONDS, TRUE);
    boss_placed = TRUE;
}

// Whether the player is in the hall.
static s32 boss_player_in_arena(Actor *player) {
    f32 dy = player->y_position - boss_arena->y;

    return grunty_distance_xz(boss_arena->x, boss_arena->z, player->x_position, player->z_position) <= boss_arena->trigger_radius &&
           dy < ARENA_HEIGHT && dy > -ARENA_HEIGHT;
}

// Whether the player's attack reaches her this frame (as jiggy_conga.c checks, for her size).
static s32 boss_attacked(Actor *player) {
    f32 distance = grunty_distance_xz(boss_pos[0], boss_pos[2], player->x_position, player->z_position);
    s32 to_her;
    s32 off;

    if (player->y_position + ATTACK_HEIGHT < boss_pos[1] || player->y_position > boss_pos[1] + GRUNTY_HEIGHT) {
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
            if (distance <= BODY_RADIUS) {
                return TRUE;
            }
            to_her = func_global_asm_80665DE0(boss_pos[0], boss_pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_her - (player->y_rotation & 0xFFF)) & 0xFFF;
            return off <= PUNCH_ARC || off >= 0x1000 - PUNCH_ARC;
        case STATE_RUNNING_ATTACK:
        case STATE_AERIAL_ATTACK:
        case STATE_AERIAL_ATTACK_DIDDY:
        case STATE_AERIAL_ATTACK_CHUNKY:
            return distance <= ATTACK_REACH;
        default:
            return FALSE;
    }
}

// Whether one of the player's own shots (a gun's projectile, or one of Banjo's eggs) is in her.
static s32 boss_shot(Actor *player) {
    s32 i;

    for (i = 0; i < D_global_asm_807FBB34; i++) {
        Actor *shot = D_global_asm_807FB930[i].actor;

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
        if (shot->y_position >= boss_pos[1] - SPELL_HIT_BELOW && shot->y_position <= boss_pos[1] + GRUNTY_HEIGHT &&
            grunty_distance_xz(boss_pos[0], boss_pos[2], shot->x_position, shot->z_position) <= BODY_RADIUS) {
            return TRUE;
        }
    }
    return FALSE;
}

// Hurts the player through DK64's own enemy hit, knocking them away from `from`; she cackles.
static void boss_hurt_player(Actor *player, const f32 from[3]) {
    // The knockback turns the player's heading round, so it's set toward what hit them first.
    player->unkEE = func_global_asm_80665DE0(from[0], from[2], player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
    if (grunty_random() < 0.6f) {
        grunty_sfx(boss_pos, player, grunty_random() < 0.5f ? GRUNTY_SFX_LAUGH_1 : GRUNTY_SFX_LAUGH_3, 1.0f,
                   grunty_random_between(0.95f, 1.05f));
    }
}

// Casts at the player: one spell, or three in a fan once she's angry.
static void boss_cast(Actor *player) {
    s32 count = boss_hits >= BOSS_ANGRY_HITS ? 3 : 1;
    f32 hand[3];
    f32 aim[3];
    f32 length;
    s32 n;
    s32 i;

    hand[0] = boss_pos[0] + func_global_asm_80612794((s16)boss_yaw) * SPELL_HAND_FORWARD;
    hand[1] = boss_pos[1] + SPELL_HAND_HEIGHT;
    hand[2] = boss_pos[2] + func_global_asm_80612790((s16)boss_yaw) * SPELL_HAND_FORWARD;
    aim[0] = player->x_position - hand[0];
    aim[1] = player->y_position + SPELL_AIM_HEIGHT - hand[1];
    aim[2] = player->z_position - hand[2];
    length = _sqrtf(aim[0] * aim[0] + aim[1] * aim[1] + aim[2] * aim[2]);
    if (length < 1.0f) {
        return;
    }
    for (n = 0; n < count; n++) {
        Spell *s = NULL;
        s16 spread = count == 1 ? 0 : (n - 1) * SPELL_FAN;
        f32 c = func_global_asm_80612790(spread & 0xFFF);
        f32 sn = func_global_asm_80612794(spread & 0xFFF);

        for (i = 0; i < SPELL_MAX; i++) {
            if (!spells[i].live) {
                s = &spells[i];
                break;
            }
        }
        if (s == NULL) {
            break;
        }
        s->pos[0] = hand[0];
        s->pos[1] = hand[1];
        s->pos[2] = hand[2];
        // Turned `spread` round the vertical from straight at the player.
        s->vel[0] = (aim[0] * c + aim[2] * sn) / length * SPELL_SPEED;
        s->vel[1] = aim[1] / length * SPELL_SPEED;
        s->vel[2] = (aim[2] * c - aim[0] * sn) / length * SPELL_SPEED;
        s->frames = 0;
        s->live = TRUE;
    }
    grunty_sfx(boss_pos, player, count == 1 ? GRUNTY_SFX_SPELL_1 : GRUNTY_SFX_SPELL_2, 1.0f, grunty_random_between(0.95f, 1.05f));
}

// The spells in flight, `frames` game frames on.
static void spells_update(Actor *player, u32 frames) {
    s32 i;
    u32 f;

    for (i = 0; i < SPELL_MAX; i++) {
        Spell *s = &spells[i];

        for (f = 0; f < frames && s->live; f++) {
            f32 dy;
            f32 floor;

            s->pos[0] += s->vel[0];
            s->pos[1] += s->vel[1];
            s->pos[2] += s->vel[2];
            s->frames++;
            dy = s->pos[1] - player->y_position;
            if (dy >= -SPELL_HIT_BELOW && dy <= SPELL_HIT_ABOVE &&
                grunty_distance_xz(s->pos[0], s->pos[2], player->x_position, player->z_position) <= SPELL_HIT_RADIUS) {
                s->live = FALSE;
                jiggy_bk_sfx_play(GRUNTY_SFX_SPELL_LANDING, 1.0f, 1.0f);
                if (func_global_asm_806EB400(player)) {
                    f32 from[3];

                    from[0] = s->pos[0] - s->vel[0] * 4.0f;
                    from[1] = s->pos[1];
                    from[2] = s->pos[2] - s->vel[2] * 4.0f;
                    boss_hurt_player(player, from);
                }
                break;
            }
            // The floor under it (the return value isn't reliable, as jiggy_notes.c notes).
            floor = s->pos[1] + SPELL_FLOOR_PROBE;
            func_global_asm_80667110(s->pos[0], s->pos[2], &floor);
            if (floor != s->pos[1] + SPELL_FLOOR_PROBE && s->pos[1] <= floor) {
                s->live = FALSE;
                grunty_sfx(s->pos, player, GRUNTY_SFX_SPELL_LANDING, 1.0f, 1.0f);
                break;
            }
            if (s->frames >= SPELL_LIFE || s->pos[1] < boss_arena->y - 60.0f) {
                s->live = FALSE;
            }
        }
    }
}

// Hit: ow, or beaten on the last.
static void boss_hurt(Actor *player) {
    boss_hits++;
    boss_invulnerable = INVULNERABLE_SECONDS;
    jiggy_bk_sfx_play(GRUNTY_SFX_HIT_ENEMY, 1.0f, grunty_random_between(0.95f, 1.05f));
    if (boss_hits >= BOSS_HITS) {
        boss_action = BOSS_BEATEN;
        boss_play(GRUNTY_ANIM_WEEEGH, GRUNTY_WEEEGH_SECONDS, FALSE);
        spells_clear();
        boss_save(BOSS_SAVE_BEATEN);
        grunty_lock_player(TRUE);
        jiggy_bk_sfx_play(GRUNTY_SFX_WEEEGH, 1.0f, 1.0f);
        return;
    }
    boss_action = BOSS_HIT;
    boss_from_height = boss_height;
    boss_play(GRUNTY_ANIM_OW, GRUNTY_OW_SECONDS, FALSE);
    jiggy_bk_sfx_play(boss_hits & 1 ? GRUNTY_SFX_YOW : GRUNTY_SFX_OHW, 1.0f, grunty_random_between(0.95f, 1.05f));
}

static void boss_start_swoop(Actor *player) {
    f32 dx = player->x_position - boss_pos[0];
    f32 dz = player->z_position - boss_pos[2];
    f32 to_player = _sqrtf(dx * dx + dz * dz);
    f32 overshoot = SWOOP_OVERSHOOT;

    if (to_player < 1.0f) {
        boss_action = BOSS_DESCEND;
        boss_timer = 0.0f;
        boss_from_height = boss_height;
        return;
    }
    boss_swoop_dir[0] = dx / to_player;
    boss_swoop_dir[1] = dz / to_player;
    // Not out past the hall's walls.
    while (overshoot > 0.0f &&
           grunty_distance_xz(player->x_position + boss_swoop_dir[0] * overshoot, player->z_position + boss_swoop_dir[1] * overshoot,
                              boss_arena->x, boss_arena->z) > ARENA_KEEP_RADIUS) {
        overshoot -= 20.0f;
    }
    if (overshoot < 0.0f) {
        overshoot = 0.0f;
    }
    boss_swoop_from[0] = boss_pos[0];
    boss_swoop_from[2] = boss_pos[2];
    boss_from_height = boss_height;
    boss_swoop_low_at = to_player;
    boss_swoop_length = to_player + overshoot;
    boss_swoop_travelled = 0.0f;
    boss_swoop_hurt = FALSE;
    boss_action = BOSS_SWOOP;
    boss_turn_to(func_global_asm_80665DE0(player->x_position, player->z_position, boss_pos[0], boss_pos[2]), 30);
    boss_play(GRUNTY_ANIM_SWOOP, GRUNTY_SWOOP_SECONDS, TRUE);
    grunty_sfx(boss_pos, player, GRUNTY_SFX_LAUGH_2, 1.0f, 1.0f);
}

// A frame of her fight, `frames` game frames on.
static void boss_update(Actor *player, u32 frames) {
    f32 seconds = frames / GRUNTY_GAME_FPS;
    s32 angry = boss_hits >= BOSS_ANGRY_HITS;
    s32 event = GRUNTY_TALK_NONE;
    s32 vulnerable;
    f32 t;

    boss_progress += seconds / boss_anim_seconds;
    if (boss_progress >= 1.0f) {
        if (boss_anim_loops) {
            while (boss_progress >= 1.0f) {
                boss_progress -= 1.0f;
            }
        } else {
            boss_progress = 1.0f;
        }
    }
    if (grunty_talk_open()) {
        event = grunty_talk_update();
    }
    vulnerable = boss_action >= BOSS_FLY && boss_action <= BOSS_SWOOP;
    if (vulnerable) {
        if (boss_invulnerable > 0.0f) {
            boss_invulnerable -= seconds;
        } else if (boss_attacked(player) || boss_shot(player)) {
            boss_hurt(player);
        }
    }

    switch (boss_action) {
        case BOSS_WAITING:
            boss_face_player(player, frames);
            if (boss_player_in_arena(player) && player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT) {
                boss_action = BOSS_TAUNT;
                grunty_lock_player(TRUE);
                jiggy_bk_jingle_play(GRUNTY_JINGLE_MENACING, 1.0f);
                grunty_sfx(boss_pos, player, GRUNTY_SFX_LAUGH_4, 1.0f, 1.0f);
                if (boss_saved & BOSS_SAVE_TAUNTED) {
                    grunty_talk_pages(boss_taunt_again, ARRAY_COUNT_OF(boss_taunt_again));
                } else {
                    grunty_talk_pages(boss_taunt_first, ARRAY_COUNT_OF(boss_taunt_first));
                }
            }
            break;
        case BOSS_TAUNT:
            boss_face_player(player, frames);
            if (event == GRUNTY_TALK_DONE) {
                boss_save(BOSS_SAVE_TAUNTED);
                grunty_lock_player(FALSE);
                boss_start_flying();
            }
            break;
        case BOSS_FLY:
            boss_angle += boss_direction * (angry ? FLY_TURN_SPEED_ANGRY : FLY_TURN_SPEED) * seconds;
            while (boss_angle >= 4096.0f) {
                boss_angle -= 4096.0f;
            }
            while (boss_angle < 0.0f) {
                boss_angle += 4096.0f;
            }
            // Back out to her circle and up to her height, after a swoop or a cast.
            t = seconds * 2.0f > 1.0f ? 1.0f : seconds * 2.0f;
            boss_radius += (boss_arena->radius - boss_radius) * t;
            boss_height += (FLY_HEIGHT - boss_height) * t;
            boss_pos[0] = boss_arena->x + func_global_asm_80612794((s16)boss_angle) * boss_radius;
            boss_pos[2] = boss_arena->z + func_global_asm_80612790((s16)boss_angle) * boss_radius;
            boss_turn_to((s16)(boss_angle + boss_direction * 1024.0f), frames);
            boss_timer -= seconds;
            if (boss_timer <= 0.0f) {
                if (boss_hits > 0 && grunty_random() < SWOOP_CHANCE) {
                    boss_start_swoop(player);
                } else {
                    boss_action = BOSS_DESCEND;
                    boss_timer = 0.0f;
                    boss_from_height = boss_height;
                }
            }
            break;
        case BOSS_DESCEND:
            boss_face_player(player, frames);
            boss_timer += seconds;
            t = boss_timer / DESCEND_SECONDS;
            if (t >= 1.0f) {
                t = 1.0f;
                boss_action = BOSS_CAST;
                boss_cast_done = FALSE;
                boss_play(GRUNTY_ANIM_CAST, GRUNTY_CAST_SECONDS, FALSE);
            }
            boss_height = boss_from_height + (HOVER_HEIGHT - boss_from_height) * t;
            break;
        case BOSS_CAST:
            boss_face_player(player, frames);
            if (!boss_cast_done && boss_progress >= GRUNTY_CAST_RELEASE_AT) {
                boss_cast_done = TRUE;
                boss_cast(player);
            }
            if (boss_progress >= 1.0f) {
                boss_action = BOSS_HOVER;
                boss_timer = angry ? HOVER_SECONDS_ANGRY : HOVER_SECONDS;
                boss_play(GRUNTY_ANIM_FLY, GRUNTY_FLY_SECONDS, TRUE);
            }
            break;
        case BOSS_HOVER:
            boss_face_player(player, frames);
            boss_timer -= seconds;
            if (boss_timer <= 0.0f) {
                boss_action = BOSS_ASCEND;
                boss_timer = 0.0f;
                boss_from_height = boss_height;
            }
            break;
        case BOSS_ASCEND:
            boss_timer += seconds;
            t = boss_timer / ASCEND_SECONDS;
            if (t >= 1.0f) {
                t = 1.0f;
            }
            boss_height = boss_from_height + (FLY_HEIGHT - boss_from_height) * t;
            if (t >= 1.0f) {
                boss_start_flying();
            }
            break;
        case BOSS_SWOOP:
            boss_swoop_travelled += SWOOP_SPEED * seconds;
            if (boss_swoop_travelled > boss_swoop_length) {
                boss_swoop_travelled = boss_swoop_length;
            }
            boss_pos[0] = boss_swoop_from[0] + boss_swoop_dir[0] * boss_swoop_travelled;
            boss_pos[2] = boss_swoop_from[2] + boss_swoop_dir[1] * boss_swoop_travelled;
            if (boss_swoop_travelled <= boss_swoop_low_at) {
                t = boss_swoop_travelled / boss_swoop_low_at;
                boss_height = boss_from_height + (SWOOP_LOW - boss_from_height) * (1.0f - (1.0f - t) * (1.0f - t));
            } else {
                t = (boss_swoop_travelled - boss_swoop_low_at) / (boss_swoop_length - boss_swoop_low_at + 1.0f);
                boss_height = SWOOP_LOW + (FLY_HEIGHT - SWOOP_LOW) * t * t;
            }
            // Her broom bowls the player over.
            if (!boss_swoop_hurt && func_global_asm_806EB400(player) &&
                grunty_distance_xz(boss_pos[0], boss_pos[2], player->x_position, player->z_position) <= BROOM_HIT_RADIUS &&
                player->y_position + ATTACK_HEIGHT >= boss_pos[1] && player->y_position <= boss_pos[1] + GRUNTY_HEIGHT) {
                boss_swoop_hurt = TRUE;
                boss_hurt_player(player, boss_pos);
            }
            if (boss_swoop_travelled >= boss_swoop_length) {
                boss_start_flying();
            }
            break;
        case BOSS_HIT:
            boss_height += (FLY_HEIGHT - boss_height) * (seconds * 1.5f > 1.0f ? 1.0f : seconds * 1.5f);
            if (boss_progress >= 1.0f) {
                boss_start_flying();
            }
            break;
        case BOSS_BEATEN:
            if (boss_progress >= 1.0f) {
                // Her broom snaps and down she goes.
                boss_action = BOSS_FALL;
                boss_broom = FALSE;
                boss_fall_speed = 0.0f;
                boss_play(GRUNTY_ANIM_FALL, GRUNTY_FALL_SECONDS, TRUE);
                jiggy_bk_sfx_play(GRUNTY_SFX_BROOM_BREAK, 1.0f, 1.0f);
                jiggy_bk_sfx_play(GRUNTY_SFX_FALLING_OFF, 1.0f, 1.0f);
                jiggy_bk_jingle_play(GRUNTY_JINGLE_FALLING, 30000.0f / 32767.0f);
            }
            break;
        case BOSS_FALL: {
            f32 floor = grunty_floor(boss_pos[0], boss_arena->y, boss_pos[2], FLOOR_PROBE, FLOOR_MAX_STEP);
            u32 f;

            for (f = 0; f < frames; f++) {
                boss_fall_speed += FALL_GRAVITY;
                boss_height -= boss_fall_speed;
            }
            if (boss_arena->y + boss_height <= floor) {
                boss_height = floor - boss_arena->y;
                boss_action = BOSS_LANDED;
                boss_play(GRUNTY_ANIM_LAND, GRUNTY_LAND_SECONDS, FALSE);
                jiggy_bk_sfx_play(GRUNTY_SFX_THUD, 1.0f, 1.0f);
                jiggy_bk_sfx_play(GRUNTY_SFX_AH, 1.0f, 1.0f);
                grunty_talk_pages(boss_beaten, ARRAY_COUNT_OF(boss_beaten));
            }
            break;
        }
        case BOSS_LANDED:
            boss_face_player(player, frames);
            // She lies there till she's done talking.
            if (boss_progress > LAND_HOLD_AT) {
                boss_progress = LAND_HOLD_AT;
            }
            if (event == GRUNTY_TALK_DONE) {
                boss_talked = TRUE;
            }
            if (boss_talked) {
                boss_action = BOSS_VANISH;
                boss_timer = 0.0f;
                jiggy_bk_sfx_play(GRUNTY_SFX_ECHOING_CRY, 1.0f, 1.0f);
            }
            break;
        case BOSS_VANISH:
            if (boss_progress > LAND_HOLD_AT) {
                boss_progress = LAND_HOLD_AT;
            }
            boss_timer += seconds;
            boss_scale = 1.0f - boss_timer / VANISH_SECONDS;
            if (boss_scale <= 0.0f) {
                boss_scale = 0.0f;
                boss_action = BOSS_GONE;
                grunty_lock_player(FALSE);
                reward_jingle = TRUE;
            }
            break;
        case BOSS_GONE:
            break;
    }
    boss_pos[1] = boss_arena->y + boss_height;
    if (boss_action == BOSS_WAITING || boss_action == BOSS_TAUNT || boss_action == BOSS_FLY || boss_action == BOSS_HOVER) {
        boss_pos[1] += FLY_BOB * func_global_asm_80612794((s16)(object_timer * 40));
    }
    spells_update(player, frames);

    // Her Jiggy, once she's gone, until it's collected.
    if (boss_action == BOSS_GONE) {
        if (reward_jingle && !jiggy_reward_collected(GRUNTY_REWARD_BOSS)) {
            jiggy_bk_jingle_play(GRUNTY_JINGLE_JIGGY_SPAWN, 1.0f);
        }
        reward_jingle = FALSE;
        jiggy_reward_update(GRUNTY_REWARD_BOSS, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], player, frames);
    }
}

// A matrix that stands a spell at pos facing the camera (as jiggy_notes.c's note_billboard).
static void spell_billboard(Mtx *m, f32 pos[3], s32 width) {
    CharacterChange *cc = &character_change_array[cc_player_index];
    f32 mf[4][4];
    f32 f[3], r[3], u[3];
    f32 length;
    f32 scale = SPELL_SIZE / width;

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

static Gfx *spells_draw(Gfx *dl) {
    s32 buffer = D_global_asm_807444FC;
    s32 drawn = FALSE;
    s32 i;

    if (spell_state != SPELL_SPRITE_READY) {
        return dl;
    }
    for (i = 0; i < SPELL_MAX; i++) {
        s32 frame;

        if (!spells[i].live) {
            continue;
        }
        if (!drawn) {
            gDPPipeSync(dl++);
            gSPClearGeometryMode(dl++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN);
            gSPSetGeometryMode(dl++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
            gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
            gDPSetCycleType(dl++, G_CYC_1CYCLE);
            gDPSetTexturePersp(dl++, G_TP_PERSP);
            gDPSetTextureLUT(dl++, G_TT_NONE);
            gDPSetTextureFilter(dl++, G_TF_BILERP);
            gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPSetRenderMode(dl++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
            gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            drawn = TRUE;
        }
        frame = (object_timer / SPELL_TICKS_PER_FRAME + i * 3) % spell_frames;
        spell_billboard(&spell_mtx[buffer][i], spells[i].pos, spell_width[frame]);
        gSPMatrix(dl++, &spell_mtx[buffer][i], G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPLoadTextureTile(dl++, spell_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            spell_width[frame], spell_height[frame],
            0, 0, spell_width[frame] - 1, spell_height[frame] - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPVertex(dl++, spell_vtx[frame], 4, 0);
        gSP2Triangles(dl++, 0, 3, 2, 0, 0, 2, 1, 0);
        gSPPopMatrix(dl++, G_MTX_MODELVIEW);
    }
    if (drawn) {
        gDPPipeSync(dl++);
    }
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// her and her spells once a frame, with the player.
Gfx *jiggy_grunty_boss_draw(Gfx *dl, Actor *actor) {
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over, as after a death): the
    // fight starts over.
    if ((s32)current_map != boss_last_map || object_timer < boss_last_timer) {
        if (boss_arena != NULL) {
            grunty_talk_close();
        }
        boss_last_map = current_map;
        boss_placed = FALSE;
        boss_arena = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(boss_arenas); i++) {
            if (boss_arenas[i].map == current_map) {
                boss_arena = &boss_arenas[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        boss_saved = boss_arena != NULL ? jiggy_save_get(current_file, GRUNTY_SAVE_KEY(GRUNTY_SAVE_BOSS, current_map)) : 0;
        spells_clear();
    }
    if (boss_arena == NULL) {
        boss_last_timer = object_timer;
        return dl;
    }
    // Only once Helm's machine is shut down (and its timer stopped).
    if (!isFlagSet(PERMFLAG_HELM_SHUTDOWN, FLAG_TYPE_PERMANENT)) {
        boss_last_timer = object_timer;
        return dl;
    }
    if (!grunty_ready()) {
        return dl;
    }
    if (spell_state == SPELL_SPRITE_UNLOADED) {
        spells_load();
    }
    if (!boss_placed) {
        boss_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        boss_place(actor);
    }
    frames = object_timer - boss_last_timer;
    boss_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // She waits while the game is paused or someone else's cutscene holds the player.
    if (frames > 0 && !((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !grunty_talk_open() && boss_action < BOSS_BEATEN)) {
        boss_update(actor, frames);
    }
    if (boss_action != BOSS_GONE) {
        dl = grunty_draw(dl, boss_anim, boss_progress, boss_pos, boss_yaw, boss_broom,
                         boss_action == BOSS_CAST && !boss_cast_done, boss_scale);
    }
    return spells_draw(dl);
}
