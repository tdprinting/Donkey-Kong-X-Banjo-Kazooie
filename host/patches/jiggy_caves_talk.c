// The text box Crystal Caves' Banjo-Kazooie characters talk in (jiggy_boggy.c, jiggy_wozza.c,
// jiggy_sir_slush.c), drawn as jiggy_bottles.c draws Bottles': the speaker's Banjo-Kazooie
// portrait (its mouth frames moving while the text types out, as gczoombox does) beside DK64's
// dialogue font (printStyledText style 6), with their talking sounds. A shows the rest of the
// page, then the next. While it's up the player is held in the game's cutscene lock.
//
// One box serves them all, so only one portrait is kept: it's decoded when a talk starts.
// Drawn in the HUD pass (jiggy_caves_talk_hud_draw, patches_ui.c).

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"
#include "jiggy_caves.h"

// The text box (as jiggy_bottles.c draws it), in screen pixels.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 10
#define PORTRAIT_MAX_TEXELS (32 * 32)
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
#define TALK_SFX_EVERY 5 // Characters typed out between talking sounds.
#define BOX_FADE_STEP 0x30
#define BOX_ALPHA 0xC8

extern Maps current_map;
extern u8 cc_number_of_players;
extern s16 D_global_asm_80744490; // Screen width.
extern s16 D_global_asm_80744494; // Screen height.
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_8076A0B3;
extern OSContPad D_global_asm_807ECD58; // Buttons held this frame, before the cutscene lock.

static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static u32 portrait_loaded = 0xFFFFFFFF; // The sprite in portrait_texels.

static const CavesSpeaker *talk_speaker;
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static s32 talk_open;
static s32 talk_hold;
static s32 talk_map;
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_length; // Characters in the page, counting a space between lines.
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_a_held = TRUE;
static u32 talk_random_state = 0x43415645;

static f32 talk_random(void) {
    talk_random_state = talk_random_state * 1664525 + 1013904223;
    return (talk_random_state >> 8) * (1.0f / 16777216.0f);
}

void jiggy_caves_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CAVES_CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CAVES_CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CAVES_CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

// Decodes the speaker's talking frames, unless they're the ones already loaded.
static s32 talk_load_portrait(const CavesSpeaker *speaker) {
    s32 count;
    s32 i;

    if (portrait_loaded == speaker->portrait && portrait_frames > 0) {
        return TRUE;
    }
    portrait_loaded = 0xFFFFFFFF;
    portrait_frames = 0;
    count = jiggy_bk_sprite_frame_count(speaker->portrait);
    if (count <= 0) {
        return FALSE;
    }
    if (count > PORTRAIT_MAX_FRAMES) {
        count = PORTRAIT_MAX_FRAMES;
    }
    for (i = 0; i < count && i < speaker->talk_frames; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(speaker->portrait, i, portrait_texels[portrait_frames],
                                               sizeof(portrait_texels[0]));
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
        return FALSE;
    }
    portrait_loaded = speaker->portrait;
    return TRUE;
}

// Breaks the page into lines that fit the box, a word at a time (as jiggy_bottles.c does).
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

s32 jiggy_caves_talk_start(const CavesSpeaker *speaker, const char *const *pages, s32 count, s32 hold) {
    if (talk_open || count <= 0 || !talk_load_portrait(speaker)) {
        return FALSE;
    }
    talk_speaker = speaker;
    talk_pages = pages;
    talk_page_count = count;
    talk_hold = hold;
    talk_map = current_map;
    talk_set_page(0);
    talk_open = TRUE;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    jiggy_caves_lock_player(TRUE);
    return TRUE;
}

s32 jiggy_caves_talk_is_open(void) {
    return talk_open;
}

void jiggy_caves_talk_cancel(void) {
    if (talk_open) {
        talk_open = FALSE;
        jiggy_caves_lock_player(FALSE);
    }
}

s32 jiggy_caves_talk_update(void) {
    s32 a_held = (D_global_asm_807ECD58.button & A_BUTTON) != 0;
    s32 a_pressed = a_held && !talk_a_held;
    s32 before = talk_shown;

    if (!talk_open) {
        return FALSE;
    }
    talk_a_held = a_held;
    talk_ticks++;
    if (talk_alpha < 0xFF) {
        talk_alpha += BOX_FADE_STEP;
        if (talk_alpha > 0xFF) {
            talk_alpha = 0xFF;
        }
    }
    // Held in place the whole while (another character's cutscene ending mustn't free them).
    jiggy_caves_lock_player(TRUE);
    if (talk_shown < talk_length) {
        if (a_pressed) {
            talk_shown = talk_length;
        } else {
            talk_shown += TEXT_CHARS_PER_FRAME;
        }
        if (talk_shown / TALK_SFX_EVERY != before / TALK_SFX_EVERY && talk_shown < talk_length &&
            talk_speaker->sfx_count > 0) {
            u32 pick = (u32)(talk_random() * talk_speaker->sfx_count) % talk_speaker->sfx_count;

            jiggy_bk_sfx_play(talk_speaker->sfx[pick], talk_speaker->volume,
                              talk_speaker->pitch - 0.05f + talk_random() * 0.1f);
        }
        return FALSE;
    }
    if (!a_pressed) {
        return FALSE;
    }
    if (talk_page + 1 < talk_page_count) {
        talk_set_page(talk_page + 1);
        return FALSE;
    }
    talk_open = FALSE;
    if (!talk_hold) {
        jiggy_caves_lock_player(FALSE);
    }
    return TRUE;
}

// A filled rectangle in the primitive color, blended over what's there.
static Gfx *box_fill(Gfx *dl, s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, r, g, b, a);
    gDPFillRectangle(dl++, x0, y0, x1, y1);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the box,
// while someone talks.
Gfx *jiggy_caves_talk_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (!talk_open) {
        return dl;
    }
    // Left the map mid-talk: the player goes free.
    if ((s32)current_map != talk_map || cc_number_of_players != 1) {
        jiggy_caves_talk_cancel();
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
    dl = box_fill(dl, box_x, box_y + BOX_HEIGHT - BOX_BORDER, box_x + BOX_WIDTH, box_y + BOX_HEIGHT, 0xF0, 0xD8, 0x78,
                  talk_alpha);
    dl = box_fill(dl, box_x, box_y + BOX_BORDER, box_x + BOX_BORDER, box_y + BOX_HEIGHT - BOX_BORDER, 0xF0, 0xD8, 0x78,
                  talk_alpha);
    dl = box_fill(dl, box_x + BOX_WIDTH - BOX_BORDER, box_y + BOX_BORDER, box_x + BOX_WIDTH, box_y + BOX_HEIGHT - BOX_BORDER,
                  0xF0, 0xD8, 0x78, talk_alpha);
    dl = box_fill(dl, box_x + BOX_BORDER, box_y + BOX_BORDER, box_x + BOX_WIDTH - BOX_BORDER, box_y + BOX_HEIGHT - BOX_BORDER,
                  0x10, 0x10, 0x28, BOX_ALPHA * talk_alpha / 0xFF);

    // The portrait, its mouth moving while the text types out.
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

    // The name and the page so far, in DK64's dialogue font.
    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SCALE * 32 + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)talk_speaker->name, 1);
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
