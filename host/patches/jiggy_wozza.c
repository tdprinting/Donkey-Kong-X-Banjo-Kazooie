// Wozza: Freezeezy Peak's shy walrus, read from the player's Banjo-Kazooie ROM, in Crystal
// Caves, where the two worlds have collided; with a game of hide and seek.
//
// In Banjo-Kazooie Wozza (ASSET_494_MODEL_WOZZA, ACTOR_1F3_WOZZA) hides in his cave and won't
// talk to a bear and a bird; only a walrus gets his Jiggy (JIGGY_32_FP_WOZZA), and before then he
// hops away from Banjo (ASSET_1AE_ANIM_WOZZA_LEAVING, with SFX_3F2_BOING). DK64 has no walrus
// transformation, so here he hides from the Kongs instead: found at his first spot, he talks
// (his portrait, ASSET_835_SPRITE_WOZZA, and his noise, SFX_88_WOZZA_NOISE), hops off and hides
// at the next; found at the third, he gives up and throws the player a Golden Banana (drawn as a
// Jiggy), with the Jiggy jingle (COMUSIC_3D). The reward is jiggy_rewards.c's
// CAVES_REWARD_WOZZA, where he throws it.
//
// His model is one of Banjo-Kazooie's vertex-animated ones (its geo list has no bones), which
// jiggy_bk_model_pose draws in its rest pose, so his hops and his throw are drawn by moving the
// whole model: hops up and away, then gone; a bob for the throw. He turns to face the player.
//
// Talking, in the shared text box (jiggy_caves_talk.c), the player held in the cutscene lock
// through the talk and his hops away (or his throw). Found standing or walking, on the ground,
// close to him. Once the game's done, a repeat comes by jiggy_bottles.c's reminder rule.
//
// Saved (jiggy_save_get/set, CAVES_SAVE_KEY(CAVES_CHARACTER_WOZZA, map)): the spot he's at in the
// low bits, WOZZA_SAVE_* above. Drawn right after the player's own model
// (func_global_asm_80630DCC in patches_draw.c). Without a Banjo-Kazooie ROM nothing is drawn and
// nothing happens.

#include "common_structs.h"
#include "jiggy.h"
#include "jiggy_caves.h"

#define WOZZA_MODEL 0x494    // ASSET_494_MODEL_WOZZA
#define WOZZA_PORTRAIT 0x835 // ASSET_835_SPRITE_WOZZA
// He converts to about 16 KiB (21 bones, but no geo bones: 8 pose commands).
#define WOZZA_BLOCK_MAX 0x4000
#define WOZZA_GFX_MAX 16
#define WOZZA_MTX_MAX 24
#define WOZZA_ANIM_IN_CAVE 0x221 // ASSET_221_ANIM_WOZZA_IN_CAVE (kept for when vertex animation is drawn)

// Banjo-Kazooie sounds and music.
#define SFX_WOZZA_NOISE 0x88 // SFX_88_WOZZA_NOISE: his talking sound, 20000/32767, 1.0
#define SFX_BOING 0x3F2      // SFX_3F2_BOING: each hop away, 24000/32767, 0.8 (at 0.9 through the hop)
#define JINGLE_JIGGY_SPAWN 0x3D // COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767
static const u16 wozza_talk_sfx[] = {SFX_WOZZA_NOISE};
static const CavesSpeaker wozza_speaker = {"WOZZA", WOZZA_PORTRAIT, 8, wozza_talk_sfx, 1, 20000.0f / 32767.0f, 1.0f};

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo. At Banjo-Kazooie's own
// size (1.0) he stood about twice DK's height in game; this brings him to about DK's height.
#define BK_TO_DK64 0.25f
#define WOZZA_SCALE (BK_TO_DK64 * 0.55f)
#define GAME_FPS 30.0f
#define TURN_RATE 0.08f
// He breathes: his height stretches by this much, this often.
#define WOZZA_BREATH 0.025f
#define WOZZA_BREATH_SECONDS 3.5f
// Hopping away: this many hops, each this long, this high and this far, then he's gone.
#define HOP_COUNT 3
#define HOP_SECONDS 0.55f
#define HOP_HEIGHT 14.0f
#define HOP_DISTANCE 22.0f
// Throwing the Golden Banana: a bob this long and this deep, then it appears.
#define THROW_SECONDS 1.4f
#define THROW_BOB 6.0f

