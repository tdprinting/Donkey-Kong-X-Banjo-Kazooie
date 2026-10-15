// What Angry Aztec's Banjo-Kazooie characters share (jiggy_aztec.h): the text box, the
// cutscene lock, drawing a model at rest, the floor, and the player's attacks and shots.
//
// The text box is Bottles' (jiggy_bottles.c): the speaker's Banjo-Kazooie portrait, its mouth
// frames moving while the text types out (as gczoombox does), beside DK64's dialogue font
// (printStyledText style 6), with the speaker's talking sounds, A for the rest of the page and
// then the next one. Only one character talks at a time, so they share one box, drawn in the
// HUD pass (jiggy_aztec_talk_hud_draw, patches_ui.c). Portraits 64 pixels across (Grabba's,
// Trunker's) are halved to 32 when they're read, so every portrait is drawn the same size.
//
// While the box is up the player is held in the game's cutscene lock (bit 0x10 of
// D_global_asm_8076A0B1), which also brings the cutscene bars in; A is read from the raw pad.

#include "jiggy_aztec.h"
#include "ui.h"
#include "patches_ui.h"

#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SIZE 32
#define PORTRAIT_SCALE 2
#define PORTRAIT_MAX_FRAMES 10
#define PORTRAIT_TICKS_PER_FRAME 2
#define PORTRAIT_READ_MAX (64 * 64)
#define TEXT_STYLE 6 // DK64's dialogue font (uppercase letters, digits and . , ! ? : ; ' - ( ) %).
#define TEXT_LINE_HEIGHT 17
#define TEXT_NAME_Y 6
#define TEXT_FIRST_LINE_Y 24
// printStyledText's y for a box row (see jiggy_bottles.c).
#define TEXT_Y(y) (((y) - 117) * 4 / 5 + 120)
#define TEXT_MAX_LINES 4
#define TEXT_LINE_MAX 48
#define TEXT_CHARS_PER_FRAME 1
#define TALK_SFX_EVERY 5 // Characters typed out between voice sounds.
#define BOX_FADE_STEP 0x30
#define BOX_ALPHA 0xC8
#define VOICE_MAX 4

#define CUTSCENE_LOCK 0x10
// DK64 player states (Actor control_state) that are attacks.
#define STATE_SIMIAN_SLAM 0x1C
#define STATE_PUNCH 0x26
#define STATE_PUNCH_2 0x27
#define STATE_PUNCH_3 0x28
#define STATE_RUNNING_ATTACK 0x29
#define STATE_AERIAL_ATTACK 0x2B
#define STATE_AERIAL_ATTACK_DIDDY 0x2E
#define STATE_AERIAL_ATTACK_CHUNKY 0x2F
// How far the attacks reach past the thing's own radius, within this height; punches only hit
// what the player faces (within this many 12-bit angle units either side).
#define ATTACK_REACH 30.0f
#define SLAM_REACH 44.0f
#define ATTACK_HEIGHT 60.0f
#define PUNCH_ARC 0x300
// DK64's player action for a touch from an attacking enemy (see jiggy_grublins.c).
#define ACTION_ENEMY_HIT 0xE
// Banjo-Kazooie fades positional sounds out between 1250 and 2500 units; scaled.
#define SFX_NEAR 310.0f
#define SFX_FAR 625.0f

extern u8 cc_number_of_players;
extern s16 D_global_asm_80744490; // Screen width.
extern s16 D_global_asm_80744494; // Screen height.
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_8076A0B3;
extern u32 object_timer;
extern OSContPad D_global_asm_807ECD58; // Buttons held this frame, before the cutscene lock.
// The actors that update this frame (as jiggy_banjo_eggs.c reads them).
typedef struct {
    Actor *actor;
    s32 unk4;
} AztecActiveActor;
extern AztecActiveActor D_global_asm_807FB930[];
extern u16 D_global_asm_807FBB34; // How many.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z); // Yaw from (x, z) to the target.
// The floor under (x, z): writes its height to *y and returns nonzero, or leaves it as it was.
s32 func_global_asm_80667110(f32 x, f32 z, f32 *y);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
s32 jiggy_banjo_air_attack(Actor *player);

