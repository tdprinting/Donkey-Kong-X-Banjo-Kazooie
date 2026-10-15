// Gruntilda, read from the player's Banjo-Kazooie ROM: what her Furnace Fun quiz in the Hideout
// Helm lobby (jiggy_grunty_quiz.c) and her broomstick fight in Hideout Helm
// (jiggy_grunty_boss.c) share, since only one of them is ever on screen.
//
// Her model is Banjo-Kazooie's final boss (ASSET_53D_MODEL_GRUNTILDA_FINAL_BOSS: Gruntilda
// astride her broomstick), posed with the final fight's animations (chFinalBossAnimationInfo).
// chfinalboss_draw shows her broomstick with appendage selectors 3 and 4 and the spell glowing
// in her hand with selector 5; grunty_draw does the same.
//
// Her text box is Bottles' (jiggy_bottles.c): her Banjo-Kazooie portrait
// (ASSET_816_SPRITE_GRUNTILDA, ten 64x64 frames, shrunk to 32x32 here and drawn twice the
// size, like the others) with its mouth moving as the text types out, her talking sounds
// (SFX_EC/ED/EE_GRUNTY_TALKING at 20000/32767, pitch 1.1, as gczoombox plays them) and DK64's
// dialogue font. For the quiz it also shows a choice, the way gcquiz does: the question, then
// the answers, one picked with the control stick (up and down, with gcquiz's 4-frame repeat
// delay) and A. Drawn in the HUD pass (jiggy_gruntilda_hud_draw, patches_ui.c).

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"
#include "jiggy_gruntilda.h"

#define GRUNTY_MODEL 0x53D    // ASSET_53D_MODEL_GRUNTILDA_FINAL_BOSS
#define GRUNTY_PORTRAIT 0x816 // ASSET_816_SPRITE_GRUNTILDA
// She converts to 27800 bytes, with 58 bones and about 20 pose commands with her broomstick.
#define GRUNTY_BLOCK_MAX 0x7200
#define GRUNTY_GFX_MAX 48
#define GRUNTY_MTX_MAX 64
// Her appendage selectors (chfinalboss_draw): the broomstick's two, and the spell in her hand.
#define GRUNTY_SELECTOR_BROOM_A 3
#define GRUNTY_SELECTOR_BROOM_B 4
#define GRUNTY_SELECTOR_GLOW 5

// Her talking sounds (gczoombox's Gruntilda entry).
static const u16 grunty_talk_sfx[] = {0xEC, 0xED, 0xEE};
#define GRUNTY_TALK_SFX_EVERY 5 // Characters typed out between sounds.
// Banjo-Kazooie fades sounds out between 1250 and 2500 units; scaled to DK64 units.
#define SFX_NEAR 310.0f
#define SFX_FAR 625.0f

// The text box (as jiggy_bottles.c draws it), in screen pixels; it grows for a choice.
#define BOX_WIDTH 288
#define BOX_MIN_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SCALE 2
#define PORTRAIT_SIZE 32 // Shrunk from the sprite's 64x64.
#define PORTRAIT_SOURCE_MAX_TEXELS (64 * 64)
#define PORTRAIT_FRAMES_MAX 10
#define PORTRAIT_TICKS_PER_FRAME 2
#define TEXT_STYLE 6 // DK64's dialogue font (uppercase letters, digits and . , ! ? : ; ' - ( ) %).
#define TEXT_LINE_HEIGHT 17
#define TEXT_NAME_Y 6
#define TEXT_FIRST_LINE_Y 24
#define TEXT_BOTTOM_PAD 10
// printStyledText's y for a box row (see jiggy_bottles.c).
#define TEXT_Y(y) (((y) - 117) * 4 / 5 + 120)
#define TEXT_MAX_LINES 5
#define TEXT_PAGE_LINES 4
// A choice's prompt gets the lines its answers leave (at least two).
#define TEXT_PROMPT_LINES_MIN 2
#define TEXT_LINE_MAX 48
#define TEXT_CHARS_PER_FRAME 1
#define BOX_FADE_STEP 0x30
#define BOX_ALPHA 0xC8
// The stick past this (of about 80) moves the answer cursor, then waits this many frames.
#define STICK_THRESHOLD 60
#define CURSOR_DELAY 4

// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

extern Maps current_map;
extern u8 D_global_asm_807444FC;
extern u32 object_timer;
extern s16 D_global_asm_80744490; // Screen width.
extern s16 D_global_asm_80744494; // Screen height.
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_8076A0B3;
extern u8 cc_number_of_players;
extern OSContPad D_global_asm_807ECD58; // Buttons held this frame, before the cutscene lock.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);

typedef enum {
    GRUNTY_UNLOADED,
    GRUNTY_READY,
    GRUNTY_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model or the portrait didn't convert.
} GruntyLoadState;

static u8 grunty_block[GRUNTY_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx grunty_gfx[2][GRUNTY_GFX_MAX];
static Mtx grunty_mtx[2][GRUNTY_MTX_MAX];
static JiggyBkPose grunty_pose;
static u16 portrait_source[PORTRAIT_SOURCE_MAX_TEXELS] __attribute__((aligned(8)));
static u16 portrait_texels[PORTRAIT_FRAMES_MAX][PORTRAIT_SIZE * PORTRAIT_SIZE] __attribute__((aligned(8)));
static s32 portrait_frames;
static GruntyLoadState grunty_state = GRUNTY_UNLOADED;
static u32 grunty_random_state = 0x4752554E;
static s32 grunty_locked;

// The talk: its lines, how much is typed out, the choice and its cursor.
static s32 talk_open;
static s32 talk_map;
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static s32 talk_choice;       // A choice, not pages.
static const char *const *talk_answers;
static s32 talk_answer_count;
static s32 talk_cursor;
static s32 talk_cursor_wait;
static s32 talk_answer;
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_typed_lines; // Lines typed out (a choice's answers come up at once after).
static s32 talk_length;      // Characters to type, counting a space between lines.
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_a_held = TRUE;

f32 grunty_random(void) {
    grunty_random_state = grunty_random_state * 1664525 + 1013904223;
    return (grunty_random_state >> 8) * (1.0f / 16777216.0f);
}

f32 grunty_random_between(f32 low, f32 high) {
    return low + grunty_random() * (high - low);
}

f32 grunty_distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

f32 grunty_floor(f32 x, f32 y, f32 z, f32 probe_above, f32 max_step) {
    f32 probe = y + probe_above;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < max_step && y - floor < max_step) {
        return floor;
    }
    return y;
}

void grunty_sfx(const f32 pos[3], Actor *player, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = grunty_distance_xz(pos[0], pos[2], player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

void grunty_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
        grunty_locked = TRUE;
    } else if (grunty_locked) {
        grunty_locked = FALSE;
        if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
            D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
            // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
            D_global_asm_8076A0B3 = 0xFF;
        }
    }
}

// Her portrait's 64x64 frame, shrunk to 32x32: each texel the average of four, opaque if
// at least two of them are.
static void portrait_shrink(const u16 *src, s32 width, s32 height, u16 *dst) {
    s32 x, y;

    for (y = 0; y < PORTRAIT_SIZE; y++) {
        for (x = 0; x < PORTRAIT_SIZE; x++) {
            s32 r = 0, g = 0, b = 0, a = 0, n = 0;
            s32 i, j;

            for (j = 0; j < 2; j++) {
                for (i = 0; i < 2; i++) {
                    s32 sx = x * 2 + i;
                    s32 sy = y * 2 + j;
                    u16 t;

                    if (sx >= width || sy >= height) {
                        continue;
                    }
                    t = src[sy * width + sx];
                    if (t & 1) {
                        r += (t >> 11) & 0x1F;
                        g += (t >> 6) & 0x1F;
                        b += (t >> 1) & 0x1F;
                        a++;
                    }
                    n++;
                }
            }
            if (a == 0 || a * 2 < n) {
                dst[y * PORTRAIT_SIZE + x] = 0;
            } else {
                dst[y * PORTRAIT_SIZE + x] = ((r / a) << 11) | ((g / a) << 6) | ((b / a) << 1) | 1;
            }
        }
    }
}

