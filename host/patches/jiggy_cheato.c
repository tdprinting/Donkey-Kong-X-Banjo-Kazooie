// Cheato: Banjo-Kazooie's talking spellbook, read from the player's Banjo-Kazooie ROM, hovering
// on DK Isles' north beach, where Gruntilda's Lair has moved in.
//
// In Banjo-Kazooie he hides in the lair and gives cheats (src/lair/ch/cheato.c). Here Gruntilda
// has torn three pages out of him and scattered them over the Isles: one along the main
// island's east beach, one on Krem Isle's east shore and one on the Banana Fairy's island, all
// on ground reached on foot or by swimming. Touching a page collects it, with Banjo-Kazooie's
// ding (COMUSIC_2B_DING_B) and a word from Cheato in a box that doesn't stop the player. With
// all three, talking to him puts them back: he flaps and spins with his tune (COMUSIC_79_CHEATO)
// while the player is held (the game's cutscene lock), then says his piece and a Jiggy (reward
// 0x02, jiggy_rewards.c) appears beside him with Banjo-Kazooie's Jiggy jingle.
//
// The pages aren't Banjo-Kazooie art (it has no loose page): each is a small parchment-colored
// sheet with ruled lines, drawn here as a few shaded triangles, turning and bobbing.
//
// Cheato (ASSET_54F_MODEL_CHEATO, with branches 3 and 4 shown as chCheato's draw shows them)
// flaps with ASSET_272 and turns to face the player; he talks in jiggy_talk.c's box with his
// portrait (ASSET_842_SPRITE_CHEATO) and voice (SFX_149/14A_CHEATO_TALKING). Progress is kept
// with the save file (key 0x57010000 | map, CHEATO_SAVE_* below). Drawn right after the
// player's own model (func_global_asm_80630DCC in patches_draw.c). Without a Banjo-Kazooie ROM
// nothing is drawn and nothing happens.

#include "common_structs.h"
#include "jiggy.h"
#include "jiggy_talk.h"

#define CHEATO_MODEL 0x54F    // ASSET_54F_MODEL_CHEATO
#define CHEATO_PORTRAIT 0x842 // ASSET_842_SPRITE_CHEATO
// He converts to 27240 bytes, with 30 bones and 77 pose commands.
#define CHEATO_BLOCK_MAX 0x6B00
#define CHEATO_GFX_MAX 112
#define CHEATO_MTX_MAX 32
#define CHEATO_ANIM_FLAP 0x272 // ASSET_272_ANIM_unk, chCheato's only animation
#define CHEATO_FLAP_SECONDS 0.8f
// Putting the pages back: how long, and how much faster he flaps and turns meanwhile.
#define CHEATO_MEND_SECONDS 3.0f
#define CHEATO_MEND_FLAP 3.0f
#define CHEATO_MEND_SPIN 4096.0f // 12-bit angle units a second.

// Banjo-Kazooie sounds and music.
static const u16 cheato_talk_sfx[] = {0x149, 0x14A}; // SFX_149/14A_CHEATO_TALKING
#define CHEATO_SFX_IDLE 0xF0      // SFX_F0_CHEATO_IDLE_1, as he notices the player
#define JINGLE_CHEATO 0x79        // COMUSIC_79_CHEATO
#define JINGLE_PAGE 0x2B          // COMUSIC_2B_DING_B
#define JINGLE_JIGGY_SPAWN 0x3D   // COMUSIC_3D_JIGGY_SPAWN

#define BK_TO_DK64 0.25f
#define GAME_FPS 30.0f
// He hovers over the floor (his model already floats 47 units up), bobbing this much.
#define CHEATO_HOVER 6.0f
#define CHEATO_BOB 3.0f
#define CHEATO_BOB_SECONDS 2.0f
#define CHEATO_TURN_RATE 0.2f
#define CHEATO_LOOK_RADIUS 250.0f
// Talking (as Bottles): this close, on the ground; not again until the player has walked this
// far off; a reminder at most once a minute.
#define CHEATO_REMIND_FRAMES (60 * 30)
#define CHEATO_TALK_RADIUS 70.0f
#define CHEATO_REARM_RADIUS 220.0f
#define CHEATO_TALK_FEET 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
#define CHEATO_FLOOR_PROBE 40.0f
#define CHEATO_FLOOR_MAX_STEP 60.0f
#define REWARD_HEIGHT 12.5f
#define REWARD_ID 0x02

