// A text box shared by the Banjo-Kazooie characters of DK Isles (Brentilda, Cheato and
// Gruntilda's taunts), drawn as jiggy_bottles.c draws Bottles': the character's portrait from
// the player's Banjo-Kazooie ROM (its mouth frames moving while the text types out, as
// gczoombox does) beside DK64's dialogue font (printStyledText style 6), with the character's
// talking sounds every few characters.
//
// Three kinds of box:
// - Locked (JIGGY_TALK_LOCK): the game's cutscene lock holds the player (as jiggy_bottles.c
//   sets it) and A types out the rest of a page, then turns to the next.
// - Auto (JIGGY_TALK_AUTO): the player keeps playing and A isn't read (it's their jump); each
//   page stays up a moment once typed, then the next comes (Gruntilda's taunts, as Banjo-
//   Kazooie's lair taunts play while Banjo moves).
// - A question (jiggy_talk_ask): locked, with up to four options under the question, one
//   picked with the stick or the D-pad and A (Brentilda's quiz, like Grunty's Furnace Fun).
//
// Only one portrait is held at a time (decoded again when another character talks), so the
// box costs one 64x64 portrait's frames however many characters use it. The box is driven by
// its owner's update (jiggy_talk_update, once per game frame however often it's called) and
// drawn in the HUD pass (jiggy_talk_hud_draw, patches_ui.c). Leaving the map closes it.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"
#include "jiggy_talk.h"

// The text box, in screen pixels (jiggy_bottles.c's layout): the portrait in a 64x64 square on
// the left, the name and the text on the right.
#define BOX_WIDTH 288
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SIZE 64
#define PORTRAIT_MAX_FRAMES 8
#define PORTRAIT_MAX_TEXELS (64 * 64)
#define PORTRAIT_BAND_TEXELS 2048 // RGBA16 texels in TMEM (4 KiB) at once.
#define PORTRAIT_TICKS_PER_FRAME 2
#define TEXT_STYLE 6 // DK64's dialogue font (uppercase letters, digits and . , ! ? : ; ' - ( ) %).
#define TEXT_LINE_HEIGHT 17
#define TEXT_NAME_Y 6
#define TEXT_FIRST_LINE_Y 24
#define TEXT_BOTTOM_PAD 8
// printStyledText's y for a box row (see jiggy_bottles.c).
#define TEXT_Y(y) (((y) - 117) * 4 / 5 + 120)
#define TEXT_WIDTH (BOX_WIDTH - 2 * BOX_BORDER - BOX_PAD - PORTRAIT_SIZE - 2 * BOX_PAD)
#define TEXT_MAX_LINES 7 // A question: two lines and up to four options (and one spare).
#define TEXT_PAGE_LINES 4
#define TEXT_LINE_MAX 48
#define TEXT_CHARS_PER_FRAME 1
#define TALK_SFX_EVERY 5 // Characters typed out between talking sounds.
#define BOX_FADE_STEP 0x30
#define BOX_ALPHA 0xC8
// An auto box's page stays up this long (game frames) once typed out.
#define AUTO_HOLD_FRAMES 75
#define OPTIONS_MAX 4
// The stick has to go this far to move the pick.
#define STICK_THRESHOLD 40
// Banjo-Kazooie's pick sound as the cursor moves (SFX_90_SWITCH_PRESS, softly).
#define SFX_PICK 0x90

// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

extern Maps current_map;
extern u8 cc_number_of_players;
extern u32 object_timer;
extern s16 D_global_asm_80744490; // Screen width.
extern s16 D_global_asm_80744494; // Screen height.
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_8076A0B3;
extern OSContPad D_global_asm_807ECD58; // Buttons held this frame, before the cutscene lock.

static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static u32 portrait_asset; // The portrait decoded into portrait_texels, or 0.

static const JiggyTalker *talk_who;
static s32 talk_owner;
static u32 talk_flags;
static s32 talk_map = -1;
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static const char *talk_rest; // What didn't fit of the page, shown next.
static s32 talk_is_open;
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_length; // Characters in the page, counting a space between lines.
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_hold; // Auto: frames the typed page has stayed up.
static s32 talk_a_held = TRUE;
static u32 talk_last_timer;
static u32 talk_random_state = 0x54414C4B;

// A question: its options, the one picked, and the last answer.
static const char *const *ask_options;
static s32 ask_count;
static s32 ask_pick;
static s32 ask_question_lines;
static s32 ask_stick_held = TRUE;
static s32 answer_owner;
static s32 answer_pick = -1;

