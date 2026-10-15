// Tumblar: Mad Monster Mansion's talking box (chTumblar, src/MMM/ch/tumblar.c), read from the
// player's Banjo-Kazooie ROM, sitting on the grass by Creepy Castle's way in, where the two
// worlds have collided.
//
// In Banjo-Kazooie he sits in his shed over a floor of letters and the player rolls him from
// letter to letter to spell a word. Here his puzzle is a word puzzle in the HUD: when the player
// stops by him he greets them as Banjo-Kazooie's Tumblar does ("GREETINGS, O SOLID ONE!") in
// Bottles' text box (jiggy_bottles.c), with his portrait (ASSET_826_SPRITE_TUMBLAR, his eyes
// rolling as the text types out), his voice (SFX_10A_TUMBLAR_TALKING, as gczoombox plays it) and
// the game's cutscene lock holding the player. He gives a clue, and a row of letters appears over
// the box: the stick (or C-left and C-right) picks a letter and A rolls onto it. Each right
// letter joins the word; a wrong one buzzes (COMUSIC_2C_BUZZER), as Banjo-Kazooie's wrong
// letters set off a Tee-Hee, and the word starts over. Two words (GHOST, then COLLIDE), and he
// hops and spins with Banjo-Kazooie's puzzle-solved fanfare (COMUSIC_2D_PUZZLE_SOLVED_FANFARE),
// says his last line, and a Golden Banana (drawn as a Jiggy) appears beside him with
// Banjo-Kazooie's Jiggy jingle. B leaves the puzzle.
//
// He's a still model (ASSET_2F1, no bones); the player can't walk through him (pushed back to
// his edge). The Golden Banana is a jiggy_rewards.c reward (id 0x72, docs/GAME_PLAN.md). His
// progress is kept with the DK64 save file (jiggy_save_get/set, key 0x56020000 | map: Creepy
// Castle's characters' prefix 0x56, Tumblar character 2).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his text
// box and the letters in the HUD pass (jiggy_tumblar_hud_draw, patches_ui.c). Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

#define TUMBLAR_MODEL 0x2F1    // Mad Monster Mansion's Tumblar (unnamed in asset_e)
#define TUMBLAR_PORTRAIT 0x826 // ASSET_826_SPRITE_TUMBLAR
// He converts to about 9 KiB, with no bones and 6 pose commands.
#define TUMBLAR_BLOCK_MAX 0x2400
#define TUMBLAR_GFX_MAX 16
#define TUMBLAR_MTX_MAX 4

// Banjo-Kazooie sounds (sfx_e) and music (comusic_e).
#define TUMBLAR_SFX_VOICE 0x10A  // SFX_10A_TUMBLAR_TALKING: his voice in the zoombox, 20000/32767
#define TUMBLAR_TALK_SFX_EVERY 5 // Characters typed out between sounds.
#define TUMBLAR_SFX_ROLL 0x3EC   // SFX_3EC_CCW_DOOR_OPENING: his rolling (chTumblar's sliding), 0.6
#define JINGLE_PUZZLE_SOLVED 0x2D // COMUSIC_2D_PUZZLE_SOLVED_FANFARE
#define JINGLE_JIGGY_SPAWN 0x3D   // COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo (he's 150 units across).
#define BK_TO_DK64 0.25f
// The player is kept this far (across) from his middle, up to this high over his floor.
#define TUMBLAR_RADIUS 24.0f
#define TUMBLAR_HEIGHT 35.0f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// His celebration: this long, this many hops and whole turns, each hop this high.
#define CELEBRATE_SECONDS 2.5f
#define CELEBRATE_HOPS 4
#define CELEBRATE_TURNS 2
#define CELEBRATE_HOP_HEIGHT 14.0f
// He talks when the player comes this close, on the ground; not again until the player has
// walked this far off; and a remark he's given before at most this often (as Bottles).
#define TUMBLAR_REMIND_FRAMES (60 * 30) // A minute.
#define TUMBLAR_TALK_RADIUS 60.0f
#define TUMBLAR_REARM_RADIUS 220.0f
#define TUMBLAR_TALK_FEET_ABOVE 40.0f
#define TUMBLAR_TALK_FEET_BELOW 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing him: the floor is looked for from this far above his spot, trusted this close to it.
#define TUMBLAR_FLOOR_PROBE 20.0f
#define TUMBLAR_FLOOR_MAX_STEP 40.0f
// His Golden Banana floats this high over the floor.
#define REWARD_HEIGHT 12.5f
#define TUMBLAR_REWARD 0x72