s32 grunty_ready(void) {
    s32 size;
    s32 count;
    s32 i;

    if (grunty_state != GRUNTY_UNLOADED) {
        return grunty_state == GRUNTY_READY;
    }
    grunty_state = GRUNTY_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return FALSE;
    }
    size = jiggy_bk_model_build(GRUNTY_MODEL, grunty_block, sizeof(grunty_block));
    if (size <= 0 || size > (s32)sizeof(grunty_block)) {
        return FALSE;
    }
    count = jiggy_bk_sprite_frame_count(GRUNTY_PORTRAIT);
    if (count <= 0) {
        return FALSE;
    }
    if (count > PORTRAIT_FRAMES_MAX) {
        count = PORTRAIT_FRAMES_MAX;
    }
    portrait_frames = 0;
    for (i = 0; i < count; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(GRUNTY_PORTRAIT, i, portrait_source, sizeof(portrait_source));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        if (got <= 0 || width == 0 || height == 0 || width * height > PORTRAIT_SOURCE_MAX_TEXELS) {
            continue;
        }
        portrait_shrink(portrait_source, width, height, portrait_texels[portrait_frames]);
        portrait_frames++;
    }
    if (portrait_frames == 0) {
        return FALSE;
    }
    // As Banjo-Kazooie's modelRender resets them for each model: branch 1 of selector 1, the
    // rest hidden (grunty_draw sets hers).
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        grunty_pose.selectors[i] = 0;
    }
    grunty_pose.selectors[1] = 1;
    grunty_state = GRUNTY_READY;
    return TRUE;
}