static f32 talk_random(void) {
    talk_random_state = talk_random_state * 1664525 + 1013904223;
    return (talk_random_state >> 8) * (1.0f / 16777216.0f);
}

// Decodes `who`'s portrait (its mouth frames) unless it's the one held. FALSE if it didn't.
static s32 talk_load_portrait(const JiggyTalker *who) {
    s32 count;
    s32 i;

    if (portrait_asset == who->portrait && portrait_frames > 0) {
        return TRUE;
    }
    portrait_asset = 0;
    portrait_frames = 0;
    if (!jiggy_bk_available()) {
        return FALSE;
    }
    count = jiggy_bk_sprite_frame_count(who->portrait);
    if (count <= 0) {
        return FALSE;
    }
    if (count > who->talk_frames) {
        count = who->talk_frames;
    }
    if (count > PORTRAIT_MAX_FRAMES) {
        count = PORTRAIT_MAX_FRAMES;
    }
    for (i = 0; i < count; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(who->portrait, i, portrait_texels[portrait_frames],
                                               sizeof(portrait_texels[0]));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        if (got <= 0 || width == 0 || height == 0 || width * height > PORTRAIT_MAX_TEXELS || width > PORTRAIT_SIZE ||
            height > PORTRAIT_SIZE) {
            continue;
        }
        portrait_width[portrait_frames] = width;
        portrait_height[portrait_frames] = height;
        portrait_frames++;
    }
    if (portrait_frames == 0) {
        return FALSE;
    }
    portrait_asset = who->portrait;
    return TRUE;
}

static void talk_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

