// Bottles: Banjo-Kazooie's mole, read from the player's Banjo-Kazooie ROM, as an NPC in
// Jungle Japes' first area, where the two worlds have collided.
//
// Like chmole (src/core2/ch/mole.c) and chMolehill (src/core2/ch/molehill.c), his molehill
// (ASSET_388_MODEL_MOLEHILL) waits on the ground; when the player comes near and is standing
// or walking, Bottles (ASSET_387_MODEL_BOTTLES) pops out of it with Banjo-Kazooie's pop-out
// animations and sounds, turns to face the player and talks; after the last line he ducks
// back down. Both are drawn right after the player's own model (func_global_asm_80630DCC in
// patches_draw.c), and his text box in the HUD pass (jiggy_bottles_hud_draw, patches_ui.c).
//
// The text box: Banjo-Kazooie's Bottles portrait (ASSET_7F0_SPRITE_BOTTLES, its mouth
// frames moving while the text types out, as gczoombox does) beside DK64's own dialogue font
// (printStyledText style 6, the font of DK64's text bubbles), with Bottles' talking sounds
// (SFX_B4/BC/BD_BOTTLES_TALKING, as gczoombox plays them). A shows the rest of the line, then
// the next one. DK64's text bubble only shows lines from its own text files, so the box is
// drawn here instead.
//
// While he talks the player can't move: the game's own cutscene lock is set (bit 0x10 of
// D_global_asm_8076A0B1, as playCutscene and func_global_asm_8061C518 do), which keeps the
// player's controller from the player (func_global_asm_8060AC7C) and brings the cutscene
// bars in; A is read from the raw pad, as the game's own text prompts do. The camera stays
// where it is.
//
// The first talk is kept with the DK64 save file (jiggy_save_get/set); after it, Bottles
// gives a one-line reminder each time.
//
// Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

#define BOTTLES_MODEL 0x387  // ASSET_387_MODEL_BOTTLES
#define MOLEHILL_MODEL 0x388 // ASSET_388_MODEL_MOLEHILL
// Bottles converts to about 22 KiB, with 37 bones and about 60 pose commands; the molehill
// to about 7.5 KiB, with 2 bones and 12 commands.
#define BOTTLES_BLOCK_MAX 0x8000
#define BOTTLES_GFX_MAX 96
#define BOTTLES_MTX_MAX 40
#define MOLEHILL_BLOCK_MAX 0x3000
#define MOLEHILL_GFX_MAX 32
#define MOLEHILL_MTX_MAX 4

// Banjo-Kazooie animations (moleAnimations, chMolehillAnimations) and how long each plays,
// in seconds.
#define BOTTLES_ANIM_EXIT 0x139  // ASSET_139_ANIM_BOTTLES_EXIT: back down into the molehill
#define BOTTLES_ANIM_ENTER 0x13A // ASSET_13A_ANIM_BOTTLES_ENTER: popping out
#define BOTTLES_ANIM_IDLE 0x13B  // ASSET_13B_ANIM_BOTTLES_IDLE: while he talks
#define MOLEHILL_ANIM_EXIT 0x13C // ASSET_13C_ANIM_MOLEHILL_UNKNOWN: as he goes back down
#define MOLEHILL_ANIM_ENTER 0x13D // ASSET_13D_ANIM_MOLEHILL_UNKNOWN: as he pops out (its start
                                  // is the molehill at rest)
#define BOTTLES_ENTER_SECONDS 4.5f
#define BOTTLES_IDLE_SECONDS 7.0f
#define BOTTLES_EXIT_SECONDS 1.7f
// Where through popping out the text starts (chmole_learnAbility at 0.35).
#define BOTTLES_TALK_AT 0.35f

// Banjo-Kazooie sounds (sfx_e) and how chmole plays them.
#define BOTTLES_SFX_DIGGING 0x3F9      // SFX_3F9_UNKNOWN: digging, while popping out (to 0.16) and
                                       // going back down (0.35 to 0.9), 26000/32767, 1.4