// Found when the player comes this close (his size plus a little), within this height; a
// repeat needs the player to have been this far off since, and comes at most this often.
#define FIND_RADIUS 75.0f
#define FIND_HEIGHT 45.0f
#define REARM_RADIUS 250.0f
#define REPEAT_FRAMES (60 * 30)
#define PLAYER_AIRBORNE_HEIGHT 6.0f
#define FLOOR_PROBE 40.0f
#define FLOOR_MAX_STEP 60.0f
#define REWARD_HEIGHT 6.0f

#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D

#define WOZZA_SPOTS 3
// Where he hides, map by map, in order, and where his Golden Banana lands. Crystal Caves: on the
// north-east shelf past the slope up from the entrance (y 153), clear of its notes; out west on
// the round ledge by the door at (300, 2428) (y 180); then south of the middle, under the ice
// arch's south end (y 115), with the Golden Banana a few steps toward the middle. All are ground
// DK64's own bananas and coins stand on.
typedef struct {
    Maps map;
    f32 spot[WOZZA_SPOTS][3];
    f32 reward[3];
} WozzaPlacement;
static const WozzaPlacement wozza_maps[] = {
    {MAP_CAVES,
     {{2650.0f, 153.0f, 850.0f}, {380.0f, 180.0f, 2380.0f}, {1500.0f, 115.0f, 1800.0f}},
     {1540.0f, 106.0f, 1760.0f}},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// What he says at each spot. Each page is one press of A; lines are broken to fit the box.
static const char *const wozza_found_1[] = {
    "EEK! A MONKEY! ...OH. HELLO. I'M WOZZA.",
    "I DON'T LIKE VISITORS. I'M VERY SHY, YOU KNOW.",
    "I'LL JUST GO AND HIDE SOMEWHERE ELSE. SOMEWHERE QUIET, OUT WEST. DON'T FOLLOW ME!",
};
static const char *const wozza_found_2[] = {
    "OH NO, NOT YOU AGAIN! CAN'T A WALRUS HAVE A LITTLE PEACE?",
    "RIGHT, I'M OFF TO THE MIDDLE OF THE CAVES, UNDER THE BIG ICE ARCH. YOU'LL NEVER LOOK THERE.",
};
static const char *const wozza_found_3[] = {
    "WHAT?! HOW DO YOU KEEP FINDING ME?",
    "ALL RIGHT, ALL RIGHT, YOU WIN. TAKE THIS JIGGY AND LEAVE A POOR WALRUS ALONE!",
};
static const char *const wozza_after[] = {
    "YOU AGAIN? I GAVE YOU THE JIGGY ALREADY. SHOO! GO ON!",
};
static const char *const *const wozza_found[WOZZA_SPOTS] = {wozza_found_1, wozza_found_2, wozza_found_3};
static const s32 wozza_found_count[WOZZA_SPOTS] = {
    ARRAY_COUNT_OF(wozza_found_1), ARRAY_COUNT_OF(wozza_found_2), ARRAY_COUNT_OF(wozza_found_3)};

// Save keys: the spot he's at (0 to 2) in the low bits; then these.
#define WOZZA_SAVE_KEY(map) CAVES_SAVE_KEY(CAVES_CHARACTER_WOZZA, map)
#define WOZZA_SAVE_SPOT_MASK 0x3
#define WOZZA_SAVE_DONE 0x4 // He's thrown the Jiggy.

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u8 D_global_asm_8076A0B1;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    WOZZA_UNLOADED,
    WOZZA_READY,
    WOZZA_UNAVAILABLE,
} WozzaLoadState;

typedef enum {
    WOZZA_WAITING,  // At his spot, until found.
    WOZZA_TALKING,
    WOZZA_HOPPING,  // Off to the next spot.
    WOZZA_THROWING, // The Golden Banana, at the last.
} WozzaAction;