static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_SIZE * PORTRAIT_SIZE] __attribute__((aligned(8)));
static u16 portrait_read[PORTRAIT_READ_MAX] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static u32 portrait_loaded = 0xFFFFFFFF;

static s32 talk_owner = -1;
static const char *talk_name;
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_length; // Characters in the page, counting a space between lines.
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_a_held = TRUE;
static u16 talk_voice[VOICE_MAX];
static s32 talk_voice_count;
static f32 talk_voice_volume;
static f32 talk_voice_pitch;
static u32 talk_random_state = 0x415A5443;

static f32 talk_random(void) {
    talk_random_state = talk_random_state * 1664525 + 1013904223;
    return (talk_random_state >> 8) * (1.0f / 16777216.0f);
}

// Reads the portrait's mouth frames, halving any 64 pixels across to 32.
static void portrait_load(u32 portrait, s32 mouth_frames) {
    s32 count;
    s32 i;

    if (portrait == portrait_loaded) {
        return;
    }
    portrait_loaded = portrait;
    portrait_frames = 0;
    count = jiggy_bk_sprite_frame_count(portrait);
    if (count > mouth_frames) {
        count = mouth_frames;
    }
    if (count > PORTRAIT_MAX_FRAMES) {
        count = PORTRAIT_MAX_FRAMES;
    }
    for (i = 0; i < count; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(portrait, i, portrait_read, sizeof(portrait_read));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;
        s32 step = 1;
        s32 x, y;
        u16 *out = portrait_texels[portrait_frames];

        if (got <= 0 || width == 0 || height == 0 || width * height > PORTRAIT_READ_MAX) {
            continue;
        }
        if (width > PORTRAIT_SIZE || height > PORTRAIT_SIZE) {
            step = 2;
        }
        if (width / step > PORTRAIT_SIZE || height / step > PORTRAIT_SIZE) {
            continue;
        }
        for (y = 0; y < height / step; y++) {
            for (x = 0; x < width / step; x++) {
                out[y * (width / step) + x] = portrait_read[(y * step) * width + x * step];
            }
        }
        portrait_width[portrait_frames] = width / step;
        portrait_height[portrait_frames] = height / step;
        portrait_frames++;
    }
}

void aztec_lock_player(s32 locked) {
    if (locked) {
        D_global_asm_8076A0B1 |= CUTSCENE_LOCK;
        D_global_asm_8076A0B3 = 0;
    } else if (D_global_asm_8076A0B1 & CUTSCENE_LOCK) {
        D_global_asm_8076A0B1 &= ~CUTSCENE_LOCK;
        // The bars slide back out, as at the end of a cutscene (func_global_asm_8061D4E4).
        D_global_asm_8076A0B3 = 0xFF;
    }
}

s32 aztec_player_locked(void) {
    return (D_global_asm_8076A0B1 & CUTSCENE_LOCK) != 0;
}