// The pages: how high they float, how they turn and bob, and how close the player has to come
// (as a note's touch, jiggy_notes.c).
#define PAGE_HOVER 18.0f
#define PAGE_BOB 3.0f
#define PAGE_SPIN 1400.0f // 12-bit angle units a second.
#define PAGE_TOUCH_RADIUS 26.0f
#define PAGE_TOUCH_BELOW 12.0f
#define PAGE_TOUCH_ABOVE 55.0f
#define PAGE_COUNT 3
#define PAGE_VTX 28

#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
#define CUTSCENE_LOCK 0x10

// Save key and bits.
#define CHEATO_SAVE_MAP_KEY(map) (0x57010000 | (u32)(map))
#define CHEATO_SAVE_PAGES 0x7 // One bit a page.
#define CHEATO_SAVE_MET 0x8
#define CHEATO_SAVE_MENDED 0x10

// Where he and his pages are, map by map (ground at about y 500, clear of DK64's own objects,
// the note trails and the other collectibles), and where his Jiggy appears.
typedef struct {
    f32 x, y, z;
} CheatoSpot;
typedef struct {
    Maps map;
    CheatoSpot cheato;
    CheatoSpot reward;
    CheatoSpot pages[PAGE_COUNT];
} CheatoPlacement;
static const CheatoPlacement cheato_maps[] = {
    {MAP_DK_ISLES_OVERWORLD,
     {2850.0f, 500.0f, 950.0f}, // The main island's north beach, below the mountain.
     {2910.0f, 500.0f, 960.0f},
     {
         {3510.0f, 495.0f, 1400.0f}, // The main island's east beach, south of the Aztec ledge.
         {2850.0f, 500.0f, 3500.0f}, // Krem Isle's east shore.
         {1070.0f, 500.0f, 2350.0f}, // The Banana Fairy's island, by its entrance.
     }},
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// What he says.
static const char *const cheato_meet[] = {
    "WELL, WELL! A BEAR, A BIRD AND AN APE. I AM CHEATO, KEEPER OF GRUNTY'S CHEATS... OR I WAS!",
    "THAT HAG TORE THREE PAGES OUT OF ME AND FLUNG THEM ALL OVER THESE ISLES!",
    "ONE BLEW ROUND THIS BEACH TOWARD THE JUNGLE LOBBY, ONE TO KREM ISLE'S FAR SHORE, ONE TO THE FAIRY'S ISLAND.",
    "BRING ALL THREE BACK TO ME AND I'LL MAKE IT WORTH YOUR WHILE!",
};
static char cheato_count_line[48]; // "YOU'VE FOUND N OF MY 3 PAGES."
static const char *const cheato_remind[] = {
    cheato_count_line,
    "ROUND THIS BEACH, KREM ISLE'S FAR SHORE AND THE FAIRY'S ISLAND. CHOP CHOP!",
};
static const char *const cheato_mend[] = {
    "MY PAGES! ALL THREE! HOLD STILL WHILE I STICK THEM BACK IN...",
};
static const char *const cheato_mended[] = {
    "AHH, GOOD AS NEW! HERE'S A LITTLE CHEAT FOR YOU: ONE JIGGY, NO QUESTIONS ASKED!",
};
static const char *const cheato_after[] = {
    "TOP SECRET CHEAT: THERE IS NO CHEAT FOR BEATING GRUNTY. SORRY!",
};
static char cheato_page_line[48]; // "A PAGE OF MINE! N OF 3."
static const char *const cheato_page[] = {
    cheato_page_line,
};
static const JiggyTalker cheato_talker = {
    CHEATO_PORTRAIT, 6, "CHEATO", cheato_talk_sfx, 2, 22000.0f / 32767.0f, 1.0f,
};

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_8076A0B3;
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    CHEATO_UNLOADED,
    CHEATO_READY,
    CHEATO_UNAVAILABLE,
} CheatoLoadState;