// DK64 player states (Actor control_state) he talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10
// The stick has to go this far to pick the next letter, and come back inside this to pick again.
#define STICK_PICK 40
#define STICK_REST 20

// The words: the answer, and the letters he offers (the answer's, shuffled, and some that
// aren't). A letter can be used once.
typedef struct {
    const char *answer;
    const char *letters;
} TumblarWord;
static const TumblarWord tumblar_words[] = {
    {"GHOST", "TBGAOSHE"},
    {"COLLIDE", "LDNOCEIL"},
};
#define WORD_COUNT 2
#define LETTERS_MAX 8
#define ANSWER_MAX 8

// The text box (as jiggy_bottles.c draws it, his 32x32 portrait twice the size), in screen
// pixels, and the letters over it.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SIZE 64
#define PORTRAIT_MAX_FRAMES 7
#define PORTRAIT_MAX_TEXELS (32 * 32)
#define PORTRAIT_TALK_FRAMES 7 // Frames 0 to 6 roll his eyes; the rest turn him away.
#define PORTRAIT_TICKS_PER_FRAME 2
#define TEXT_STYLE 6 // DK64's dialogue font (uppercase letters, digits and . , ! ? : ; ' - ( ) %).
#define TEXT_LINE_HEIGHT 17
#define TEXT_NAME_Y 6
#define TEXT_FIRST_LINE_Y 24
// printStyledText's y for a box row (see jiggy_bottles.c).
#define TEXT_Y(y) (((y) - 117) * 4 / 5 + 120)
#define TEXT_MAX_LINES 4
#define TEXT_LINE_MAX 48
#define TEXT_CHARS_PER_FRAME 1
#define BOX_FADE_STEP 0x30
#define BOX_ALPHA 0xC8
#define CELL_WIDTH 28
#define CELL_HEIGHT 22
#define CELL_GAP 4
#define PANEL_ROW 20  // The word so far, over the letters.
#define PANEL_GAP 6   // Between the panel and the box.
#define LETTER_INSET 9 // A letter's left from its cell's.

// Where he is, map by map. Creepy Castle: on the open ground by the way in from the lobby
// (316, 391, 412), west of DK64's row of bananas toward the drawbridge, on unbroken floor; his
// Golden Banana appears beside him to the north.
typedef struct {
    Maps map;
    f32 x, y, z;
    s16 yaw;                // Which way he faces (12-bit angle units, 0 is +z).
    f32 reward_x, reward_z; // Where his Golden Banana appears.
} TumblarPlacement;
static const TumblarPlacement tumblar_spots[] = {
    {MAP_CASTLE, 255.0f, 391.0f, 300.0f, 0, 225.0f, 255.0f},
};