// Breaks the page into lines that fit the box, a word at a time (as jiggy_bottles.c does).
static void talk_set_page(s32 page) {
    const char *text = talk_pages[page];
    s32 text_width = BOX_WIDTH - 2 * BOX_BORDER - BOX_PAD - PORTRAIT_SCALE * PORTRAIT_SIZE - 2 * BOX_PAD;
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

void aztec_talk_start(s32 owner, u32 portrait, s32 mouth_frames, const char *name, const char *const *pages, s32 count,
                      const u16 *voice, s32 voice_count, f32 voice_volume, f32 voice_pitch) {
    s32 i;

    portrait_load(portrait, mouth_frames);
    talk_owner = owner;
    talk_name = name;
    talk_pages = pages;
    talk_page_count = count;
    talk_voice_count = voice_count < VOICE_MAX ? voice_count : VOICE_MAX;
    for (i = 0; i < talk_voice_count; i++) {
        talk_voice[i] = voice[i];
    }
    talk_voice_volume = voice_volume;
    talk_voice_pitch = voice_pitch;
    talk_set_page(0);
    talk_alpha = 0;
    // A held from before doesn't count as a press.
    talk_a_held = TRUE;
    aztec_lock_player(TRUE);
}

s32 aztec_talk_owner(void) {
    return talk_owner;
}

void aztec_talk_cancel(void) {
    if (talk_owner >= 0) {
        talk_owner = -1;
        aztec_lock_player(FALSE);
    }
}

s32 aztec_talk_update(s32 owner) {
    s32 a_held = (D_global_asm_807ECD58.button & A_BUTTON) != 0;
    s32 a_pressed = a_held && !talk_a_held;
    s32 before = talk_shown;

    if (talk_owner != owner) {
        return FALSE;
    }
    aztec_lock_player(TRUE);
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
        if (talk_voice_count > 0 && talk_shown / TALK_SFX_EVERY != before / TALK_SFX_EVERY && talk_shown < talk_length) {
            u32 pick = (u32)(talk_random() * talk_voice_count) % (u32)talk_voice_count;

            jiggy_bk_sfx_play(talk_voice[pick], talk_voice_volume, talk_voice_pitch * (0.95f + talk_random() * 0.1f));
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
    talk_owner = -1;
    aztec_lock_player(FALSE);
    return TRUE;
}

f32 aztec_distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

f32 aztec_floor(f32 x, f32 y, f32 z, f32 probe_above, f32 max_step) {
    f32 probe = y + probe_above;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < max_step && y - floor < max_step) {
        return floor;
    }
    return y;
}

void aztec_turn_toward(f32 *yaw, f32 tx, f32 tz, f32 x, f32 z, f32 rate, u32 frames) {
    s16 target = func_global_asm_80665DE0(tx, tz, x, z) & 0xFFF;
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

s32 aztec_player_slams(Actor *player, const f32 pos[3], f32 radius) {
    f32 distance = aztec_distance_xz(pos[0], pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - pos[1];

    if (dy > ATTACK_HEIGHT || dy < -ATTACK_HEIGHT || distance > radius + SLAM_REACH) {
        return FALSE;
    }
    return player->control_state == STATE_SIMIAN_SLAM || jiggy_banjo_air_attack(player);
}

s32 aztec_player_attacks(Actor *player, const f32 pos[3], f32 radius) {
    f32 distance = aztec_distance_xz(pos[0], pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - pos[1];
    s32 to_target;
    s32 off;

    if (dy > ATTACK_HEIGHT || dy < -ATTACK_HEIGHT) {
        return FALSE;
    }
    switch (player->control_state) {
        case STATE_SIMIAN_SLAM:
            return distance <= radius + SLAM_REACH;
        case STATE_PUNCH:
        case STATE_PUNCH_2:
        case STATE_PUNCH_3:
            if (distance > radius + ATTACK_REACH) {
                return FALSE;
            }
            // Close in, a punch always lands; further out, only in front of the player.
            if (distance <= radius) {
                return TRUE;
            }
            to_target = func_global_asm_80665DE0(pos[0], pos[2], player->x_position, player->z_position) & 0xFFF;
            off = (to_target - (player->y_rotation & 0xFFF)) & 0xFFF;
            return off <= PUNCH_ARC || off >= 0x1000 - PUNCH_ARC;
        case STATE_RUNNING_ATTACK:
        case STATE_AERIAL_ATTACK:
        case STATE_AERIAL_ATTACK_DIDDY:
        case STATE_AERIAL_ATTACK_CHUNKY:
            return distance <= radius + ATTACK_REACH;
        default:
            // Banjo's Rat-a-tat Rap and Beak Buster, which DK64's states don't show.
            return jiggy_banjo_air_attack(player) && distance <= radius + ATTACK_REACH;
    }
}

s32 aztec_player_shot(Actor *player, const f32 pos[3], f32 radius, f32 height) {
    s32 i;

    for (i = 0; i < D_global_asm_807FBB34; i++) {
        Actor *shot = D_global_asm_807FB930[i].actor;
        f32 dy;

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
        dy = shot->y_position - pos[1];
        if (dy >= -10.0f && dy <= height && aztec_distance_xz(pos[0], pos[2], shot->x_position, shot->z_position) <= radius) {
            return TRUE;
        }
    }
    return FALSE;
}

void aztec_hurt_player(Actor *player, f32 x, f32 z) {
    // The knockback turns the player's heading round, so it's set toward the attacker first.
    player->unkEE = func_global_asm_80665DE0(x, z, player->x_position, player->z_position) & 0xFFF;
    setAction(ACTION_ENEMY_HIT, NULL, 0);
}

void aztec_sfx_at(Actor *player, f32 x, f32 z, u32 sfx, f32 volume, f32 pitch) {
    f32 distance = aztec_distance_xz(x, z, player->x_position, player->z_position);

    if (distance >= SFX_FAR) {
        return;
    }
    if (distance > SFX_NEAR) {
        volume *= (SFX_FAR - distance) / (SFX_FAR - SFX_NEAR);
    }
    jiggy_bk_sfx_play(sfx, volume, pitch);
}

u32 aztec_frames(u32 *last) {
    u32 frames = object_timer - *last;

    *last = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    return frames;
}

s32 aztec_model_load(u32 asset, void *block, u32 size) {
    s32 built = jiggy_bk_model_build(asset, block, size);

    return built > 0 && built <= (s32)size;
}

void aztec_pose_init(JiggyBkPose *pose) {
    s32 i;

    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        pose->selectors[i] = 0;
    }
    pose->selectors[1] = 1;
}

Gfx *aztec_model_draw(Gfx *dl, JiggyBkPose *pose, s32 model, void *block, const f32 pos[3], f32 yaw, f32 scale,
                      f32 stretch_y, f32 pitch, const f32 *offset, Gfx *gfx, u32 gfx_capacity, Mtx *mtx,
                      u32 mtx_capacity) {
    f32 rotate[4][4];
    f32 tilt[4][4];
    f32 sized[4][4];
    f32 turned[4][4];
    f32 s = AZTEC_BK_TO_DK64 * scale;

    guScaleF(sized, s, s * stretch_y, s);
    if (offset != NULL) {
        // Taken off in model units, before scaling.
        sized[3][0] = -offset[0] * s;
        sized[3][1] = -offset[1] * s * stretch_y;
        sized[3][2] = -offset[2] * s;
    }
    guRotateF(tilt, pitch, 1.0f, 0.0f, 0.0f);
    guMtxCatF(sized, tilt, turned);
    guRotateF(rotate, yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(turned, rotate, pose->world);
    pose->world[3][0] += pos[0];
    pose->world[3][1] += pos[1];
    pose->world[3][2] += pos[2];

    pose->model = model;
    pose->block = block;
    pose->animation = -1;
    pose->progress = 0.0f;
    pose->gfx = gfx;
    pose->gfx_capacity = gfx_capacity;
    pose->mtx = mtx;
    pose->mtx_capacity = mtx_capacity;
    if (jiggy_bk_model_pose(pose) <= 0) {
        return dl;
    }
    gDPPipeSync(dl++);
    gSPDisplayList(dl++, gfx);
    return dl;
}

// A filled rectangle in the primitive color, blended over what's there.
static Gfx *box_fill(Gfx *dl, s32 x0, s32 y0, s32 x1, s32 y1, u8 r, u8 g, u8 b, u8 a) {
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, r, g, b, a);
    gDPFillRectangle(dl++, x0, y0, x1, y1);
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): the text box
// of whichever Angry Aztec character is talking (drawn as jiggy_bottles_hud_draw draws Bottles').
Gfx *jiggy_aztec_talk_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (talk_owner < 0 || cc_number_of_players != 1) {
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

    // The portrait, its mouth moving while the text types out.
    if (portrait_frames > 0) {
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
    }

    // The name and the page so far, in DK64's dialogue font.
    text_x = box_x + BOX_BORDER + BOX_PAD + PORTRAIT_SCALE * PORTRAIT_SIZE + BOX_PAD;
    gSPDisplayList(dl++, &D_1000118);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0xF8, 0xC8, 0x40, talk_alpha);
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)talk_name, 1);
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