typedef enum {
    CHEATO_IDLE,
    CHEATO_TALKING,
    CHEATO_MENDING, // Putting the pages back.
} CheatoAction;

typedef enum {
    TALK_NONE,
    TALK_MEET,
    TALK_REMIND,
    TALK_MEND,   // After it, he mends himself.
    TALK_MENDED, // After it, his Jiggy appears.
    TALK_AFTER,
} CheatoTalk;

static u8 cheato_block[CHEATO_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx cheato_gfx[2][CHEATO_GFX_MAX];
static Mtx cheato_mtx[2][CHEATO_MTX_MAX];
static JiggyBkPose cheato_pose;
static CheatoLoadState cheato_state = CHEATO_UNLOADED;
static Vtx page_vtx[PAGE_VTX] __attribute__((aligned(8)));
static Mtx page_mtx[2][PAGE_COUNT];

static s32 cheato_last_map = -1;
static u32 cheato_last_timer;
static const CheatoPlacement *cheato_place_map;
static s32 cheato_placed;
static u32 cheato_saved;
static f32 cheato_pos[3];
static f32 cheato_reward[3];
static f32 page_pos[PAGE_COUNT][3];
static f32 cheato_yaw;
static CheatoAction cheato_action;
static f32 cheato_progress;
static f32 cheato_timer;
static f32 cheato_clock; // Seconds, for the bob and the pages' turn.
static CheatoTalk cheato_talk;
static s32 cheato_armed;
static u32 cheato_last_talk;
static s32 cheato_reward_due;
static s32 cheato_reward_jingle;

static void page_set_vtx(Vtx *v, f32 x, f32 y, f32 z, u32 rgb) {
    v->v.ob[0] = (s16)x;
    v->v.ob[1] = (s16)y;
    v->v.ob[2] = (s16)z;
    v->v.flag = 0;
    v->v.tc[0] = 0;
    v->v.tc[1] = 0;
    v->v.cn[0] = (rgb >> 16) & 0xFF;
    v->v.cn[1] = (rgb >> 8) & 0xFF;
    v->v.cn[2] = rgb & 0xFF;
    v->v.cn[3] = 0xFF;
}

// A quad's four corners, (x0, y0) to (x1, y1) at depth z, counter-clockwise.
static void page_set_quad(Vtx *v, f32 x0, f32 y0, f32 x1, f32 y1, f32 z, u32 rgb) {
    page_set_vtx(&v[0], x0, y0, z, rgb);
    page_set_vtx(&v[1], x1, y0, z, rgb);
    page_set_vtx(&v[2], x1, y1, z, rgb);
    page_set_vtx(&v[3], x0, y1, z, rgb);
}

// The page's vertices, in units of a tenth of a DK64 unit (its matrix scales them down): a
// sheet 14 by 18 units, with a red heading and two ruled lines on each side, just off it.
static void page_build(void) {
    static const u32 line_rgb[3] = {0xB03020, 0x705038, 0x705038};
    static const f32 line_y[3] = {55.0f, 15.0f, -25.0f};
    static const f32 line_half[3] = {35.0f, 50.0f, 50.0f};
    s32 i;

    page_set_quad(&page_vtx[0], -70.0f, -90.0f, 70.0f, 90.0f, 0.0f, 0xF0E4C0);
    for (i = 0; i < 3; i++) {
        page_set_quad(&page_vtx[4 + i * 8], -line_half[i], line_y[i] - 5.0f, line_half[i], line_y[i] + 5.0f, 3.0f, line_rgb[i]);
        page_set_quad(&page_vtx[8 + i * 8], -line_half[i], line_y[i] - 5.0f, line_half[i], line_y[i] + 5.0f, -3.0f, line_rgb[i]);
    }
}

static void cheato_load(void) {
    s32 size;
    s32 i;

    cheato_state = CHEATO_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(CHEATO_MODEL, cheato_block, sizeof(cheato_block));
    if (size <= 0 || size > (s32)sizeof(cheato_block)) {
        return;
    }
    // Branch 1 of selector 1, as Banjo-Kazooie's modelRender resets them, and 3 and 4 as
    // chCheato's draw (func_80390030) shows them.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        cheato_pose.selectors[i] = 0;
    }
    cheato_pose.selectors[1] = 1;
    cheato_pose.selectors[3] = 1;
    cheato_pose.selectors[4] = 1;
    page_build();
    cheato_state = CHEATO_READY;
}