static u8 wozza_block[WOZZA_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx wozza_gfx[2][WOZZA_GFX_MAX];
static Mtx wozza_mtx[2][WOZZA_MTX_MAX];
static JiggyBkPose wozza_pose;
static WozzaLoadState wozza_state = WOZZA_UNLOADED;

static s32 wozza_last_map = -1;
static u32 wozza_last_timer;
static const WozzaPlacement *wozza_map;
static s32 wozza_placed;
static u32 wozza_saved;
static WozzaAction wozza_action;
static f32 wozza_pos[3];
static f32 wozza_yaw;
static f32 wozza_breath;
static f32 wozza_timer;
static f32 wozza_lift;   // Drawn this high over his spot.
static f32 wozza_shrink; // And this much smaller (gone at 1).
static f32 hop_from[3];
static s32 wozza_armed;
static u32 wozza_last_talk;
static f32 reward_spot[3];
static s32 reward_due;

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

static f32 wozza_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < FLOOR_MAX_STEP && y - floor < FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

static void wozza_load(void) {
    s32 size;

    wozza_state = WOZZA_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(WOZZA_MODEL, wozza_block, sizeof(wozza_block));
    if (size > 0 && size <= (s32)sizeof(wozza_block)) {
        wozza_state = WOZZA_READY;
    }
}

static s32 wozza_spot_index(void) {
    s32 spot = wozza_saved & WOZZA_SAVE_SPOT_MASK;

    return spot < WOZZA_SPOTS ? spot : WOZZA_SPOTS - 1;
}

static void wozza_save(u32 value) {
    wozza_saved = value;
    jiggy_save_set(current_file, WOZZA_SAVE_KEY(current_map), wozza_saved);
}

// Stands him at the spot he's saved at.
static void wozza_stand(Actor *player) {
    const f32 *spot = wozza_map->spot[wozza_spot_index()];

    wozza_pos[0] = spot[0];
    wozza_pos[1] = wozza_floor(spot[0], spot[1], spot[2]);
    wozza_pos[2] = spot[2];
    wozza_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, wozza_pos[0], wozza_pos[2]) & 0xFFF;
    wozza_lift = 0.0f;
    wozza_shrink = 0.0f;
    wozza_action = WOZZA_WAITING;
}

static void wozza_place(Actor *player) {
    wozza_stand(player);
    reward_spot[0] = wozza_map->reward[0];
    reward_spot[1] = wozza_floor(wozza_map->reward[0], wozza_map->reward[1], wozza_map->reward[2]);
    reward_spot[2] = wozza_map->reward[2];
    reward_due = (wozza_saved & WOZZA_SAVE_DONE) != 0;
    wozza_armed = FALSE;
    wozza_last_talk = object_timer - REPEAT_FRAMES;
    wozza_placed = TRUE;
}

static s32 wozza_player_near(Actor *player, f32 radius) {
    f32 dy = wozza_pos[1] - player->y_position;

    return distance_xz(wozza_pos[0], wozza_pos[2], player->x_position, player->z_position) <= radius && dy >= -FIND_HEIGHT &&
           dy <= FIND_HEIGHT;
}

static void turn_to(f32 *yaw, const f32 pos[3], f32 x, f32 z, f32 rate, u32 frames) {
    s16 target = func_global_asm_80665DE0(x, z, pos[0], pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - *yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        *yaw += turn * rate;
        if (*yaw < 0.0f) {
            *yaw += 4096.0f;
        } else if (*yaw >= 4096.0f) {
            *yaw -= 4096.0f;
        }
    }
}