#define BOTTLES_SFX_SHAKING_MOUTH 0xC6 // SFX_C6_SHAKING_MOUTH: at 0.14 out, 24000/32767, 1.2
#define BOTTLES_SFX_PULLING 0x2C       // SFX_2C_PULLING_NOISE: at 0.4 out, 24000/32767, 1.2
#define BOTTLES_SFX_POP 0xC5           // SFX_C5_TWINKLY_POP: at 0.75 out, 32000/32767, 1.0
#define BOTTLES_SFX_HEADSCRATCH 0x6F   // SFX_6F_BANJO_HEADSCRATCH: his idle's scratches
// Bottles' voice in the text box (gczoombox), one of these at 20000/32767, about pitch 1.1.
static const u16 bottles_talk_sfx[] = {0xB4, 0xBC, 0xBD};
#define BOTTLES_TALK_SFX_EVERY 5 // Characters typed out between sounds.

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// How fast he turns to face the player (subaddie_turnToYaw(this, 4.0f): a quarter of the
// way a frame, at 30 frames a second).
#define BOTTLES_TURN_RATE 0.25f
// He talks when the player comes this close (Banjo-Kazooie's 250 units, scaled), on the
// ground; after a talk, not again until the player has walked this far off.
#define BOTTLES_TALK_RADIUS 70.0f
#define BOTTLES_REARM_RADIUS 110.0f
#define BOTTLES_TALK_FEET_ABOVE 40.0f
#define BOTTLES_TALK_FEET_BELOW 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing him on the floor under his spot: looked for from this far above the spot, and only
// trusted this close to the spot's height.
#define BOTTLES_FLOOR_PROBE 40.0f
#define BOTTLES_FLOOR_MAX_STEP 60.0f

// DK64 player states (Actor control_state) he talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
// D_global_asm_8076A0B1's cutscene lock (see the top of this file).
#define CUTSCENE_LOCK 0x10

// The text box, in screen pixels: Bottles' portrait (32x32 frames, drawn twice the size, as
// the zoombox zooms them) on the left, his name and the text on the right.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 10
#define PORTRAIT_MAX_TEXELS (32 * 32)
// Frames 0 to 5 open his mouth; 6 on turn him away, which the box doesn't need.
#define PORTRAIT_TALK_FRAMES 6
#define PORTRAIT_TICKS_PER_FRAME 2
#define TEXT_STYLE 6 // DK64's dialogue font (uppercase letters, digits and . , ! ? : ; ' - ( ) %).
#define TEXT_LINE_HEIGHT 17
#define TEXT_NAME_Y 6
#define TEXT_FIRST_LINE_Y 24
// printStyledText's text comes out 1.25 times as far from the middle of the screen (y 120) as
// the box's own pixels, 3 pixels higher; this turns a box row into the y it takes for it.
#define TEXT_Y(y) (((y) - 117) * 4 / 5 + 120)
#define TEXT_MAX_LINES 4
#define TEXT_LINE_MAX 48
// Characters typed out a frame, and the box's fade in (alpha a frame).
#define TEXT_CHARS_PER_FRAME 1
#define BOX_FADE_STEP 0x30
#define BOX_ALPHA 0xC8

// Where he is, map by map: Jungle Japes' first area, on the open grass in the middle (the player
// comes in at about (931, 280, 721)), clear of the notes' trail, the Jinjos, the Honeycombs and
// the feathers.
typedef struct {
    Maps map;
    f32 x, y, z;
} BottlesPlacement;
static const BottlesPlacement bottles_spots[] = {
    {MAP_JAPES, 1120.0f, 280.0f, 560.0f},
};