static f32 cheato_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + CHEATO_FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < CHEATO_FLOOR_MAX_STEP &&
        y - floor < CHEATO_FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

static s32 cheato_pages_found(void) {
    s32 n = 0;
    s32 i;

    for (i = 0; i < PAGE_COUNT; i++) {
        if (cheato_saved & (1 << i)) {
            n++;
        }
    }
    return n;
}

static void cheato_save(u32 saved) {
    cheato_saved = saved;
    jiggy_save_set(current_file, CHEATO_SAVE_MAP_KEY(current_map), cheato_saved);
}

static void cheato_place(Actor *player) {
    s32 i;

    cheato_pos[0] = cheato_place_map->cheato.x;
    cheato_pos[1] = cheato_floor(cheato_place_map->cheato.x, cheato_place_map->cheato.y, cheato_place_map->cheato.z);
    cheato_pos[2] = cheato_place_map->cheato.z;
    cheato_reward[0] = cheato_place_map->reward.x;
    cheato_reward[1] = cheato_floor(cheato_place_map->reward.x, cheato_place_map->reward.y, cheato_place_map->reward.z);
    cheato_reward[2] = cheato_place_map->reward.z;
    for (i = 0; i < PAGE_COUNT; i++) {
        const CheatoSpot *spot = &cheato_place_map->pages[i];

        page_pos[i][0] = spot->x;
        page_pos[i][1] = cheato_floor(spot->x, spot->y, spot->z) + PAGE_HOVER;
        page_pos[i][2] = spot->z;
    }
    cheato_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, cheato_pos[0], cheato_pos[2]) & 0xFFF;
    cheato_action = CHEATO_IDLE;
    cheato_talk = TALK_NONE;
    cheato_progress = 0.0f;
    cheato_reward_due = (cheato_saved & CHEATO_SAVE_MENDED) != 0;
    cheato_reward_jingle = FALSE;
    cheato_placed = TRUE;
}

static s32 cheato_player_near(Actor *player, f32 radius) {
    f32 dx = cheato_pos[0] - player->x_position;
    f32 dy = cheato_pos[1] - player->y_position;
    f32 dz = cheato_pos[2] - player->z_position;

    return dx * dx + dz * dz <= radius * radius && dy >= -CHEATO_TALK_FEET && dy <= CHEATO_TALK_FEET;
}

static void cheato_turn_to(s16 target, u32 frames) {
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = (target & 0xFFF) - cheato_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        cheato_yaw += turn * CHEATO_TURN_RATE;
        if (cheato_yaw < 0.0f) {
            cheato_yaw += 4096.0f;
        } else if (cheato_yaw >= 4096.0f) {
            cheato_yaw -= 4096.0f;
        }
    }
}

static void cheato_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

// Writes "<before>N<after>" into `line`.
static void cheato_format(char *line, const char *before, s32 n, const char *after) {
    s32 i = 0;

    while (*before != '\0') {
        line[i++] = *before++;
    }
    line[i++] = '0' + n;
    while (*after != '\0') {
        line[i++] = *after++;
    }
    line[i] = '\0';
}

static s32 cheato_say(CheatoTalk kind, const char *const *pages, s32 count) {
    if (!jiggy_talk_say(JIGGY_TALK_OWNER_CHEATO, &cheato_talker, pages, count, JIGGY_TALK_LOCK)) {
        return FALSE;
    }
    cheato_talk = kind;
    cheato_action = CHEATO_TALKING;
    return TRUE;
}

// What he'd say now.
static CheatoTalk cheato_next_talk(void) {
    if (cheato_saved & CHEATO_SAVE_MENDED) {
        return TALK_AFTER;
    }
    if (cheato_pages_found() == PAGE_COUNT) {
        return TALK_MEND;
    }
    return (cheato_saved & CHEATO_SAVE_MET) ? TALK_REMIND : TALK_MEET;
}