// What he says, in his grand way (Banjo-Kazooie's lines first). Each page is one press of A;
// lines are broken to fit the box when shown.
static const char *const tumblar_meet[] = {
    "GREETINGS, O SOLID ONE! I'M TUMBLAR THE MIGHTY!",
    "SOLVE MY PUZZLE AND YOU CAN TAKE THIS INFERNAL THING THAT'S STUCK UNDER ME!",
    "PICK MY LETTERS WITH THE STICK AND A, IN THE RIGHT ORDER. B IF YOU GIVE UP.",
};
static const char *const tumblar_again[] = {
    "BACK AGAIN, O SOLID ONE? THEN LET US ROLL!",
};
static const char *const tumblar_clue_one[] = {
    "FIRST WORD: WHAT FLOATS ABOUT A HAUNTED MANSION, GOING BOO?",
};
static const char *const tumblar_clue_two[] = {
    "CORRECT! NOW THE HARD ONE: WHAT DID YOUR TWO WORLDS DO, TO MAKE THIS PLACE?",
};
static const char *const tumblar_wrong[] = {
    "WRONG LETTER! I ROLL BACK TO THE START OF THE WORD. AGAIN, O SOLID ONE!",
};
static const char *const tumblar_quit[] = {
    "GIVING UP? TUMBLAR THE MIGHTY WILL WAIT. I'M NOT GOING ANYWHERE...",
};
static const char *const tumblar_complete[] = {
    "AT LAST, THE PUZZLE IS SOLVED! TAKE THIS THING, IT'S BEEN STUCK UNDER ME FOR AGES!",
};
static const char *const tumblar_after_waiting[] = {
    "WELL? THE SHINY THING IS RIGHT THERE, O SOLID ONE. TAKE IT!",
};
static const char *const tumblar_after[] = {
    "FREE AT LAST! TUMBLAR THE MIGHTY ROLLS WHERE HE PLEASES NOW.",
};
static const char *const tumblar_spell[] = {"SPELL IT OUT! (B TO STOP)"};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): TUMBLAR_SAVE_* bits.
#define TUMBLAR_SAVE_MAP_KEY(map) (0x56020000 | (u32)(map))
#define TUMBLAR_SAVE_MET 0x1 // He's given his first talk.
#define TUMBLAR_SAVE_WON 0x2 // Both words spelled: his Golden Banana is due.

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
// The floor under (x, z): writes its height to *y and returns nonzero, or leaves it as it was.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
f32 func_global_asm_80612790(s16 angle); // cos of a 12-bit angle
f32 func_global_asm_80612794(s16 angle); // sin of a 12-bit angle

typedef enum {
    TUMBLAR_UNLOADED,
    TUMBLAR_READY,
    TUMBLAR_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model or the portrait didn't convert.
} TumblarLoadState;

typedef enum {
    GAME_NONE,      // Sitting: no puzzle.
    GAME_TALK,      // A talk that leads into the puzzle (or out of it).
    GAME_SPELLING,  // The player picks letters.
    GAME_CELEBRATE, // Solved: he hops and spins.
} TumblarGame;

typedef enum {
    TALK_MEET,
    TALK_AGAIN,
    TALK_CLUE_ONE,
    TALK_CLUE_TWO,
    TALK_WRONG,
    TALK_QUIT,
    TALK_COMPLETE,
    TALK_AFTER_WAITING,
    TALK_AFTER,
} TumblarTalk;