// Found (or stopped by, once it's over): he talks.
static void wozza_check_found(Actor *player) {
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;
    s32 standing = player->control_state == STATE_IDLE;
    s32 walking = player->control_state == STATE_WALK;
    s32 done = (wozza_saved & WOZZA_SAVE_DONE) != 0;
    s32 started;
    s16 to_wozza;

    if (!wozza_armed) {
        wozza_armed = !wozza_player_near(player, REARM_RADIUS) && object_timer - wozza_last_talk >= REPEAT_FRAMES;
    }
    if (!wozza_player_near(player, FIND_RADIUS) || !grounded || !(standing || walking)) {
        return;
    }
    if (done) {
        if (!wozza_armed || !standing) {
            return;
        }
        started = jiggy_caves_talk_start(&wozza_speaker, wozza_after, ARRAY_COUNT_OF(wozza_after), FALSE);
    } else {
        s32 spot = wozza_spot_index();

        // Held after the talk, through his hops away or his throw.
        started = jiggy_caves_talk_start(&wozza_speaker, wozza_found[spot], wozza_found_count[spot], TRUE);
        if (!started) {
            // No portrait: he goes without a word.
            jiggy_caves_lock_player(TRUE);
            started = TRUE;
        }
    }
    if (!started) {
        return;
    }
    wozza_action = WOZZA_TALKING;
    wozza_armed = FALSE;
    wozza_last_talk = object_timer;
    to_wozza = func_global_asm_80665DE0(wozza_pos[0], wozza_pos[2], player->x_position, player->z_position);
    player->y_rotation = to_wozza & 0xFFF;
    player->unkEE = to_wozza & 0xFFF;
}

// The talk's over: he hops off, or throws the Golden Banana, or (once it's over) just stays.
static void wozza_talk_done(void) {
    if (wozza_saved & WOZZA_SAVE_DONE) {
        wozza_action = WOZZA_WAITING;
        return;
    }
    wozza_timer = 0.0f;
    if (wozza_spot_index() == WOZZA_SPOTS - 1) {
        wozza_action = WOZZA_THROWING;
        return;
    }
    wozza_action = WOZZA_HOPPING;
    hop_from[0] = wozza_pos[0];
    hop_from[1] = wozza_pos[1];
    hop_from[2] = wozza_pos[2];
}

static s32 passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

static void wozza_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;

    if (frames == 0) {
        return;
    }
    wozza_breath += seconds / WOZZA_BREATH_SECONDS;
    while (wozza_breath >= 1.0f) {
        wozza_breath -= 1.0f;
    }
    switch (wozza_action) {
        case WOZZA_WAITING:
            turn_to(&wozza_yaw, wozza_pos, player->x_position, player->z_position, TURN_RATE, frames);
            if (!jiggy_caves_talk_is_open() && !(D_global_asm_8076A0B1 & CAVES_CUTSCENE_LOCK)) {
                wozza_check_found(player);
            }
            break;
        case WOZZA_TALKING:
            turn_to(&wozza_yaw, wozza_pos, player->x_position, player->z_position, TURN_RATE, frames);
            if (!jiggy_caves_talk_is_open() || jiggy_caves_talk_update()) {
                wozza_talk_done();
            }
            break;
        case WOZZA_HOPPING: {
            f32 before = wozza_timer;
            f32 hop;
            f32 within;
            s32 i;

            jiggy_caves_lock_player(TRUE);
            if (wozza_timer == 0.0f) {
                // He turns away from the player to go.
                wozza_yaw = (func_global_asm_80665DE0(player->x_position, player->z_position, wozza_pos[0], wozza_pos[2]) +
                             2048) & 0xFFF;
            }
            wozza_timer += seconds;
            for (i = 0; i < HOP_COUNT; i++) {
                if (passed(before / HOP_SECONDS, wozza_timer / HOP_SECONDS, i + 0.9f)) {
                    jiggy_bk_sfx_play(SFX_BOING, 24000.0f / 32767.0f, 0.8f);
                }
            }
            hop = wozza_timer / HOP_SECONDS;
            if (hop >= HOP_COUNT) {
                // Gone: he waits at the next spot, and the player can move again.
                wozza_save((wozza_saved & ~WOZZA_SAVE_SPOT_MASK) | (wozza_spot_index() + 1));
                wozza_stand(player);
                // He's somewhere new, but out of sight; he shows up when the player gets there.
                jiggy_caves_lock_player(FALSE);
                wozza_last_talk = object_timer;
                break;
            }
            within = hop - (s32)hop;
            wozza_lift = HOP_HEIGHT * 4.0f * within * (1.0f - within);
            wozza_pos[0] = hop_from[0] + func_global_asm_80612794((s16)wozza_yaw) * HOP_DISTANCE * hop;
            wozza_pos[2] = hop_from[2] + func_global_asm_80612790((s16)wozza_yaw) * HOP_DISTANCE * hop;
            wozza_pos[1] = wozza_floor(wozza_pos[0], hop_from[1], wozza_pos[2]);
            // The last hop takes him out of sight.
            wozza_shrink = hop > HOP_COUNT - 1 ? hop - (HOP_COUNT - 1) : 0.0f;
            break;
        }
        case WOZZA_THROWING:
            jiggy_caves_lock_player(TRUE);
            turn_to(&wozza_yaw, wozza_pos, player->x_position, player->z_position, TURN_RATE, frames);
            wozza_timer += seconds;
            wozza_lift = -THROW_BOB * func_global_asm_80612794((s16)(wozza_timer / THROW_SECONDS * 2048.0f));
            if (wozza_timer >= THROW_SECONDS) {
                wozza_lift = 0.0f;
                wozza_save(wozza_saved | WOZZA_SAVE_DONE);
                reward_due = TRUE;
                jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
                jiggy_caves_lock_player(FALSE);
                wozza_action = WOZZA_WAITING;
                wozza_last_talk = object_timer;
            }
            break;
    }
    if (reward_due) {
        if (jiggy_reward_update(CAVES_REWARD_WOZZA, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], player,
                                frames)) {
            reward_due = FALSE;
        }
    }
}