static void cheato_start_talk(CheatoTalk kind) {
    switch (kind) {
        case TALK_MEET:
            cheato_say(kind, cheato_meet, ARRAY_COUNT_OF(cheato_meet));
            break;
        case TALK_REMIND:
            cheato_format(cheato_count_line, "STILL MISSING PAGES! YOU'VE FOUND ", cheato_pages_found(), " OF MY 3.");
            cheato_say(kind, cheato_remind, ARRAY_COUNT_OF(cheato_remind));
            break;
        case TALK_MEND:
            cheato_say(kind, cheato_mend, ARRAY_COUNT_OF(cheato_mend));
            break;
        default:
            cheato_say(kind, cheato_after, ARRAY_COUNT_OF(cheato_after));
            break;
    }
}

static void cheato_talk_done(void) {
    CheatoTalk kind = cheato_talk;

    cheato_talk = TALK_NONE;
    cheato_action = CHEATO_IDLE;
    switch (kind) {
        case TALK_MEET:
            cheato_save(cheato_saved | CHEATO_SAVE_MET);
            break;
        case TALK_MEND:
            // Mended: saved now, so the Jiggy is his to give whatever happens.
            cheato_save(cheato_saved | CHEATO_SAVE_MET | CHEATO_SAVE_MENDED);
            cheato_action = CHEATO_MENDING;
            cheato_timer = 0.0f;
            cheato_lock_player(TRUE);
            jiggy_bk_jingle_play(JINGLE_CHEATO, 1.0f);
            break;
        case TALK_MENDED:
            cheato_reward_due = TRUE;
            cheato_reward_jingle = TRUE;
            break;
        default:
            break;
    }
}

// Collects the pages the player touches.
static void cheato_collect_pages(Actor *player) {
    s32 i;

    for (i = 0; i < PAGE_COUNT; i++) {
        f32 dx, dy, dz;

        if (cheato_saved & (1 << i)) {
            continue;
        }
        dx = page_pos[i][0] - player->x_position;
        dy = page_pos[i][1] - player->y_position;
        dz = page_pos[i][2] - player->z_position;
        if (dx * dx + dz * dz > PAGE_TOUCH_RADIUS * PAGE_TOUCH_RADIUS || dy < -PAGE_TOUCH_BELOW || dy > PAGE_TOUCH_ABOVE) {
            continue;
        }
        cheato_save(cheato_saved | (1 << i));
        jiggy_bk_jingle_play(JINGLE_PAGE, 28000.0f / 32767.0f);
        if (cheato_pages_found() == PAGE_COUNT) {
            cheato_format(cheato_page_line, "THAT'S ALL ", PAGE_COUNT, " OF MY PAGES! BRING THEM HERE!");
        } else {
            cheato_format(cheato_page_line, "A PAGE OF MINE! THAT'S ", cheato_pages_found(), " OF 3.");
        }
        // Cheato's word from afar, without stopping the player.
        jiggy_talk_say(JIGGY_TALK_OWNER_CHEATO, &cheato_talker, cheato_page, 1, JIGGY_TALK_AUTO);
    }
}

