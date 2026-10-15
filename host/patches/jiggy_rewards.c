// Golden Banana rewards (drawn as Jiggies by jiggy_collectibles.c) without a DK64 flag each.
//
// DK64 ties every Golden Banana to a permanent flag, and only a handful past the game's own
// (0x316 up) look free, too few for a whole game's worth of quests. So a reward here is our own
// id (0-255), and whether it's been collected lives in jiggy_save.bin (key 0x4B000000 | id).
// While it's due, its Golden Banana is spawned as jiggy_conga.c spawns his (an
// ACTOR_GOLDEN_BANANA from func_global_asm_806A5DF0) on a borrowed scratch flag: when the player
// picks it up DK64 counts it as usual (the Kong's golden_bananas[level] goes up and is saved with
// the file) and sets the scratch flag; we then record the reward as collected and clear the
// flag for the next one. Scratch flags left set (the game closed mid-pickup) are cleared too.
//
// Callers (an NPC's quest, a Jinjo set, a boss) call jiggy_reward_update every frame while the
// reward is due; it spawns, respawns and records it. jiggy_reward_collected says if it's done.
//
// Ids are allocated in docs/GAME_PLAN.md (and project memory's dk64-flag-allocations).

#include "patches.h"
#include "common_structs.h"
#include "jiggy.h"

#define REWARD_GOLDEN_BANANA 0x2D // ACTOR_GOLDEN_BANANA
#define REWARD_SPAWNER_TYPE (REWARD_GOLDEN_BANANA - 0x10)
#define FLAG_TYPE_PERMANENT 0
#define REWARD_SAVE_KEY(id) (0x4B000000u | ((id) & 0xFFu))
// Scratch flags, from the free ones past DK64's own (see the memory note above): one per
// reward on screen at once.
#define SCRATCH_FIRST 0x316
#define SCRATCH_COUNT 3
// Don't spawn it on top of the player (they'd pick it up before seeing it), and wait this long
// after it's gone before spawning it again.
#define REWARD_CLEARANCE 30.0f
#define REWARD_RESPAWN_FRAMES 90

typedef struct {
    s32 id;                // The reward on this flag, or -1.
    ActorSpawner *spawner; // Its Golden Banana's spawner while it's out.
    s32 wait;              // Frames before spawning it again.
    s32 seen;              // Updated this frame (callers stop when the reward isn't due).
} RewardSlot;

extern u8 current_file;
extern s16 current_map;
extern ActorSpawner *actor_spawner_pointer;
u8 isFlagSet(s16 flagIndex, u8 flagType);
void setFlag(s16 flagIndex, u8 newValue, u8 flagType);
void func_global_asm_806A5DF0(s16 actor, f32 x, f32 y, f32 z, s16 angle, u8 control_state, s16 flag, s32 arg7);

static RewardSlot slots[SCRATCH_COUNT];
static s32 slots_map = -1;
static s32 slots_file = -1;

static f32 reward_distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;
    return _sqrtf(dx * dx + dz * dz);
}

static s32 reward_spawner_alive(ActorSpawner *target) {
    ActorSpawner *spawner;

    for (spawner = actor_spawner_pointer; spawner != NULL; spawner = spawner->next_spawner) {
        if (spawner == target) {
            return spawner->actor_type == REWARD_SPAWNER_TYPE;
        }
    }
    return FALSE;
}

// Records rewards whose scratch flag DK64 set (picked up), and starts afresh on a new map or
// file (the spawners went with the map). A set flag with no reward on it is cleared.
static void rewards_refresh(void) {
    s32 i;

    if (slots_map != current_map || slots_file != current_file) {
        for (i = 0; i < SCRATCH_COUNT; i++) {
            if (isFlagSet(SCRATCH_FIRST + i, FLAG_TYPE_PERMANENT)) {
                if (slots[i].id >= 0 && slots_file == current_file) {
                    jiggy_save_set(current_file, REWARD_SAVE_KEY(slots[i].id), 1);
                }
                setFlag(SCRATCH_FIRST + i, FALSE, FLAG_TYPE_PERMANENT);
            }
            slots[i].id = -1;
            slots[i].spawner = NULL;
            slots[i].wait = 0;
        }
        slots_map = current_map;
        slots_file = current_file;
        return;
    }
    for (i = 0; i < SCRATCH_COUNT; i++) {
        if (isFlagSet(SCRATCH_FIRST + i, FLAG_TYPE_PERMANENT)) {
            if (slots[i].id >= 0) {
                jiggy_save_set(current_file, REWARD_SAVE_KEY(slots[i].id), 1);
            }
            setFlag(SCRATCH_FIRST + i, FALSE, FLAG_TYPE_PERMANENT);
            slots[i].id = -1;
            slots[i].spawner = NULL;
        }
    }
}

s32 jiggy_reward_collected(u32 id) {
    rewards_refresh();
    return jiggy_save_get(current_file, REWARD_SAVE_KEY(id)) != 0;
}

// Spawns reward `id`'s Golden Banana at (x, y, z) while it isn't collected; call every frame
// (`frames` game frames on) while it's due. Returns TRUE once it's collected.
s32 jiggy_reward_update(u32 id, f32 x, f32 y, f32 z, void *player_actor, u32 frames) {
    Actor *player = player_actor;
    RewardSlot *slot = NULL;
    s32 i;

    if (jiggy_reward_collected(id)) {
        return TRUE;
    }
    for (i = 0; i < SCRATCH_COUNT; i++) {
        if (slots[i].id == (s32)id) {
            slot = &slots[i];
            break;
        }
    }
    if (slot == NULL) {
        for (i = 0; i < SCRATCH_COUNT; i++) {
            // A slot whose reward stopped being due (its spawner gone) can be taken back.
            if (slots[i].id < 0 || (slots[i].spawner != NULL && !reward_spawner_alive(slots[i].spawner))) {
                slot = &slots[i];
                slot->id = id;
                slot->spawner = NULL;
                slot->wait = 0;
                break;
            }
        }
        if (slot == NULL) {
            return FALSE; // All borrowed: it'll come out once one's picked up.
        }
    }
    if (slot->spawner != NULL) {
        if (reward_spawner_alive(slot->spawner)) {
            return FALSE;
        }
        slot->spawner = NULL;
        slot->wait = REWARD_RESPAWN_FRAMES;
    }
    if (slot->wait > 0) {
        slot->wait -= frames;
        return FALSE;
    }
    if (player != NULL && reward_distance_xz(player->x_position, player->z_position, x, z) < REWARD_CLEARANCE &&
        player->y_position > y - REWARD_CLEARANCE && player->y_position < y + REWARD_CLEARANCE) {
        return FALSE;
    }
    func_global_asm_806A5DF0(REWARD_GOLDEN_BANANA, x, y, z, 0, 0, SCRATCH_FIRST + (slot - slots), 0);
    // func_global_asm_80689250 puts the new spawner at the head of the list.
    slot->spawner = actor_spawner_pointer;
    return FALSE;
}