// Breaks `text` into lines that fit the box, a word at a time, from line `first`. Returns the
// lines used; what didn't fit is left in talk_rest.
static s32 talk_wrap(const char *text, s32 first, s32 max_lines) {
    char candidate[TEXT_LINE_MAX];
    s32 line = first;

    talk_lines[line][0] = '\0';
    while (*text != '\0' && line < first + max_lines) {
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
        if (talk_lines[line][0] != '\0' && getCenterOfString(TEXT_STYLE, (u8 *)candidate) > TEXT_WIDTH) {
            // The word starts the next line.
            line++;
            if (line >= first + max_lines) {
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
    talk_rest = text;
    return (line < first + max_lines ? line + 1 : first + max_lines) - first;
}

static void talk_count_length(void) {
    s32 line;

    talk_length = 0;
    for (line = 0; line < talk_line_count; line++) {
        s32 i = 0;

        while (talk_lines[line][i] != '\0') {
            i++;
        }
        talk_length += i + 1;
    }
    talk_shown = 0;
    talk_ticks = 0;
    talk_hold = 0;
}

// Shows `text` (a page, or the rest of one that didn't fit).
static void talk_set_text(const char *text) {
    talk_line_count = talk_wrap(text, 0, TEXT_PAGE_LINES);
    talk_count_length();
}

static void talk_set_page(s32 page) {
    talk_page = page;
    talk_set_text(talk_pages[page]);
}

// Starts a box (the common part of jiggy_talk_say and jiggy_talk_ask).
static s32 talk_begin(s32 owner, const JiggyTalker *who, u32 flags) {
    if (cc_number_of_players != 1) {
        return FALSE;
    }
    if (talk_is_open && (!(talk_flags & JIGGY_TALK_AUTO) || (flags & JIGGY_TALK_AUTO))) {
        return FALSE;
    }
    if (!talk_load_portrait(who)) {
        return FALSE;
    }
    talk_who = who;
    talk_owner = owner;
    talk_flags = flags;
    talk_map = current_map;
    talk_is_open = TRUE;
    talk_alpha = 0;
    talk_pages = NULL;
    talk_page_count = 0;
    ask_options = NULL;
    ask_count = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    ask_stick_held = TRUE;
    talk_last_timer = object_timer;
    talk_lock_player((flags & JIGGY_TALK_LOCK) != 0);
    return TRUE;
}

s32 jiggy_talk_say(s32 owner, const JiggyTalker *who, const char *const *pages, s32 count, u32 flags) {
    if (count <= 0 || !talk_begin(owner, who, flags)) {
        return FALSE;
    }
    talk_pages = pages;
    talk_page_count = count;
    talk_set_page(0);
    return TRUE;
}

s32 jiggy_talk_ask(s32 owner, const JiggyTalker *who, const char *question, const char *const *options, s32 count) {
    s32 i;

    if (count <= 0 || !talk_begin(owner, who, JIGGY_TALK_LOCK)) {
        return FALSE;
    }
    if (count > OPTIONS_MAX) {
        count = OPTIONS_MAX;
    }
    ask_options = options;
    ask_count = count;
    ask_pick = 0;
    if (answer_owner == owner) {
        answer_pick = -1;
    }
    ask_question_lines = talk_wrap(question, 0, TEXT_MAX_LINES - count);
    talk_line_count = ask_question_lines;
    for (i = 0; i < count; i++) {
        s32 n = 0;

        // Room for the cursor in front.
        talk_lines[ask_question_lines + i][n++] = ' ';
        talk_lines[ask_question_lines + i][n++] = ' ';
        while (options[i][n - 2] != '\0' && n < TEXT_LINE_MAX - 1) {
            talk_lines[ask_question_lines + i][n] = options[i][n - 2];
            n++;
        }
        talk_lines[ask_question_lines + i][n] = '\0';
    }
    // Only the question types out; the options come up once it has.
    talk_count_length();
    return TRUE;
}

s32 jiggy_talk_answer(s32 owner) {
    return answer_owner == owner ? answer_pick : -1;
}

s32 jiggy_talk_open(s32 owner) {
    return talk_is_open && (owner < 0 || owner == talk_owner);
}

static void talk_end(void) {
    talk_is_open = FALSE;
    if (talk_flags & JIGGY_TALK_LOCK) {
        talk_lock_player(FALSE);
    }
}

void jiggy_talk_close(s32 owner) {
    if (talk_is_open && owner == talk_owner) {
        talk_end();
    }
}

// A question's frame: the stick or the D-pad moves the pick, A takes it.
static void talk_update_ask(s32 a_pressed) {
    s32 stick = D_global_asm_807ECD58.stick_y;
    u16 buttons = D_global_asm_807ECD58.button;
    s32 move = 0;

    if (stick > STICK_THRESHOLD || (buttons & U_JPAD)) {
        move = -1;
    } else if (stick < -STICK_THRESHOLD || (buttons & D_JPAD)) {
        move = 1;
    }
    if (move != 0 && !ask_stick_held) {
        ask_pick = (ask_pick + move + ask_count) % ask_count;
        jiggy_bk_sfx_play(SFX_PICK, 12000.0f / 32767.0f, 1.6f);
    }
    ask_stick_held = move != 0;
    if (a_pressed) {
        answer_owner = talk_owner;
        answer_pick = ask_pick;
        talk_end();
    }
}

void jiggy_talk_update(void) {
    s32 a_held;
    s32 a_pressed;
    s32 before;

    if (!talk_is_open || object_timer == talk_last_timer) {
        return;
    }
    talk_last_timer = object_timer;
    if ((s32)current_map != talk_map) {
        talk_end();
        return;
    }
    a_held = (D_global_asm_807ECD58.button & A_BUTTON) != 0;
    a_pressed = a_held && !talk_a_held && !(talk_flags & JIGGY_TALK_AUTO);
    talk_a_held = a_held;
    before = talk_shown;
    talk_ticks++;
    if (talk_flags & JIGGY_TALK_LOCK) {
        talk_lock_player(TRUE);
    }
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
        if (talk_shown / TALK_SFX_EVERY != before / TALK_SFX_EVERY && talk_shown < talk_length && talk_who->sfx_count > 0) {
            u32 pick = (u32)(talk_random() * talk_who->sfx_count) % talk_who->sfx_count;

            jiggy_bk_sfx_play(talk_who->sfx[pick], talk_who->sfx_volume, talk_who->sfx_pitch + talk_random() * 0.2f);
        }
        return;
    }
    if (ask_count > 0) {
        talk_update_ask(a_pressed);
        return;
    }
    if (talk_flags & JIGGY_TALK_AUTO) {
        if (++talk_hold < AUTO_HOLD_FRAMES) {
            return;
        }
    } else if (!a_pressed) {
        return;
    }
    if (talk_rest != NULL && *talk_rest != '\0') {
        talk_set_text(talk_rest);
        return;
    }
    if (talk_page + 1 < talk_page_count) {
        talk_set_page(talk_page + 1);
        return;
    }
    talk_end();
}

// A filled rectangle in the primitive color, blended over what's there.
static Gfx *box_fill(Gfx *dl, s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, r, g, b, a);
    gDPFillRectangle(dl++, x0, y0, x1, y1);
    return dl;
}

// The portrait's frame, drawn to fill the 64x64 square (a 32x32 one at twice the size), in
// bands of rows that each fit in TMEM.
static Gfx *talk_draw_portrait(Gfx *dl, s32 frame, s32 x, s32 y) {
    s32 width = portrait_width[frame];
    s32 height = portrait_height[frame];
    s32 scale = (width <= PORTRAIT_SIZE / 2 && height <= PORTRAIT_SIZE / 2) ? 2 : 1;
    s32 band = PORTRAIT_BAND_TEXELS / width;
    s32 px = x + (PORTRAIT_SIZE - width * scale) / 2;
    s32 py = y + (PORTRAIT_SIZE - height * scale) / 2;
    s32 row;

    gDPPipeSync(dl++);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    for (row = 0; row < height; row += band) {
        s32 rows = height - row < band ? height - row : band;

        gDPLoadTextureTile(dl++, portrait_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
            width, height,
            0, row, width - 1, row + rows - 1,
            0,
            G_TX_CLAMP, G_TX_CLAMP,
            G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(dl++,
            px << 2, (py + row * scale) << 2, (px + width * scale) << 2, (py + (row + rows) * scale) << 2,
            G_TX_RENDERTILE,
            0, row << 5, (1 << 10) / scale, (1 << 10) / scale);
    }
    gDPPipeSync(dl++);
    return dl;
}

Gfx *jiggy_talk_hud_draw(Gfx *dl) {
    s32 box_x, box_y, box_height;
    s32 text_x, text_y;
    s32 frame;
    s32 remaining;
    s32 line;
    s32 lines;
    char shown[TEXT_LINE_MAX];

    if (!talk_is_open || cc_number_of_players != 1 || portrait_frames == 0) {
        return dl;
    }
    if ((s32)current_map != talk_map) {
        talk_end();
        return dl;
    }
    lines = ask_count > 0 ? ask_question_lines + ask_count : TEXT_PAGE_LINES;
    box_height = TEXT_FIRST_LINE_Y + lines * TEXT_LINE_HEIGHT + TEXT_BOTTOM_PAD;
    if (box_height < PORTRAIT_SIZE + 2 * (BOX_BORDER + BOX_PAD)) {
        box_height = PORTRAIT_SIZE + 2 * (BOX_BORDER + BOX_PAD);
    }
    box_x = (D_global_asm_80744490 - BOX_WIDTH) / 2;
    box_y = D_global_asm_80744494 - BOX_BOTTOM_MARGIN - box_height;

    // The box: a light border round a dark, see-through middle.
    gDPPipeSync(dl++);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gSPClearGeometryMode(dl++, G_ZBUFFER);
    gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    dl = box_fill(dl, box_x, box_y, box_x + BOX_WIDTH, box_y + BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x, box_y + box_height - BOX_BORDER, box_x + BOX_WIDTH, box_y + box_height, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x, box_y + BOX_BORDER, box_x + BOX_BORDER, box_y + box_height - BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x + BOX_WIDTH - BOX_BORDER, box_y + BOX_BORDER, box_x + BOX_WIDTH, box_y + box_height - BOX_BORDER, 0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x + BOX_BORDER, box_y + BOX_BORDER, box_x + BOX_WIDTH - BOX_BORDER, box_y + box_height - BOX_BORDER,
                  0x10, 0x10, 0x28, BOX_ALPHA * talk_alpha / 0xFF);

    // The portrait, its mouth moving while the text types out.
    frame = 0;
    if (talk_shown < talk_length && portrait_frames > 1) {
        frame = (talk_ticks / PORTRAIT_TICKS_PER_FRAME) % (2 * portrait_frames - 2);
        if (frame >= portrait_frames) {
            frame = 2 * portrait_frames - 2 - frame;
        }
    }
    dl = talk_draw_portrait(dl, frame, box_x + BOX_BORDER + BOX_PAD, box_y + (box_height - PORTRAIT_SIZE) / 2);

    // The name and the page so far, in DK64's dialogue font (set up as the game's own HUD
    // text is: patches_debug.c's displayGFXLoad).
    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SIZE + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)talk_who->name, 1);
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
    // A question's options, once it's typed out: the pick marked and lit.
    if (ask_count > 0 && talk_shown >= talk_length) {
        for (line = 0; line < ask_count; line++) {
            char *text = talk_lines[ask_question_lines + line];

            // The font has no arrow: a dash marks the pick.
            text[0] = line == ask_pick ? '-' : ' ';
            gDPPipeSync(dl++);
            if (line == ask_pick) {
                gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
            } else {
                gDPSetPrimColor(dl++, 0, 0, 0xB0, 0xB0, 0xC0, talk_alpha);
            }
            text_y = TEXT_Y(box_y + TEXT_FIRST_LINE_Y + (ask_question_lines + line) * TEXT_LINE_HEIGHT);
            dl = printStyledText(dl, TEXT_STYLE, text_x * 4, text_y * 4, (u8 *)text, 1);
        }
    }
    gDPPipeSync(dl++);
    return dl;
}