static void cheato_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    s16 to_player = func_global_asm_80665DE0(player->x_position, player->z_position, cheato_pos[0], cheato_pos[2]);
    CheatoTalk next;
    s32 is_new;

    if (frames == 0) {
        return;
    }
    cheato_clock += seconds;
    if (cheato_clock > 3600.0f) {
        cheato_clock -= 3600.0f;
    }
    cheato_progress += seconds / CHEATO_FLAP_SECONDS * (cheato_action == CHEATO_MENDING ? CHEATO_MEND_FLAP : 1.0f);
    while (cheato_progress >= 1.0f) {
        cheato_progress -= 1.0f;
    }
    if (cheato_action != CHEATO_MENDING && !jiggy_talk_open(JIGGY_TALK_OWNER_BRENTILDA)) {
        cheato_collect_pages(player);
    }
    switch (cheato_action) {
        case CHEATO_IDLE:
            if (cheato_player_near(player, CHEATO_LOOK_RADIUS)) {
                cheato_turn_to(to_player, frames);
            }
            // His word about a page, while it's up.
            jiggy_talk_update();
            next = cheato_next_talk();
            is_new = next != TALK_AFTER && next != TALK_REMIND;
            if (!cheato_armed) {
                cheato_armed = !cheato_player_near(player, CHEATO_REARM_RADIUS) &&
                               (is_new || object_timer - cheato_last_talk >= CHEATO_REMIND_FRAMES);
                break;
            }
            if (cheato_player_near(player, CHEATO_TALK_RADIUS) &&
                player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT &&
                (player->control_state == STATE_IDLE || (is_new && player->control_state == STATE_WALK)) &&
                !jiggy_talk_open(JIGGY_TALK_OWNER_BRENTILDA)) {
                s16 to_cheato = func_global_asm_80665DE0(cheato_pos[0], cheato_pos[2], player->x_position, player->z_position);

                cheato_armed = FALSE;
                cheato_last_talk = object_timer;
                player->y_rotation = to_cheato & 0xFFF;
                player->unkEE = to_cheato & 0xFFF;
                jiggy_bk_sfx_play(CHEATO_SFX_IDLE, 24000.0f / 32767.0f, 1.0f);
                cheato_start_talk(next);
            }
            break;
        case CHEATO_TALKING:
            cheato_turn_to(to_player, frames);
            jiggy_talk_update();
            if (!jiggy_talk_open(JIGGY_TALK_OWNER_CHEATO)) {
                cheato_talk_done();
            }
            break;
        case CHEATO_MENDING:
            cheato_lock_player(TRUE);
            cheato_timer += seconds;
            cheato_yaw += CHEATO_MEND_SPIN * seconds;
            while (cheato_yaw >= 4096.0f) {
                cheato_yaw -= 4096.0f;
            }
            if (cheato_timer >= CHEATO_MEND_SECONDS) {
                cheato_yaw = to_player & 0xFFF;
                cheato_action = CHEATO_IDLE;
                if (!cheato_say(TALK_MENDED, cheato_mended, ARRAY_COUNT_OF(cheato_mended))) {
                    cheato_lock_player(FALSE);
                    cheato_reward_due = TRUE;
                    cheato_reward_jingle = TRUE;
                }
            }
            break;
    }
    if (cheato_reward_due) {
        if (cheato_reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
            cheato_reward_jingle = FALSE;
        }
        if (jiggy_reward_update(REWARD_ID, cheato_reward[0], cheato_reward[1] + REWARD_HEIGHT, cheato_reward[2], player, frames)) {
            cheato_reward_due = FALSE;
        }
    }
}