Gfx *grunty_draw(Gfx *dl, s32 animation, f32 progress, const f32 pos[3], f32 yaw, s32 broom, s32 glow, f32 scale) {
    s32 buffer = D_global_asm_807444FC;
    f32 rotate[4][4];
    f32 size[4][4];
    f32 s = GRUNTY_SCALE * scale;

    if (grunty_state != GRUNTY_READY || s <= 0.0001f) {
        return dl;
    }
    guScaleF(size, s, s, s);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(size, rotate, grunty_pose.world);
    grunty_pose.world[3][0] = pos[0];
    grunty_pose.world[3][1] = pos[1];
    grunty_pose.world[3][2] = pos[2];
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    grunty_pose.selectors[GRUNTY_SELECTOR_BROOM_A] = broom ? 1 : 0;
    grunty_pose.selectors[GRUNTY_SELECTOR_BROOM_B] = broom ? 1 : 0;
    grunty_pose.selectors[GRUNTY_SELECTOR_GLOW] = glow ? 1 : 0;
    grunty_pose.model = GRUNTY_MODEL;
    grunty_pose.block = grunty_block;
    grunty_pose.animation = animation;
    grunty_pose.progress = progress;
    grunty_pose.gfx = grunty_gfx[buffer];
    grunty_pose.gfx_capacity = GRUNTY_GFX_MAX;
    grunty_pose.mtx = grunty_mtx[buffer];
    grunty_pose.mtx_capacity = GRUNTY_MTX_MAX;
    if (jiggy_bk_model_pose(&grunty_pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, grunty_gfx[buffer]);
    return dl;
}

// Breaks text into lines that fit the box beside the portrait, a word at a time (as
// jiggy_bottles.c does), from line `first` and at most `max_lines` of them. Returns the line
// after the last one written.
static s32 talk_wrap(const char *text, s32 first, s32 max_lines) {
    s32 text_width = BOX_WIDTH - 2 * BOX_BORDER - BOX_PAD - PORTRAIT_SCALE * PORTRAIT_SIZE - 2 * BOX_PAD;
    char candidate[TEXT_LINE_MAX];
    s32 line = first;
    s32 last = first + max_lines;

    talk_lines[line][0] = '\0';
    while (*text != '\0' && line < last) {
        s32 word_length = 0;
        s32 i;
        s32 n;

        while (text[word_length] != '\0' && text[word_length] != ' ') {
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
            candidate[n++] = text[i];
        }
        candidate[n] = '\0';
        if (talk_lines[line][0] != '\0' && getCenterOfString(TEXT_STYLE, (u8 *)candidate) > text_width) {
            // The word starts the next line.
            line++;
            if (line >= last) {
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
    if (line < last && talk_lines[line][0] != '\0') {
        line++;
    }
    return line > first ? line : first + 1;
}

static void talk_count_typed(void) {
    s32 line;

    talk_length = 0;
    for (line = 0; line < talk_typed_lines; line++) {
        s32 i = 0;

        while (talk_lines[line][i] != '\0') {
            i++;
        }
        talk_length += i + 1;
    }
    talk_shown = 0;
    talk_ticks = 0;
}

static void talk_set_page(s32 page) {
    talk_page = page;
    talk_line_count = talk_wrap(talk_pages[page], 0, TEXT_PAGE_LINES);
    talk_typed_lines = talk_line_count;
    talk_count_typed();
}

static void talk_begin(void) {
    talk_open = TRUE;
    talk_map = current_map;
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    talk_answer = -1;
}

void grunty_talk_pages(const char *const *pages, s32 count) {
    if (count <= 0) {
        return;
    }
    talk_pages = pages;
    talk_page_count = count;
    talk_choice = FALSE;
    talk_set_page(0);
    talk_begin();
}

void grunty_talk_choice(const char *prompt, const char *const *answers, s32 answer_count) {
    s32 prompt_lines;
    s32 i;

    if (answer_count > GRUNTY_ANSWERS_MAX) {
        answer_count = GRUNTY_ANSWERS_MAX;
    }
    talk_choice = TRUE;
    talk_answers = answers;
    talk_answer_count = answer_count;
    talk_cursor = 0;
    talk_cursor_wait = 0;
    prompt_lines = TEXT_MAX_LINES - answer_count;
    if (prompt_lines < TEXT_PROMPT_LINES_MIN) {
        prompt_lines = TEXT_PROMPT_LINES_MIN;
    }
    // The answers start on the line after the prompt's last.
    talk_typed_lines = talk_wrap(prompt, 0, prompt_lines);
    talk_line_count = talk_typed_lines;
    for (i = 0; i < answer_count && talk_line_count < TEXT_MAX_LINES; i++) {
        s32 n = 0;

        while (answers[i][n] != '\0' && n < TEXT_LINE_MAX - 1) {
            talk_lines[talk_line_count][n] = answers[i][n];
            n++;
        }
        talk_lines[talk_line_count][n] = '\0';
        talk_line_count++;
    }
    talk_count_typed();
    talk_begin();
}

s32 grunty_talk_open(void) {
    if (talk_open && talk_map != (s32)current_map) {
        grunty_talk_close();
    }
    return talk_open;
}

s32 grunty_talk_answer(void) {
    return talk_answer;
}

void grunty_talk_close(void) {
    talk_open = FALSE;
    grunty_lock_player(FALSE);
}

s32 grunty_talk_update(void) {
    s32 a_held = (D_global_asm_807ECD58.button & A_BUTTON) != 0;
    s32 a_pressed = a_held && !talk_a_held;
    s32 before = talk_shown;

    if (!grunty_talk_open()) {
        return GRUNTY_TALK_NONE;
    }
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
        if (talk_shown / GRUNTY_TALK_SFX_EVERY != before / GRUNTY_TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 count = sizeof(grunty_talk_sfx) / sizeof(grunty_talk_sfx[0]);
            u32 pick = (u32)(grunty_random() * count) % count;

            jiggy_bk_sfx_play(grunty_talk_sfx[pick], 20000.0f / 32767.0f, 1.1f);
        }
        return GRUNTY_TALK_NONE;
    }
    if (talk_choice) {
        s32 stick = D_global_asm_807ECD58.stick_y;

        if (talk_cursor_wait > 0) {
            talk_cursor_wait--;
        } else if (stick > STICK_THRESHOLD && talk_cursor > 0) {
            talk_cursor--;
            talk_cursor_wait = CURSOR_DELAY;
        } else if (stick < -STICK_THRESHOLD && talk_cursor + 1 < talk_answer_count) {
            talk_cursor++;
            talk_cursor_wait = CURSOR_DELAY;
        }
        if (a_pressed) {
            talk_answer = talk_cursor;
            talk_open = FALSE;
            return GRUNTY_TALK_ANSWERED;
        }
        return GRUNTY_TALK_NONE;
    }
    if (!a_pressed) {
        return GRUNTY_TALK_NONE;
    }
    if (talk_page + 1 < talk_page_count) {
        talk_set_page(talk_page + 1);
        return GRUNTY_TALK_NONE;
    }
    talk_open = FALSE;
    return GRUNTY_TALK_DONE;
}

// A filled rectangle in the primitive color, blended over what's there.
static Gfx *box_fill(Gfx *dl, s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, r, g, b, a);
    gDPFillRectangle(dl++, x0, y0, x1, y1);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): her text box
// while she talks (drawn as jiggy_bottles_hud_draw draws Bottles').
Gfx *jiggy_gruntilda_hud_draw(Gfx *dl) {
    s32 box_x, box_y, box_height;
    s32 text_x, text_y;
    s32 frame;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (!grunty_talk_open() || grunty_state != GRUNTY_READY || cc_number_of_players != 1) {
        return dl;
    }
    box_height = TEXT_FIRST_LINE_Y + talk_line_count * TEXT_LINE_HEIGHT + TEXT_BOTTOM_PAD;
    if (box_height < BOX_MIN_HEIGHT) {
        box_height = BOX_MIN_HEIGHT;
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

    // Her portrait, its mouth moving while the text types out.
    frame = 0;
    if (talk_shown < talk_length && portrait_frames > 1) {
        frame = (talk_ticks / PORTRAIT_TICKS_PER_FRAME) % (2 * portrait_frames - 2);
        if (frame >= portrait_frames) {
            frame = 2 * portrait_frames - 2 - frame;
        }
    }
    gDPPipeSync(dl++);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    gDPLoadTextureTile(dl++, portrait_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
        PORTRAIT_SIZE, PORTRAIT_SIZE,
        0, 0, PORTRAIT_SIZE - 1, PORTRAIT_SIZE - 1,
        0,
        G_TX_CLAMP, G_TX_CLAMP,
        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    {
        s32 px = box_x + BOX_BORDER + BOX_PAD;
        s32 py = box_y + (box_height - PORTRAIT_SIZE * PORTRAIT_SCALE) / 2;

        gSPTextureRectangle(dl++,
            px << 2, py << 2, (px + PORTRAIT_SIZE * PORTRAIT_SCALE) << 2, (py + PORTRAIT_SIZE * PORTRAIT_SCALE) << 2,
            G_TX_RENDERTILE,
            0, 0, (1 << 10) / PORTRAIT_SCALE, (1 << 10) / PORTRAIT_SCALE);
    }
    gDPPipeSync(dl++);

    // Her name and the page so far, in DK64's dialogue font.
    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SCALE * PORTRAIT_SIZE + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xB0, 0xE0, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"GRUNTILDA", 1);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    remaining = talk_shown;
    for (line = 0; line < talk_typed_lines && remaining > 0; line++) {
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
    // A choice's answers, once the prompt is out: the one under the cursor lit and nudged in.
    if (talk_choice && talk_shown >= talk_length) {
        for (line = talk_typed_lines; line < talk_line_count; line++) {
            s32 answer = line - talk_typed_lines;
            s32 selected = answer == talk_cursor;

            gDPPipeSync(dl++);
            if (selected) {
                gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
            } else {
                gDPSetPrimColor(dl++, 0, 0, 0x90, 0x90, 0xA0, talk_alpha);
            }
            text_y = TEXT_Y(box_y + TEXT_FIRST_LINE_Y + line * TEXT_LINE_HEIGHT);
            dl = printStyledText(dl, TEXT_STYLE, (text_x + (selected ? 6 : 0)) * 4, text_y * 4, (u8 *)talk_lines[line], 1);
        }
    }
    gDPPipeSync(dl++);
    return dl;
}