// What he says. Each page is one press of A; lines are broken to fit the box when shown.
static const char *const bottles_first_talk[] = {
    "HEY THERE, BANJO! AND YOU MUST BE DONKEY KONG. I'M BOTTLES THE MOLE!",
    "OUR TWO WORLDS HAVE COLLIDED, AND THIS JUNGLE IS NOW A BIT OF BOTH!",
    "LOOK OUT FOR MUSIC NOTES, JINJOS, RED AND GOLD FEATHERS AND HONEYCOMBS.",
    "THEY'RE SCATTERED ALL OVER THE PLACE. GOOD LUCK, YOU TWO!",
};
static const char *const bottles_reminder[] = {
    "REMEMBER: NOTES, JINJOS, FEATHERS AND HONEYCOMBS. FIND THEM ALL!",
};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): set once he's had his first talk on a map.
#define BOTTLES_SAVE_MAP_KEY(map) (0x42000000 | (u32)(map)) // 'B'

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern u8 current_file;
extern s16 D_global_asm_80744490; // Screen width.
extern s16 D_global_asm_80744494; // Screen height.
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_8076A0B3;
extern OSContPad D_global_asm_807ECD58; // Buttons held this frame, before the cutscene lock.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y, leaving it as it was if there's none.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);

typedef enum {
    BOTTLES_UNLOADED,
    BOTTLES_READY,
    BOTTLES_UNAVAILABLE, // No Banjo-Kazooie ROM, or a model or the portrait didn't convert.
} BottlesLoadState;

typedef enum {
    BOTTLES_HIDDEN,  // Only the molehill.
    BOTTLES_POPPING, // Out of the molehill (the text starts part way).
    BOTTLES_TALKING, // Out, talking.
    BOTTLES_DUCKING, // Back down.
} BottlesAction;