static Gfx *cheato_draw(Gfx *dl, Gfx *gfx, Mtx *mtx) {
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 progress = cheato_progress;

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, cheato_yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, cheato_pose.world);
    cheato_pose.world[3][0] = cheato_pos[0];
    cheato_pose.world[3][1] = cheato_pos[1] + CHEATO_HOVER +
                              CHEATO_BOB * func_global_asm_80612794((s16)(cheato_clock / CHEATO_BOB_SECONDS * 4096.0f) & 0xFFF);
    cheato_pose.world[3][2] = cheato_pos[2];
    if (progress > 0.9999f) {
        progress = 0.9999f;
    }
    cheato_pose.model = CHEATO_MODEL;
    cheato_pose.block = cheato_block;
    cheato_pose.animation = CHEATO_ANIM_FLAP;
    cheato_pose.progress = progress;
    cheato_pose.gfx = gfx;
    cheato_pose.gfx_capacity = CHEATO_GFX_MAX;
    cheato_pose.mtx = mtx;
    cheato_pose.mtx_capacity = CHEATO_MTX_MAX;
    if (jiggy_bk_model_pose(&cheato_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// The pages not yet found, each turning and bobbing over its spot.
static Gfx *cheato_draw_pages(Gfx *dl, s32 buffer) {
    s32 drawn = FALSE;
    s32 i;

    for (i = 0; i < PAGE_COUNT; i++) {
        f32 mf[4][4];
        f32 rotate[4][4];
        f32 tilt[4][4];
        f32 scale[4][4];
        f32 turn;
        f32 bob;

        if (cheato_saved & (1 << i)) {
            continue;
        }
        if (!drawn) {
            gDPPipeSync(dl++);
            gSPClearGeometryMode(dl++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN);
            gSPSetGeometryMode(dl++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
            gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
            gDPSetCycleType(dl++, G_CYC_1CYCLE);
            gDPSetCombineMode(dl++, G_CC_SHADE, G_CC_SHADE);
            gDPSetRenderMode(dl++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
            drawn = TRUE;
        }
        turn = cheato_clock * PAGE_SPIN + i * 1365.0f;
        bob = PAGE_BOB * func_global_asm_80612794((s16)(cheato_clock * 2048.0f + i * 1000.0f) & 0xFFF);
        guScaleF(scale, 0.1f, 0.1f, 0.1f);
        guRotateF(tilt, 12.0f, 1.0f, 0.0f, 0.0f);
        guMtxCatF(scale, tilt, mf);
        guRotateF(rotate, turn * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
        guMtxCatF(mf, rotate, mf);
        mf[3][0] = page_pos[i][0];
        mf[3][1] = page_pos[i][1] + bob;
        mf[3][2] = page_pos[i][2];
        guMtxF2L(mf, &page_mtx[buffer][i]);
        gSPMatrix(dl++, &page_mtx[buffer][i], G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(dl++, page_vtx, PAGE_VTX, 0);
        gSP2Triangles(dl++, 0, 1, 2, 0, 0, 2, 3, 0);
        gSP2Triangles(dl++, 4, 5, 6, 0, 4, 6, 7, 0);
        gSP2Triangles(dl++, 8, 9, 10, 0, 8, 10, 11, 0);
        gSP2Triangles(dl++, 12, 13, 14, 0, 12, 14, 15, 0);
        gSP2Triangles(dl++, 16, 17, 18, 0, 16, 18, 19, 0);
        gSP2Triangles(dl++, 20, 21, 22, 0, 20, 22, 23, 0);
        gSP2Triangles(dl++, 24, 25, 26, 0, 24, 26, 27, 0);
        gSPPopMatrix(dl++, G_MTX_MODELVIEW);
    }
    if (drawn) {
        gDPPipeSync(dl++);
    }
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Cheato and his pages once a frame, with the player.
Gfx *jiggy_cheato_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != cheato_last_map || object_timer < cheato_last_timer) {
        if (cheato_action != CHEATO_IDLE) {
            jiggy_talk_close(JIGGY_TALK_OWNER_CHEATO);
            if (cheato_action == CHEATO_MENDING) {
                cheato_lock_player(FALSE);
            }
        }
        cheato_last_map = current_map;
        cheato_placed = FALSE;
        cheato_place_map = NULL;
        cheato_action = CHEATO_IDLE;
        for (i = 0; i < ARRAY_COUNT_OF(cheato_maps); i++) {
            if (cheato_maps[i].map == current_map) {
                cheato_place_map = &cheato_maps[i];
            }
        }
        cheato_saved = cheato_place_map != NULL ? jiggy_save_get(current_file, CHEATO_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (cheato_place_map == NULL) {
        cheato_last_timer = object_timer;
        return dl;
    }
    if (cheato_state == CHEATO_UNLOADED) {
        cheato_load();
    }
    if (cheato_state != CHEATO_READY) {
        return dl;
    }
    if (!cheato_placed) {
        cheato_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        cheato_place(actor);
        cheato_armed = !cheato_player_near(actor, CHEATO_REARM_RADIUS);
    }
    frames = object_timer - cheato_last_timer;
    cheato_last_timer = object_timer;
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && cheato_action == CHEATO_IDLE && !jiggy_talk_open(JIGGY_TALK_OWNER_CHEATO)) {
        frames = 0;
    }
    cheato_update(actor, frames);
    dl = cheato_draw(dl, cheato_gfx[buffer], cheato_mtx[buffer]);
    return cheato_draw_pages(dl, buffer);
}
