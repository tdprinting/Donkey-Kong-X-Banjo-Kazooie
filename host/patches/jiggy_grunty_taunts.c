// Gruntilda's taunts: as she does from afar in Banjo-Kazooie's lair, Gruntilda cackles a rhyme
// when the player comes into a new part of DK Isles, now that her lair has moved in. Each lobby
// entrance (and the Training Grounds' tunnel, K. Lumsy's island, the Banana Fairy's island and
// Snide's door) has its own rhyme the first time; after that, coming back gets one of her
// all-purpose ones, now and then.
//
// The taunt plays in jiggy_talk.c's box with her portrait (ASSET_816_SPRITE_GRUNTILDA) and
// voice (SFX_EC to SFX_EE_GRUNTY_TALKING), after one of her laughs (SFX_EA/EB_GRUNTY_LAUGH). It
// doesn't hold the player or read A (Banjo-Kazooie's lair taunts play while Banjo moves on);
// each one stays up a moment and goes. She waits for Banjo's portrait (jiggy_cameo.c), any
// other box and any cutscene, and leaves a gap between taunts. Which rhymes have been heard is
// kept with the save file (key 0x57020000 | map). No model: she's only heard.
//
// Updated with the player's draw (func_global_asm_80630DCC in patches_draw.c). Without a
// Banjo-Kazooie ROM nothing happens.

#include "common_structs.h"
#include "jiggy.h"
#include "jiggy_talk.h"

#define GRUNTY_PORTRAIT 0x816 // ASSET_816_SPRITE_GRUNTILDA
static const u16 grunty_talk_sfx[] = {0xEC, 0xED, 0xEE}; // SFX_EC/ED/EE_GRUNTY_TALKING
static const u16 grunty_laughs[] = {0xEA, 0xEB};         // SFX_EA/EB_GRUNTY_LAUGH
static const JiggyTalker grunty_talker = {
    GRUNTY_PORTRAIT, 6, "GRUNTILDA", grunty_talk_sfx, 3, 22000.0f / 32767.0f, 1.0f,
};

// Game frames between taunts, and the chance (in 256) of a repeat taunt on coming back.
#define TAUNT_GAP_FRAMES (30 * 20)
#define TAUNT_REPEAT_CHANCE 96
// Zones are entered within this height of their middle.
#define ZONE_HEIGHT 120.0f
#define CUTSCENE_LOCK 0x10
#define TAUNT_SAVE_MAP_KEY(map) (0x57020000 | (u32)(map))

typedef struct {
    f32 x, y, z, radius;
    const char *rhyme;
} TauntZone;

// DK Isles: around the loading zones of the lobbies and the rest (the map's own trigger list),
// each zone well clear of the next.
static const TauntZone taunts_isles[] = {
    {2700.0f, 500.0f, 2230.0f, 200.0f, "SO THE BEAR AND APE HAVE COME TO PLAY? MY ISLES WILL BE YOUR GRAVE TODAY!"},
    {3219.0f, 530.0f, 1759.0f, 170.0f, "THAT JUNGLE'S WHERE YOUR CHUMS ALL HIDE, BUT GRUBLINS WAIT FOR YOU INSIDE!"},
    {3515.0f, 1039.0f, 1737.0f, 150.0f, "THE TEMPLE'S HOT, THE SAND IS DEEP, YOU'LL SOON BE IN A SANDY HEAP!"},
    {2633.0f, 1000.0f, 1499.0f, 150.0f, "IN CAVES OF ICE YOU'LL SHAKE AND FREEZE, YOUR KNOBBLY KNEES WILL START TO SNEEZE!"},
    {2507.0f, 1496.0f, 861.0f, 150.0f, "UP SO HIGH, YOU FOOLISH PAIR? THE FOREST'S TREES WILL GIVE YOU A SCARE!"},
    {1994.0f, 370.0f, 3339.0f, 180.0f, "INTO THE WATER, DOWN YOU GO! MY SHARKS ARE HUNGRY, DON'T YOU KNOW!"},
    {2446.0f, 1260.0f, 3511.0f, 150.0f, "MY FACTORY HUMS WITH GEARS AND STEAM. YOU'LL NEVER WIN, SO DON'T EVEN DREAM!"},
    {2457.0f, 1610.0f, 3453.0f, 150.0f, "THE TOP OF KREM ISLE, WHAT A VIEW! THE LAST THING THAT YOU'LL SEE, IT'S TRUE!"},
    {3395.0f, 500.0f, 3331.0f, 220.0f, "THAT GREAT BIG OAF IS LOCKED UP TIGHT. I'LL PUT YOU IN THERE TOO TONIGHT!"},
    {1000.0f, 500.0f, 2430.0f, 280.0f, "A FAIRY'S CAMERA WON'T HELP YOU. I'LL SNAP THE PAIR OF YOU IN TWO!"},
    {2582.0f, 900.0f, 3353.0f, 150.0f, "THAT WEASEL'S BLUEPRINTS WON'T SAVE YOUR HIDE. YOU'LL SOON BE TRAPPED WITH HIM INSIDE!"},
};
static const char *const taunts_repeat[] = {
    "BACK AGAIN, YOU FURRY FOOL? YOUR CLUMSY TRICKS ARE NOT SO COOL!",
    "YOU MAY LOOK BRAVE, YOU MAY LOOK STRONG, BUT TRUST ME, YOU WON'T LAST VERY LONG!",
    "THAT APE IS BIG, THAT BEAR IS SLOW. TOGETHER YOU'RE A RUBBISH SHOW!",
    "HEAR THAT CACKLE? THAT'S ME, GRUNTY! YOU'LL BE MY SUPPER, NICE AND CRUNCHY!",
};
static const struct {
    Maps map;
    const TauntZone *zones;
    u32 count;
} taunt_maps[] = {
    {MAP_DK_ISLES_OVERWORLD, taunts_isles, sizeof(taunts_isles) / sizeof(taunts_isles[0])},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))