static Gfx *wozza_draw_model(Gfx *dl, s32 buffer) {
    f32 rotate[4][4];
    f32 size[4][4];
    f32 scale = WOZZA_SCALE * (1.0f - wozza_shrink);
    f32 stretch = 1.0f + WOZZA_BREATH * func_global_asm_80612794((s16)(wozza_breath * 4096.0f));
    s32 i;

    if (scale <= 0.001f) {
        return dl;
    }
    guScaleF(size, scale, scale * stretch, scale);
    guRotateF(rotate, wozza_yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(size, rotate, wozza_pose.world);
    wozza_pose.world[3][0] = wozza_pos[0];
    wozza_pose.world[3][1] = wozza_pos[1] + wozza_lift;
    wozza_pose.world[3][2] = wozza_pos[2];
    // Branch 1 of selector 1 (his eyes open), the rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        wozza_pose.selectors[i] = 0;
    }
    wozza_pose.selectors[1] = 1;
    wozza_pose.model = WOZZA_MODEL;
    wozza_pose.block = wozza_block;
    wozza_pose.animation = WOZZA_ANIM_IN_CAVE;
    wozza_pose.progress = wozza_breath > 0.9999f ? 0.9999f : wozza_breath;
    wozza_pose.gfx = wozza_gfx[buffer];
    wozza_pose.gfx_capacity = WOZZA_GFX_MAX;
    wozza_pose.mtx = wozza_mtx[buffer];
    wozza_pose.mtx_capacity = WOZZA_MTX_MAX;
    if (jiggy_bk_model_pose(&wozza_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, wozza_gfx[buffer]);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// Wozza once a frame, with the player.
Gfx *jiggy_wozza_draw(Gfx *dl, Actor *actor) {
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != wozza_last_map || object_timer < wozza_last_timer) {
        if (wozza_placed && wozza_action != WOZZA_WAITING) {
            jiggy_caves_talk_cancel();
            jiggy_caves_lock_player(FALSE);
        }
        wozza_last_map = current_map;
        wozza_placed = FALSE;
        wozza_map = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(wozza_maps); i++) {
            if (wozza_maps[i].map == current_map) {
                wozza_map = &wozza_maps[i];
            }
        }
        wozza_saved = wozza_map != NULL ? jiggy_save_get(current_file, WOZZA_SAVE_KEY(current_map)) : 0;
    }
    if (wozza_map == NULL) {
        wozza_last_timer = object_timer;
        return dl;
    }
    if (wozza_state == WOZZA_UNLOADED) {
        wozza_load();
    }
    if (wozza_state != WOZZA_READY) {
        return dl;
    }
    if (!wozza_placed) {
        wozza_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        wozza_place(actor);
    }
    frames = object_timer - wozza_last_timer;
    wozza_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CAVES_CUTSCENE_LOCK) && wozza_action == WOZZA_WAITING) {
        frames = 0;
    }
    wozza_update(actor, frames);
    return wozza_draw_model(dl, D_global_asm_807444FC);
}