static u8 bottles_block[BOTTLES_BLOCK_MAX] __attribute__((aligned(16)));
static u8 molehill_block[MOLEHILL_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx bottles_gfx[2][BOTTLES_GFX_MAX];
static Mtx bottles_mtx[2][BOTTLES_MTX_MAX];
static Gfx molehill_gfx[2][MOLEHILL_GFX_MAX];
static Mtx molehill_mtx[2][MOLEHILL_MTX_MAX];
static JiggyBkPose bottles_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static BottlesLoadState bottles_state = BOTTLES_UNLOADED;

static s32 bottles_last_map = -1;
static const BottlesPlacement *bottles_spot; // This map's, or NULL.
static s32 bottles_placed;
static f32 bottles_pos[3];
static f32 bottles_yaw; // 12-bit angle units, as a float so the turn eases.
static BottlesAction bottles_action;
static f32 bottles_progress; // 0 to 1 through the action's animation.
static u32 bottles_last_timer;
static s32 bottles_talked;   // His first talk is done (saved).
static s32 bottles_armed;    // The player has been away since his last talk.
static s32 bottles_digging;  // The digging sound's handle, or 0.
static u32 bottles_random_state = 0x424F5454;

// The dialogue: the pages being said, the page shown, its lines, how many characters of it
// are typed out, and the box's alpha.
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static s32 talk_open;
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_length; // Characters in the page, counting a space between lines.
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_a_held = TRUE;

static f32 bottles_random(void) {
    bottles_random_state = bottles_random_state * 1664525 + 1013904223;
    return (bottles_random_state >> 8) * (1.0f / 16777216.0f);
}

static void bottles_load(void) {
    s32 size;
    s32 count;
    s32 i;

    bottles_state = BOTTLES_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(BOTTLES_MODEL, bottles_block, sizeof(bottles_block));
    if (size <= 0 || size > (s32)sizeof(bottles_block)) {
        return;
    }
    size = jiggy_bk_model_build(MOLEHILL_MODEL, molehill_block, sizeof(molehill_block));
    if (size <= 0 || size > (s32)sizeof(molehill_block)) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(0x7F0); // ASSET_7F0_SPRITE_BOTTLES
    if (count <= 0) {
        return;
    }
    if (count > PORTRAIT_MAX_FRAMES) {
        count = PORTRAIT_MAX_FRAMES;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(0x7F0, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        if (got <= 0 || width == 0 || height == 0 || width * height > PORTRAIT_MAX_TEXELS) {
            continue;
        }
        portrait_width[portrait_frames] = width;
        portrait_height[portrait_frames] = height;
        portrait_frames++;
    }
    if (portrait_frames == 0) {
        return;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden (chmole's draw hides 3 and 4 itself).
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        bottles_pose.selectors[i] = 0;
    }
    bottles_pose.selectors[1] = 1;
    bottles_state = BOTTLES_READY;
}

// Stands him over the floor at his spot (or at the spot's height if no floor is found near).
static void bottles_place(void) {
    f32 probe = bottles_spot->y + BOTTLES_FLOOR_PROBE;
    f32 floor = probe;

    bottles_pos[0] = bottles_spot->x;
    bottles_pos[1] = bottles_spot->y;
    bottles_pos[2] = bottles_spot->z;
    func_global_asm_80667110(bottles_spot->x, bottles_spot->z, &floor);
    if (floor != probe && floor - bottles_spot->y < BOTTLES_FLOOR_MAX_STEP &&
        bottles_spot->y - floor < BOTTLES_FLOOR_MAX_STEP) {
        bottles_pos[1] = floor;
    }
    bottles_yaw = func_global_asm_80665DE0(gPlayerPointer->x_position, gPlayerPointer->z_position, bottles_pos[0], bottles_pos[2]) & 0xFFF;
    bottles_action = BOTTLES_HIDDEN;
    bottles_progress = 0.0f;
    bottles_placed = TRUE;
}

// Breaks the page into lines that fit the box, a word at a time.
static void talk_set_page(s32 page) {
    const char *text = talk_pages[page];
    s32 text_width = BOX_WIDTH - 2 * BOX_BORDER - BOX_PAD - PORTRAIT_SCALE * 32 - 2 * BOX_PAD;
    char candidate[TEXT_LINE_MAX];
    s32 line = 0;
    s32 length = 0;

    talk_page = page;
    talk_lines[0][0] = '\0';
    while (*text != '\0' && line < TEXT_MAX_LINES) {
        const char *word = text;
        s32 word_length = 0;
        s32 i;
        s32 n;

        while (word[word_length] != '\0' && word[word_length] != ' ') {
            word_length++;
        }
        n = 0;
        for (i = 0; talk_lines[line][i] != '\0'; i++) {
            candidate[n++] = talk_lines[line][i];
        }
        if (n > 0) {
            candidate[n++] = ' ';
        }
        for (i = 0; i < word_length && n < TEXT_LINE_MAX - 1; i++) {
            candidate[n++] = word[i];
        }
        candidate[n] = '\0';
        if (talk_lines[line][0] != '\0' && getCenterOfString(TEXT_STYLE, (u8 *)candidate) > text_width) {
            // The word starts the next line.
            line++;
            if (line >= TEXT_MAX_LINES) {
                break;
            }
            talk_lines[line][0] = '\0';
            continue;
        }
        for (i = 0; i <= n; i++) {
            talk_lines[line][i] = candidate[i];
        }
        text += word_length;
        while (*text == ' ') {
            text++;
        }
    }
    talk_line_count = line < TEXT_MAX_LINES ? line + 1 : TEXT_MAX_LINES;
    for (line = 0; line < talk_line_count; line++) {
        s32 i = 0;

        while (talk_lines[line][i] != '\0') {
            i++;
        }
        length += i + 1;
    }
    talk_length = length;
    talk_shown = 0;
    talk_ticks = 0;
}

static void talk_start(void) {
    if (bottles_talked) {
        talk_pages = bottles_reminder;
        talk_page_count = ARRAY_COUNT_OF(bottles_reminder);
    } else {
        talk_pages = bottles_first_talk;
        talk_page_count = ARRAY_COUNT_OF(bottles_first_talk);
    }
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
}

// The player can't move while he talks (see the top of this file).
static void bottles_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void bottles_stop_digging(void) {
    if (bottles_digging > 0) {
        jiggy_bk_sound_stop(bottles_digging);
        bottles_digging = 0;
    }
}

// True when `at` is passed going from `from` to `to` through an animation.
static s32 bottles_passed(f32 from, f32 to, f32 at) {
    return from < at && to >= at;
}

static void bottles_start_action(BottlesAction action) {
    bottles_action = action;
    bottles_progress = 0.0f;
    bottles_stop_digging();
    if (action == BOTTLES_POPPING) {
        bottles_digging = jiggy_bk_sfx_play(BOTTLES_SFX_DIGGING, 26000.0f / 32767.0f, 1.4f);
    }
}

// Whether the player is close enough, and standing or walking on the ground, to talk.
static s32 bottles_player_near(Actor *player, f32 radius) {
    f32 dx = bottles_pos[0] - player->x_position;
    f32 dy = bottles_pos[1] - player->y_position;
    f32 dz = bottles_pos[2] - player->z_position;

    return dx * dx + dz * dz <= radius * radius && dy >= -BOTTLES_TALK_FEET_ABOVE && dy <= BOTTLES_TALK_FEET_BELOW;
}

// The text box's frame: A to type out the rest of the page or go on to the next, and the
// talking sounds while it types.
static void talk_update(void) {
    s32 a_held = (D_global_asm_807ECD58.button & A_BUTTON) != 0;
    s32 a_pressed = a_held && !talk_a_held;
    s32 before = talk_shown;

    talk_a_held = a_held;
    talk_ticks++;
    if (talk_alpha < 0xFF) {
        talk_alpha += BOX_FADE_STEP;
        if (talk_alpha > 0xFF) {
            talk_alpha = 0xFF;
        }
    }
    if (talk_shown < talk_length) {
        if (a_pressed) {
            talk_shown = talk_length;
        } else {
            talk_shown += TEXT_CHARS_PER_FRAME;
        }
        if (talk_shown / BOTTLES_TALK_SFX_EVERY != before / BOTTLES_TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 pick = (u32)(bottles_random() * ARRAY_COUNT_OF(bottles_talk_sfx)) % ARRAY_COUNT_OF(bottles_talk_sfx);

            jiggy_bk_sfx_play(bottles_talk_sfx[pick], 20000.0f / 32767.0f, 1.0f + bottles_random() * 0.2f);
        }
        return;
    }
    if (!a_pressed) {
        return;
    }
    if (talk_page + 1 < talk_page_count) {
        talk_set_page(talk_page + 1);
        return;
    }
    // The last page: he goes back down and the player can move again.
    talk_open = FALSE;
    if (!bottles_talked) {
        bottles_talked = TRUE;
        jiggy_save_set(current_file, BOTTLES_SAVE_MAP_KEY(current_map), 1);
    }
    bottles_lock_player(FALSE);
    bottles_start_action(BOTTLES_DUCKING);
}

// A frame of his behavior (chmole_update), `frames` game frames on.
static void bottles_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    f32 before = bottles_progress;
    s16 to_player = func_global_asm_80665DE0(player->x_position, player->z_position, bottles_pos[0], bottles_pos[2]) & 0xFFF;
    f32 turn;
    u32 f;

    // Turns to face the player while he's out.
    if (bottles_action != BOTTLES_HIDDEN) {
        for (f = 0; f < frames; f++) {
            turn = to_player - bottles_yaw;
            if (turn > 2048.0f) {
                turn -= 4096.0f;
            } else if (turn < -2048.0f) {
                turn += 4096.0f;
            }
            bottles_yaw += turn * BOTTLES_TURN_RATE;
            if (bottles_yaw < 0.0f) {
                bottles_yaw += 4096.0f;
            } else if (bottles_yaw >= 4096.0f) {
                bottles_yaw -= 4096.0f;
            }
        }
    }

    switch (bottles_action) {
        case BOTTLES_HIDDEN:
            if (!bottles_armed) {
                bottles_armed = !bottles_player_near(player, BOTTLES_REARM_RADIUS);
                break;
            }
            if (bottles_player_near(player, BOTTLES_TALK_RADIUS) &&
                player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT &&
                (player->control_state == STATE_IDLE || player->control_state == STATE_WALK)) {
                s16 to_bottles = func_global_asm_80665DE0(bottles_pos[0], bottles_pos[2], player->x_position, player->z_position);

                bottles_armed = FALSE;
                bottles_lock_player(TRUE);
                // The player turns to him (func_8028F94C(2, this->position)).
                player->y_rotation = to_bottles & 0xFFF;
                player->unkEE = to_bottles & 0xFFF;
                bottles_start_action(BOTTLES_POPPING);
            }
            break;
        case BOTTLES_POPPING:
            bottles_lock_player(TRUE);
            bottles_progress += seconds / BOTTLES_ENTER_SECONDS;
            if (bottles_passed(before, bottles_progress, 0.16f)) {
                bottles_stop_digging();
            }
            if (bottles_passed(before, bottles_progress, 0.14f)) {
                jiggy_bk_sfx_play(BOTTLES_SFX_SHAKING_MOUTH, 24000.0f / 32767.0f, 1.2f);
            }
            if (bottles_passed(before, bottles_progress, BOTTLES_TALK_AT)) {
                talk_start();
            }
            if (bottles_passed(before, bottles_progress, 0.4f)) {
                jiggy_bk_sfx_play(BOTTLES_SFX_PULLING, 24000.0f / 32767.0f, 1.2f);
            }
            if (bottles_passed(before, bottles_progress, 0.75f)) {
                jiggy_bk_sfx_play(BOTTLES_SFX_POP, 32000.0f / 32767.0f, 1.0f);
            }
            if (bottles_progress >= 1.0f) {
                bottles_action = BOTTLES_TALKING;
                bottles_progress = 0.0f;
            }
            break;
        case BOTTLES_TALKING:
            bottles_lock_player(TRUE);
            bottles_progress += seconds / BOTTLES_IDLE_SECONDS;
            if (bottles_progress >= 1.0f) {
                bottles_progress -= 1.0f;
                before = 0.0f;
            }
            // His head scratches (chmole's MOLE_STATE_3_EXPLAINING_ABILITY).
            if (bottles_passed(before, bottles_progress, 0.25f) || bottles_passed(before, bottles_progress, 0.28f) ||
                bottles_passed(before, bottles_progress, 0.31f)) {
                jiggy_bk_sfx_play(BOTTLES_SFX_HEADSCRATCH, 16000.0f / 32767.0f, 1.4f + bottles_random() * 0.15f);
            } else if (bottles_passed(before, bottles_progress, 0.45f) || bottles_passed(before, bottles_progress, 0.48f) ||
                       bottles_passed(before, bottles_progress, 0.51f) || bottles_passed(before, bottles_progress, 0.7f) ||
                       bottles_passed(before, bottles_progress, 0.73f) || bottles_passed(before, bottles_progress, 0.76f)) {
                jiggy_bk_sfx_play(BOTTLES_SFX_HEADSCRATCH, 6000.0f / 32767.0f, 1.35f + bottles_random() * 0.15f);
            }
            break;
        case BOTTLES_DUCKING:
            bottles_progress += seconds / BOTTLES_EXIT_SECONDS;
            if (bottles_passed(before, bottles_progress, 0.35f)) {
                bottles_digging = jiggy_bk_sfx_play(BOTTLES_SFX_DIGGING, 26000.0f / 32767.0f, 1.4f);
            }
            if (bottles_passed(before, bottles_progress, 0.9f)) {
                bottles_stop_digging();
            }
            if (bottles_progress >= 1.0f) {
                bottles_stop_digging();
                bottles_action = BOTTLES_HIDDEN;
                bottles_progress = 0.0f;
            }
            break;
    }
    if (talk_open && frames > 0) {
        talk_update();
    }
}

static Gfx *bottles_pose_draw(Gfx *dl, s32 model, void *block, s32 animation, f32 progress, Gfx *gfx, u32 gfx_capacity,
                              Mtx *mtx, u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 scale[4][4];

    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, bottles_yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, bottles_pose.world);
    bottles_pose.world[3][0] = bottles_pos[0];
    bottles_pose.world[3][1] = bottles_pos[1];
    bottles_pose.world[3][2] = bottles_pos[2];

    if (progress > 0.9999f) {
        progress = 0.9999f;
    }
    bottles_pose.model = model;
    bottles_pose.block = block;
    bottles_pose.animation = animation;
    bottles_pose.progress = progress;
    bottles_pose.gfx = gfx;
    bottles_pose.gfx_capacity = gfx_capacity;
    bottles_pose.mtx = mtx;
    bottles_pose.mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(&bottles_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Bottles and his molehill once a frame, with the player.
Gfx *jiggy_bottles_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    s32 molehill_animation;
    f32 molehill_progress;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    if ((s32)current_map != bottles_last_map) {
        // Leaving the map mid-talk lets the player go.
        if (bottles_action != BOTTLES_HIDDEN || talk_open) {
            bottles_lock_player(FALSE);
            bottles_stop_digging();
        }
        bottles_last_map = current_map;
        bottles_placed = FALSE;
        bottles_spot = NULL;
        bottles_action = BOTTLES_HIDDEN;
        talk_open = FALSE;
        for (i = 0; i < ARRAY_COUNT_OF(bottles_spots); i++) {
            if (bottles_spots[i].map == current_map) {
                bottles_spot = &bottles_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        bottles_talked = bottles_spot != NULL && jiggy_save_get(current_file, BOTTLES_SAVE_MAP_KEY(current_map)) != 0;
    }
    if (bottles_spot == NULL) {
        return dl;
    }
    if (bottles_state == BOTTLES_UNLOADED) {
        bottles_load();
    }
    if (bottles_state != BOTTLES_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!bottles_placed) {
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        bottles_place();
        bottles_armed = !bottles_player_near(actor, BOTTLES_REARM_RADIUS);
        bottles_last_timer = object_timer;
    }
    frames = object_timer - bottles_last_timer;
    bottles_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    bottles_update(actor, frames);

    // The molehill: at rest (its pop-out's first frame, as chMolehill's state 1 holds it),
    // popping with him, or settling as he goes back down.
    molehill_animation = MOLEHILL_ANIM_ENTER;
    molehill_progress = 0.0f;
    if (bottles_action == BOTTLES_POPPING) {
        molehill_progress = bottles_progress;
    } else if (bottles_action == BOTTLES_DUCKING) {
        molehill_animation = MOLEHILL_ANIM_EXIT;
        molehill_progress = bottles_progress;
    }
    dl = bottles_pose_draw(dl, MOLEHILL_MODEL, molehill_block, molehill_animation, molehill_progress,
                           molehill_gfx[buffer], MOLEHILL_GFX_MAX, molehill_mtx[buffer], MOLEHILL_MTX_MAX);
    switch (bottles_action) {
        case BOTTLES_POPPING:
            dl = bottles_pose_draw(dl, BOTTLES_MODEL, bottles_block, BOTTLES_ANIM_ENTER, bottles_progress,
                                   bottles_gfx[buffer], BOTTLES_GFX_MAX, bottles_mtx[buffer], BOTTLES_MTX_MAX);
            break;
        case BOTTLES_TALKING:
            dl = bottles_pose_draw(dl, BOTTLES_MODEL, bottles_block, BOTTLES_ANIM_IDLE, bottles_progress,
                                   bottles_gfx[buffer], BOTTLES_GFX_MAX, bottles_mtx[buffer], BOTTLES_MTX_MAX);
            break;
        case BOTTLES_DUCKING:
            dl = bottles_pose_draw(dl, BOTTLES_MODEL, bottles_block, BOTTLES_ANIM_EXIT, bottles_progress,
                                   bottles_gfx[buffer], BOTTLES_GFX_MAX, bottles_mtx[buffer], BOTTLES_MTX_MAX);
            break;
        default:
            break;
    }
    return dl;
}

// A filled rectangle in the primitive color, blended over what's there.
static Gfx *box_fill(Gfx *dl, s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, r, g, b, a);
    gDPFillRectangle(dl++, x0, y0, x1, y1);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): his text
// box while he talks.
Gfx *jiggy_bottles_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (!talk_open || bottles_state != BOTTLES_READY || cc_number_of_players != 1) {
        return dl;
    }
    box_x = (D_global_asm_80744490 - BOX_WIDTH) / 2;
    box_y = D_global_asm_80744494 - BOX_BOTTOM_MARGIN - BOX_HEIGHT;

    // The box: a light border round a dark, see-through middle.
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gSPClearGeometryMode(dl++, G_ZBUFFER);
    gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    dl = box_fill(dl, box_x, box_y, box_x + BOX_WIDTH, box_y + BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x, box_y + BOX_HEIGHT - BOX_BORDER, box_x + BOX_WIDTH, box_y + BOX_HEIGHT, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x, box_y + BOX_BORDER, box_x + BOX_BORDER, box_y + BOX_HEIGHT - BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x + BOX_WIDTH - BOX_BORDER, box_y + BOX_BORDER, box_x + BOX_WIDTH, box_y + BOX_HEIGHT - BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x + BOX_BORDER, box_y + BOX_BORDER, box_x + BOX_WIDTH - BOX_BORDER, box_y + BOX_HEIGHT - BOX_BORDER,
                  0x10, 0x10, 0x28, BOX_ALPHA * talk_alpha / 0xFF);

    // His portrait, its mouth moving while the text types out.
    frame = 0;
    if (talk_shown < talk_length) {
        frame = (talk_ticks / PORTRAIT_TICKS_PER_FRAME) % (2 * portrait_frames - 2 > 0 ? 2 * portrait_frames - 2 : 1);
        if (frame >= portrait_frames) {
            frame = 2 * portrait_frames - 2 - frame;
        }
    }
    width = portrait_width[frame];
    height = portrait_height[frame];
    gDPPipeSync(dl++);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    gDPLoadTextureTile(dl++, portrait_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
        width, height,
        0, 0, width - 1, height - 1,
        0,
        G_TX_CLAMP, G_TX_CLAMP,
        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    {
        s32 px = box_x + BOX_BORDER + BOX_PAD;
        s32 py = box_y + (BOX_HEIGHT - height * PORTRAIT_SCALE) / 2;

        gSPTextureRectangle(dl++,
            px << 2, py << 2, (px + width * PORTRAIT_SCALE) << 2, (py + height * PORTRAIT_SCALE) << 2,
            G_TX_RENDERTILE,
            0, 0, (1 << 10) / PORTRAIT_SCALE, (1 << 10) / PORTRAIT_SCALE);
    }
    gDPPipeSync(dl++);

    // His name and the page so far, in DK64's dialogue font (set up as the game's own HUD
    // text is: patches_debug.c's displayGFXLoad).
    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SCALE * 32 + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"BOTTLES", 1);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    remaining = talk_shown;
    for (line = 0; line < talk_line_count && remaining > 0; line++) {
        s32 n = 0;

        while (talk_lines[line][n] != '\0' && n < remaining) {
            shown[n] = talk_lines[line][n];
            n++;
        }
        shown[n] = '\0';
        remaining -= n + 1;
        if (n == 0) {
            continue;
        }
        text_y = TEXT_Y(box_y + TEXT_FIRST_LINE_Y + line * TEXT_LINE_HEIGHT);
        dl = printStyledText(dl, TEXT_STYLE, text_x * 4, text_y * 4, (u8 *)shown, 1);
    }
    gDPPipeSync(dl++);
    return dl;
}