static u8 tumblar_block[TUMBLAR_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx tumblar_gfx[2][TUMBLAR_GFX_MAX];
static Mtx tumblar_mtx[2][TUMBLAR_MTX_MAX];
static JiggyBkPose tumblar_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static TumblarLoadState tumblar_state = TUMBLAR_UNLOADED;

static s32 tumblar_last_map = -1;
static u32 tumblar_last_timer;
static const TumblarPlacement *tumblar_spot; // This map's, or NULL.
static s32 tumblar_placed;
static f32 tumblar_pos[3];
static f32 reward_spot[3];
static u32 tumblar_saved;     // TUMBLAR_SAVE_* bits.
static s32 tumblar_armed;     // The player has been away since his last talk.
static u32 tumblar_last_talk; // object_timer when his last talk began.
static s32 tumblar_reward_jingle;
static u32 tumblar_random_state = 0x54554D42;

// The puzzle.
static TumblarGame game = GAME_NONE;
static TumblarTalk game_talk;
static s32 game_word;     // 0 or 1.
static s32 game_spelled;  // Letters of the answer picked so far.
static u32 game_used;     // Bit i: the word's letter i has been used.
static s32 game_cursor;
static f32 game_celebrate; // 0 to 1 through his celebration.
static s32 game_b_held = TRUE;
static s32 game_c_held = TRUE;
static s32 game_stick_ready;

// The dialogue (as jiggy_bottles.c keeps it).
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static s32 talk_open;
static s32 talk_status; // A status line in the puzzle: shown whole, A does nothing.
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_length; // Characters in the page, counting a space between lines.
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_a_held = TRUE;

static f32 tumblar_random(void) {
    tumblar_random_state = tumblar_random_state * 1664525 + 1013904223;
    return (tumblar_random_state >> 8) * (1.0f / 16777216.0f);
}

static void tumblar_load(void) {
    s32 size;
    s32 count;
    s32 i;

    tumblar_state = TUMBLAR_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(TUMBLAR_MODEL, tumblar_block, sizeof(tumblar_block));
    if (size <= 0 || size > (s32)sizeof(tumblar_block)) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(TUMBLAR_PORTRAIT);
    if (count <= 0) {
        return;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(TUMBLAR_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
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
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        tumblar_pose.selectors[i] = 0;
    }
    tumblar_pose.selectors[1] = 1;
    tumblar_state = TUMBLAR_READY;
}

// The floor under (x, z), looked for from above `y`; `y` if there's none near.
static f32 tumblar_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + TUMBLAR_FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < TUMBLAR_FLOOR_MAX_STEP &&
        y - floor < TUMBLAR_FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

static void tumblar_place(void) {
    tumblar_pos[0] = tumblar_spot->x;
    tumblar_pos[1] = tumblar_floor(tumblar_spot->x, tumblar_spot->y, tumblar_spot->z);
    tumblar_pos[2] = tumblar_spot->z;
    reward_spot[0] = tumblar_spot->reward_x;
    reward_spot[1] = tumblar_floor(tumblar_spot->reward_x, tumblar_pos[1], tumblar_spot->reward_z);
    reward_spot[2] = tumblar_spot->reward_z;
    tumblar_reward_jingle = FALSE;
    tumblar_placed = TRUE;
}

// Breaks the page into lines that fit the box, a word at a time (as jiggy_bottles.c does).
static void talk_set_page(s32 page) {
    const char *text = talk_pages[page];
    s32 text_width = BOX_WIDTH - 2 * BOX_BORDER - BOX_PAD - PORTRAIT_SIZE - 2 * BOX_PAD;
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

// The player can't move while he talks or the puzzle is on (as jiggy_bottles.c holds them).
static void tumblar_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

static void talk_show(const char *const *pages, s32 count, s32 status) {
    talk_pages = pages;
    talk_page_count = count;
    talk_status = status;
    talk_set_page(0);
    if (status) {
        talk_shown = talk_length;
    }
    if (!talk_open) {
        talk_alpha = 0;
    }
    talk_open = TRUE;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    tumblar_lock_player(TRUE);
}

static void tumblar_save(u32 bits) {
    tumblar_saved |= bits;
    jiggy_save_set(current_file, TUMBLAR_SAVE_MAP_KEY(current_map), tumblar_saved);
}

static void game_say(TumblarTalk kind) {
    game = GAME_TALK;
    game_talk = kind;
    switch (kind) {
        case TALK_MEET:
            talk_show(tumblar_meet, ARRAY_COUNT_OF(tumblar_meet), FALSE);
            break;
        case TALK_AGAIN:
            talk_show(tumblar_again, ARRAY_COUNT_OF(tumblar_again), FALSE);
            break;
        case TALK_CLUE_ONE:
            talk_show(tumblar_clue_one, ARRAY_COUNT_OF(tumblar_clue_one), FALSE);
            break;
        case TALK_CLUE_TWO:
            talk_show(tumblar_clue_two, ARRAY_COUNT_OF(tumblar_clue_two), FALSE);
            break;
        case TALK_WRONG:
            talk_show(tumblar_wrong, ARRAY_COUNT_OF(tumblar_wrong), FALSE);
            break;
        case TALK_QUIT:
            talk_show(tumblar_quit, ARRAY_COUNT_OF(tumblar_quit), FALSE);
            break;
        case TALK_COMPLETE:
            talk_show(tumblar_complete, ARRAY_COUNT_OF(tumblar_complete), FALSE);
            break;
        case TALK_AFTER_WAITING:
            talk_show(tumblar_after_waiting, ARRAY_COUNT_OF(tumblar_after_waiting), FALSE);
            break;
        default:
            talk_show(tumblar_after, ARRAY_COUNT_OF(tumblar_after), FALSE);
            break;
    }
}

// The player spells word `word` from its first letter.
static void game_start_word(s32 word) {
    game = GAME_SPELLING;
    game_word = word;
    game_spelled = 0;
    game_used = 0;
    game_cursor = 0;
    game_stick_ready = FALSE;
    game_c_held = TRUE;
    game_b_held = TRUE;
    talk_show(tumblar_spell, 1, TRUE);
}

// The puzzle's over (left, or solved): the player can move again.
static void game_end(void) {
    game = GAME_NONE;
    talk_open = FALSE;
    tumblar_lock_player(FALSE);
}

// What the last page of a talk leads to.
static void game_talk_done(void) {
    switch (game_talk) {
        case TALK_MEET:
            tumblar_save(TUMBLAR_SAVE_MET);
            game_say(TALK_CLUE_ONE);
            break;
        case TALK_AGAIN:
            game_say(TALK_CLUE_ONE);
            break;
        case TALK_CLUE_ONE:
            game_start_word(0);
            break;
        case TALK_CLUE_TWO:
            game_start_word(1);
            break;
        case TALK_WRONG:
            game_start_word(game_word);
            break;
        case TALK_COMPLETE:
            // His Golden Banana appears, with Banjo-Kazooie's Jiggy jingle.
            tumblar_reward_jingle = TRUE;
            game_end();
            break;
        default:
            game_end();
            break;
    }
}

// The text box's frame: A to type out the rest of the page or go on to the next, and his voice
// while it types.
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
    if (talk_status) {
        return;
    }
    if (talk_shown < talk_length) {
        if (a_pressed) {
            talk_shown = talk_length;
        } else {
            talk_shown += TEXT_CHARS_PER_FRAME;
        }
        if (talk_shown / TUMBLAR_TALK_SFX_EVERY != before / TUMBLAR_TALK_SFX_EVERY && talk_shown < talk_length) {
            jiggy_bk_sfx_play(TUMBLAR_SFX_VOICE, 20000.0f / 32767.0f, 0.9f + tumblar_random() * 0.2f);
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
    if (game == GAME_TALK) {
        game_talk_done();
    }
}

static s32 word_letter_count(void) {
    const char *letters = tumblar_words[game_word].letters;
    s32 n = 0;

    while (letters[n] != '\0' && n < LETTERS_MAX) {
        n++;
    }
    return n;
}

// A frame of the puzzle while the player spells.
static void game_spell_update(void) {
    OSContPad *pad = &D_global_asm_807ECD58;
    const TumblarWord *word = &tumblar_words[game_word];
    s32 b_held = (pad->button & B_BUTTON) != 0;
    s32 a_held = (pad->button & A_BUTTON) != 0;
    s32 a_pressed = a_held && !talk_a_held;
    s32 c_held = (pad->button & (L_CBUTTONS | R_CBUTTONS)) != 0;
    s32 count = word_letter_count();
    s32 step = 0;

    if (b_held && !game_b_held) {
        game_b_held = b_held;
        game_say(TALK_QUIT);
        return;
    }
    game_b_held = b_held;
    if (pad->stick_x > -STICK_REST && pad->stick_x < STICK_REST) {
        game_stick_ready = TRUE;
    } else if (game_stick_ready && pad->stick_x >= STICK_PICK) {
        step = 1;
        game_stick_ready = FALSE;
    } else if (game_stick_ready && pad->stick_x <= -STICK_PICK) {
        step = -1;
        game_stick_ready = FALSE;
    }
    if (c_held && !game_c_held) {
        step = (pad->button & R_CBUTTONS) ? 1 : -1;
    }
    game_c_held = c_held;
    game_cursor += step;
    if (game_cursor < 0) {
        game_cursor = 0;
    } else if (game_cursor >= count) {
        game_cursor = count - 1;
    }
    talk_a_held = a_held;
    if (!a_pressed || (game_used & (1u << game_cursor))) {
        return;
    }
    // He rolls onto the letter.
    jiggy_bk_sfx_play(TUMBLAR_SFX_ROLL, 0.6f, 0.9f + tumblar_random() * 0.2f);
    if (word->letters[game_cursor] != word->answer[game_spelled]) {
        jiggy_bk_jingle_play(JIGGY_BK_JINGLE_BUZZER, 22000.0f / 32767.0f);
        game_say(TALK_WRONG);
        return;
    }
    game_used |= 1u << game_cursor;
    game_spelled++;
    if (word->answer[game_spelled] != '\0') {
        return;
    }
    if (game_word + 1 < WORD_COUNT) {
        game_say(TALK_CLUE_TWO);
        return;
    }
    // Solved. Saved now, so it stands whatever happens before the Golden Banana's collected.
    tumblar_save(TUMBLAR_SAVE_WON);
    jiggy_bk_jingle_play(JINGLE_PUZZLE_SOLVED, 1.0f);
    game = GAME_CELEBRATE;
    game_celebrate = 0.0f;
    talk_open = FALSE;
}

// Whether the player is close enough, and level enough with him, to talk.
static s32 tumblar_player_near(Actor *player, f32 radius) {
    f32 dx = tumblar_pos[0] - player->x_position;
    f32 dy = tumblar_pos[1] - player->y_position;
    f32 dz = tumblar_pos[2] - player->z_position;

    return dx * dx + dz * dz <= radius * radius && dy >= -TUMBLAR_TALK_FEET_ABOVE && dy <= TUMBLAR_TALK_FEET_BELOW;
}

// Keeps the player out of him: back to his edge.
static void tumblar_push_player(Actor *player) {
    f32 dx = player->x_position - tumblar_pos[0];
    f32 dz = player->z_position - tumblar_pos[2];
    f32 dy = player->y_position - tumblar_pos[1];
    f32 distance;

    if (dy < -TUMBLAR_TALK_FEET_BELOW || dy > TUMBLAR_HEIGHT || dx * dx + dz * dz >= TUMBLAR_RADIUS * TUMBLAR_RADIUS) {
        return;
    }
    distance = _sqrtf(dx * dx + dz * dz);
    if (distance < 0.01f) {
        dx = func_global_asm_80612794(tumblar_spot->yaw);
        dz = func_global_asm_80612790(tumblar_spot->yaw);
        distance = 1.0f;
    }
    player->x_position = tumblar_pos[0] + dx / distance * TUMBLAR_RADIUS;
    player->z_position = tumblar_pos[2] + dz / distance * TUMBLAR_RADIUS;
}

// What he'd say to the player now, outside the puzzle.
static TumblarTalk tumblar_next_talk(void) {
    if (!(tumblar_saved & TUMBLAR_SAVE_WON)) {
        return (tumblar_saved & TUMBLAR_SAVE_MET) ? TALK_AGAIN : TALK_MEET;
    }
    return jiggy_reward_collected(TUMBLAR_REWARD) ? TALK_AFTER : TALK_AFTER_WAITING;
}

// A frame of his behavior, `frames` game frames on.
static void tumblar_update(Actor *player, u32 frames) {
    TumblarTalk next;
    s32 is_new;

    if (frames == 0) {
        return;
    }
    // His Golden Banana, while it's due.
    if ((tumblar_saved & TUMBLAR_SAVE_WON) && game == GAME_NONE) {
        if (!jiggy_reward_update(TUMBLAR_REWARD, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], player,
                                 frames) &&
            tumblar_reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        }
        tumblar_reward_jingle = FALSE;
    }
    switch (game) {
        case GAME_CELEBRATE:
            tumblar_lock_player(TRUE);
            game_celebrate += frames / GAME_FPS / CELEBRATE_SECONDS;
            if (game_celebrate >= 1.0f) {
                game_celebrate = 0.0f;
                game_say(TALK_COMPLETE);
            }
            return;
        case GAME_SPELLING:
            tumblar_lock_player(TRUE);
            game_spell_update();
            if (talk_open) {
                talk_update();
            }
            return;
        case GAME_TALK:
            tumblar_lock_player(TRUE);
            talk_update();
            return;
        default:
            break;
    }
    next = tumblar_next_talk();
    is_new = next == TALK_MEET;
    if (!tumblar_armed) {
        tumblar_armed = !tumblar_player_near(player, TUMBLAR_REARM_RADIUS) &&
                        (is_new || next == TALK_AGAIN || object_timer - tumblar_last_talk >= TUMBLAR_REMIND_FRAMES);
        return;
    }
    if (tumblar_player_near(player, TUMBLAR_TALK_RADIUS) && player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT &&
        (player->control_state == STATE_IDLE || (is_new && player->control_state == STATE_WALK))) {
        s16 to_tumblar = func_global_asm_80665DE0(tumblar_pos[0], tumblar_pos[2], player->x_position, player->z_position);

        tumblar_armed = FALSE;
        tumblar_last_talk = object_timer;
        // The player turns to him (func_8028F94C(2, this->position)).
        player->y_rotation = to_tumblar & 0xFFF;
        player->unkEE = to_tumblar & 0xFFF;
        game_say(next);
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Tumblar once a frame, with the player.
Gfx *jiggy_tumblar_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 yaw;
    f32 hop = 0.0f;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over).
    if ((s32)current_map != tumblar_last_map || object_timer < tumblar_last_timer) {
        // Leaving the map mid-talk or mid-puzzle lets the player go.
        if (talk_open || game != GAME_NONE) {
            game_end();
        }
        tumblar_last_map = current_map;
        tumblar_placed = FALSE;
        tumblar_spot = NULL;
        game = GAME_NONE;
        talk_open = FALSE;
        for (i = 0; i < ARRAY_COUNT_OF(tumblar_spots); i++) {
            if (tumblar_spots[i].map == current_map) {
                tumblar_spot = &tumblar_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        tumblar_saved = tumblar_spot != NULL ? jiggy_save_get(current_file, TUMBLAR_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (tumblar_spot == NULL) {
        tumblar_last_timer = object_timer;
        return dl;
    }
    if (tumblar_state == TUMBLAR_UNLOADED) {
        tumblar_load();
    }
    if (tumblar_state != TUMBLAR_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!tumblar_placed) {
        tumblar_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        tumblar_place();
        tumblar_armed = !tumblar_player_near(actor, TUMBLAR_REARM_RADIUS);
    }
    frames = object_timer - tumblar_last_timer;
    tumblar_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && game == GAME_NONE) {
        frames = 0;
    }
    tumblar_push_player(actor);
    tumblar_update(actor, frames);

    // He sits facing his way; celebrating, he hops and spins.
    yaw = tumblar_spot->yaw;
    if (game == GAME_CELEBRATE) {
        f32 hop_phase = game_celebrate * CELEBRATE_HOPS;

        hop_phase -= (s32)hop_phase;
        hop = CELEBRATE_HOP_HEIGHT * 4.0f * hop_phase * (1.0f - hop_phase);
        yaw += game_celebrate * CELEBRATE_TURNS * 4096.0f;
    }
    guScaleF(scale, BK_TO_DK64, BK_TO_DK64, BK_TO_DK64);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, tumblar_pose.world);
    tumblar_pose.world[3][0] = tumblar_pos[0];
    tumblar_pose.world[3][1] = tumblar_pos[1] + hop;
    tumblar_pose.world[3][2] = tumblar_pos[2];
    tumblar_pose.model = TUMBLAR_MODEL;
    tumblar_pose.block = tumblar_block;
    tumblar_pose.animation = -1;
    tumblar_pose.progress = 0.0f;
    tumblar_pose.gfx = tumblar_gfx[buffer];
    tumblar_pose.gfx_capacity = TUMBLAR_GFX_MAX;
    tumblar_pose.mtx = tumblar_mtx[buffer];
    tumblar_pose.mtx_capacity = TUMBLAR_MTX_MAX;
    if (jiggy_bk_model_pose(&tumblar_pose) > 0) {
        gDPPipeSync(dl++);
        gSPDisplayList(dl++, tumblar_gfx[buffer]);
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

// Where the letters' panel goes, over the box: its left, top and the row of letters' top.
static void panel_layout(s32 box_x, s32 box_y, s32 *x0, s32 *y0, s32 *cells_y) {
    s32 count = word_letter_count();
    s32 width = count * (CELL_WIDTH + CELL_GAP) - CELL_GAP;

    *x0 = box_x + (BOX_WIDTH - width) / 2;
    *cells_y = box_y - PANEL_GAP - CELL_HEIGHT - 4;
    *y0 = *cells_y - PANEL_ROW;
}

// The panel's cells: used letters dimmed, the picked one lit.
static Gfx *panel_draw(Gfx *dl, s32 box_x, s32 box_y) {
    s32 count = word_letter_count();
    s32 x0, y0, cells_y;
    s32 i;

    panel_layout(box_x, box_y, &x0, &y0, &cells_y);
    dl = box_fill(dl, x0 - 6, y0 - 4, x0 + count * (CELL_WIDTH + CELL_GAP) - CELL_GAP + 6, cells_y + CELL_HEIGHT + 4,
                  0x10, 0x10, 0x28, BOX_ALPHA * talk_alpha / 0xFF);
    for (i = 0; i < count; i++) {
        s32 cx = x0 + i * (CELL_WIDTH + CELL_GAP);

        if (i == game_cursor) {
            dl = box_fill(dl, cx, cells_y, cx + CELL_WIDTH, cells_y + CELL_HEIGHT, 0xF0, 0x80, 0x20, talk_alpha);
        } else if (game_used & (1u << i)) {
            dl = box_fill(dl, cx, cells_y, cx + CELL_WIDTH, cells_y + CELL_HEIGHT, 0x30, 0x30, 0x40, talk_alpha);
        } else {
            dl = box_fill(dl, cx, cells_y, cx + CELL_WIDTH, cells_y + CELL_HEIGHT, 0x20, 0x70, 0x90, talk_alpha);
        }
    }
    return dl;
}

// The panel's text: the word so far (blanks for the rest) and the letters.
static Gfx *panel_text_draw(Gfx *dl, s32 box_x, s32 box_y) {
    const TumblarWord *word = &tumblar_words[game_word];
    s32 count = word_letter_count();
    s32 x0, y0, cells_y;
    char text[ANSWER_MAX * 2 + 1];
    char letter[2];
    s32 n = 0;
    s32 i;

    panel_layout(box_x, box_y, &x0, &y0, &cells_y);
    for (i = 0; word->answer[i] != '\0' && i < ANSWER_MAX; i++) {
        if (i > 0) {
            text[n++] = ' ';
        }
        text[n++] = i < game_spelled ? word->answer[i] : '-';
    }
    text[n] = '\0';
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, x0 * 4, TEXT_Y(y0) * 4, (u8 *)text, 1);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    letter[1] = '\0';
    for (i = 0; i < count; i++) {
        letter[0] = word->letters[i];
        dl = printStyledText(dl, TEXT_STYLE, (x0 + i * (CELL_WIDTH + CELL_GAP) + LETTER_INSET) * 4,
                             TEXT_Y(cells_y + 3) * 4, (u8 *)letter, 1);
    }
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): his text box
// and, through the puzzle, the letters (drawn as jiggy_bottles_hud_draw draws Bottles' box).
Gfx *jiggy_tumblar_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    s32 scale;
    char shown[TEXT_LINE_MAX];

    if (!talk_open || tumblar_state != TUMBLAR_READY || cc_number_of_players != 1) {
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
    if (game == GAME_SPELLING) {
        dl = panel_draw(dl, box_x, box_y);
    }

    // His portrait, its eyes rolling while the text types out, drawn at the box's portrait size.
    frame = 0;
    if (talk_shown < talk_length) {
        frame = (talk_ticks / PORTRAIT_TICKS_PER_FRAME) % (2 * portrait_frames - 2 > 0 ? 2 * portrait_frames - 2 : 1);
        if (frame >= portrait_frames) {
            frame = 2 * portrait_frames - 2 - frame;
        }
    }
    width = portrait_width[frame];
    height = portrait_height[frame];
    scale = PORTRAIT_SIZE / (width > height ? width : height);
    if (scale < 1) {
        scale = 1;
    }
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
        s32 px = box_x + BOX_BORDER + BOX_PAD + (PORTRAIT_SIZE - width * scale) / 2;
        s32 py = box_y + (BOX_HEIGHT - height * scale) / 2;

        gSPTextureRectangle(dl++,
            px << 2, py << 2, (px + width * scale) << 2, (py + height * scale) << 2,
            G_TX_RENDERTILE,
            0, 0, (1 << 10) / scale, (1 << 10) / scale);
    }
    gDPPipeSync(dl++);

    // His name and the page so far, in DK64's dialogue font.
    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SIZE + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"TUMBLAR", 1);
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
    if (game == GAME_SPELLING) {
        dl = panel_text_draw(dl, box_x, box_y);
    }
    gDPPipeSync(dl++);
    return dl;
}