#define ZONES_MAX 32

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u32 object_timer;
extern u8 current_file;
extern u8 D_global_asm_8076A0B1;

static s32 taunt_last_map = -1;
static u32 taunt_last_timer;
static const TauntZone *taunt_zones;
static u32 taunt_zone_count;
static u32 taunt_heard;  // Bit i: zone i's own rhyme has been heard (saved).
static u32 taunt_inside; // Bit i: the player was in zone i last frame.
static s32 taunt_ready;  // The player has stood on the map (zones they start in don't count).
static u32 taunt_wait;   // Frames before the next taunt may come.
static u32 taunt_random_state = 0x47524E54;
static const char *taunt_page[1];

static u32 taunt_random(void) {
    taunt_random_state = taunt_random_state * 1664525 + 1013904223;
    return taunt_random_state >> 8;
}

// The zones the player is in now.
static u32 taunt_zones_in(Actor *player) {
    u32 in = 0;
    u32 i;

    for (i = 0; i < taunt_zone_count && i < ZONES_MAX; i++) {
        const TauntZone *zone = &taunt_zones[i];
        f32 dx = player->x_position - zone->x;
        f32 dz = player->z_position - zone->z;
        f32 dy = player->y_position - zone->y;

        if (dx * dx + dz * dz <= zone->radius * zone->radius && dy <= ZONE_HEIGHT && dy >= -ZONE_HEIGHT) {
            in |= 1u << i;
        }
    }
    return in;
}

static s32 taunt_say(const char *rhyme) {
    taunt_page[0] = rhyme;
    if (!jiggy_talk_say(JIGGY_TALK_OWNER_GRUNTY, &grunty_talker, taunt_page, 1, JIGGY_TALK_AUTO)) {
        return FALSE;
    }
    jiggy_bk_sfx_play(grunty_laughs[taunt_random() % ARRAY_COUNT_OF(grunty_laughs)], 26000.0f / 32767.0f, 1.0f);
    taunt_wait = TAUNT_GAP_FRAMES;
    return TRUE;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c): watches the
// player cross into the map's zones, once a frame.
Gfx *jiggy_grunty_taunts_draw(Gfx *dl, Actor *actor) {
    u32 frames;
    u32 in;
    u32 entered;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != taunt_last_map || object_timer < taunt_last_timer) {
        jiggy_talk_close(JIGGY_TALK_OWNER_GRUNTY);
        taunt_last_map = current_map;
        taunt_zones = NULL;
        taunt_zone_count = 0;
        taunt_ready = FALSE;
        for (i = 0; i < ARRAY_COUNT_OF(taunt_maps); i++) {
            if (taunt_maps[i].map == current_map) {
                taunt_zones = taunt_maps[i].zones;
                taunt_zone_count = taunt_maps[i].count;
            }
        }
        taunt_heard = taunt_zones != NULL ? jiggy_save_get(current_file, TAUNT_SAVE_MAP_KEY(current_map)) : 0;
    }
    frames = object_timer - taunt_last_timer;
    taunt_last_timer = object_timer;
    if (taunt_zones == NULL || !jiggy_bk_available()) {
        return dl;
    }
    if (frames > 4) {
        frames = 1;
    }
    jiggy_talk_update();
    in = taunt_zones_in(actor);
    if (!taunt_ready) {
        // Zones the player arrives in count as entered once they've stood on the ground.
        if (actor->y_position - actor->floor > 6.0f) {
            return dl;
        }
        taunt_ready = TRUE;
        taunt_inside = 0;
        // The player's first sight of the Isles comes with Banjo's portrait; Grunty speaks after.
        taunt_wait = 60;
    }
    taunt_wait = taunt_wait > frames ? taunt_wait - frames : 0;
    entered = in & ~taunt_inside;
    // A zone entered while she can't speak is taken up once she can, while the player's still in it.
    if (taunt_wait > 0 || jiggy_talk_open(-1) || jiggy_cameo_showing() || (D_global_asm_8076A0B1 & CUTSCENE_LOCK)) {
        taunt_inside &= in;
        return dl;
    }
    taunt_inside = in;
    if (entered == 0) {
        return dl;
    }
    for (i = 0; i < taunt_zone_count && i < ZONES_MAX; i++) {
        if (!(entered & (1u << i))) {
            continue;
        }
        if (!(taunt_heard & (1u << i))) {
            if (taunt_say(taunt_zones[i].rhyme)) {
                taunt_heard |= 1u << i;
                jiggy_save_set(current_file, TAUNT_SAVE_MAP_KEY(current_map), taunt_heard);
            }
        } else if ((taunt_random() & 0xFF) < TAUNT_REPEAT_CHANCE) {
            taunt_say(taunts_repeat[taunt_random() % ARRAY_COUNT_OF(taunts_repeat)]);
        } else {
            // Not this time; she'll be back.
            taunt_wait = TAUNT_GAP_FRAMES / 2;
        }
        break;
    }
    return dl;
}
