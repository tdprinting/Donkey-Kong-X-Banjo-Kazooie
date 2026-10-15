// Motzand: Mad Monster Mansion's ghostly organist (chMotzhand, src/MMM/ch/motzhand.c, and the
// organ game in src/MMM/code_3420.c), read from the player's Banjo-Kazooie ROM, haunting the top
// of Creepy Castle's hill, where the two worlds have collided.
//
// His hands (ASSET_88C_MODEL_MOTZHAND) float over the open top beside the castle door with their
// idle animation, turning to face the player. When the player stops by him he talks in rhyme in
// Bottles' text box (jiggy_bottles.c): Banjo-Kazooie's Motzand portrait (ASSET_83B_SPRITE_MOTZHAND,
// its fingers moving as the text types out) beside DK64's dialogue font, with his voice
// (SFX_10C_MUMMY_TALKING, as gczoombox plays it) and the game's cutscene lock holding the player.
//
// Then the organ game, as in Mad Monster Mansion's church: he plays a tune, a key at a time, with
// his playing animation (ASSET_A8_ANIM_MOTZHAND_PLAY) and the organ's note (SFX_3F3, pitched by
// the key, as the church organ plays it), the key lighting up on a keyboard drawn over the text
// box; then the player plays it back, the stick (or C-left and C-right) to pick a key and A to
// play it. The tunes are Banjo-Kazooie's own (sMotzhandPattern1 and 2: five keys, then ten).
// Getting the first right, he plays the second; a wrong key buzzes (COMUSIC_2C_BUZZER) and it's
// back to the first tune, as in Banjo-Kazooie. B leaves the game. Beating the second, his
// "beaten" music plays (COMUSIC_38_MOTZAND_BEATEN), he says his last rhyme and a Golden Banana
// (drawn as a Jiggy) appears beside him with Banjo-Kazooie's Jiggy jingle.
//
// The Golden Banana is a jiggy_rewards.c reward (id 0x71, docs/GAME_PLAN.md), which spawns it
// until it's collected. His progress is kept with the DK64 save file (jiggy_save_get/set, key
// 0x56010000 | map: Creepy Castle's characters' prefix 0x56, Motzand character 1).
//
// Drawn right after the player's own model (func_global_asm_80630DCC in patches_draw.c), his text
// box and the keyboard in the HUD pass (jiggy_motzand_hud_draw, patches_ui.c). Without a
// Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "ui.h"
#include "patches_ui.h"
#include "jiggy.h"

#define MOTZAND_MODEL 0x88C    // ASSET_88C_MODEL_MOTZHAND
#define MOTZAND_PORTRAIT 0x83B // ASSET_83B_SPRITE_MOTZHAND
// The hands convert to about 6 KiB, with 18 bones and about 50 pose commands.
#define MOTZAND_BLOCK_MAX 0x1800
#define MOTZAND_GFX_MAX 96
#define MOTZAND_MTX_MAX 24

// Banjo-Kazooie animations (chMotzhand_setState) and how long each plays, in seconds.
#define MOTZAND_ANIM_IDLE 0xA7 // ASSET_A7_ANIM_MOTZHAND_IDLE
#define MOTZAND_ANIM_PLAY 0xA8 // ASSET_A8_ANIM_MOTZHAND_PLAY
#define MOTZAND_IDLE_SECONDS 1.8f
// A key a beat as he plays (Banjo-Kazooie's is a whole 1.8 second loop and a glide between
// keys; faster here, where the keys are on the screen), the note at this far through.
#define MOTZAND_NOTE_SECONDS 0.8f
#define MOTZAND_NOTE_AT 0.45f

// Banjo-Kazooie sounds (sfx_e) and music (comusic_e).
#define MOTZAND_SFX_VOICE 0x10C  // SFX_10C_MUMMY_TALKING: his voice in the zoombox, 20000/32767, 1.01
#define MOTZAND_TALK_SFX_EVERY 5 // Characters typed out between sounds.
#define ORGAN_SFX_NOTE 0x3F3     // SFX_3F3: the church organ's note (a held sample, stopped after)
#define ORGAN_NOTE_FRAMES 15     // How long a note sounds, in game frames.
#define JINGLE_MOTZAND_BEATEN 0x38 // COMUSIC_38_MOTZAND_BEATEN
#define JINGLE_JIGGY_SPAWN 0x3D    // COMUSIC_3D_JIGGY_SPAWN, 0x7FFF/32767

// The organ: Banjo-Kazooie's keys are semitones (D_8038C198); these are the ones his tunes use,
// left to right, and whether each is a black key.
static const u8 organ_semitones[] = {7, 9, 10, 11, 13, 14, 15, 17, 18};
static const u8 organ_black[] = {0, 0, 1, 0, 1, 0, 1, 0, 1};
#define ORGAN_KEYS 9
// The organ's note is pitched by the key: this semitone plays at the sample's own pitch.
#define ORGAN_BASE_SEMITONE 12
// His tunes (sMotzhandPattern1 and 2), as keys above.
static const u8 tune_one[] = {0, 5, 4, 1, 7};
static const u8 tune_two[] = {2, 8, 6, 1, 5, 4, 1, 7, 3, 6};
#define TUNE_MAX 10

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define BK_TO_DK64 0.25f
// He floats this high over the floor, drawn at Banjo-Kazooie's size (his hands are spread about
// 160 units apart, as over the church organ), and leans this far toward each key he plays.
#define MOTZAND_HOVER 18.0f
#define MOTZAND_SCALE 1.0f
#define MOTZAND_KEY_SHIFT 5.0f
// Game frames per second, for object_timer.
#define GAME_FPS 30.0f
// How fast he turns to face the player, and how near the player has to be for him to look.
#define MOTZAND_TURN_RATE 0.25f
#define MOTZAND_LOOK_RADIUS 250.0f
// He talks when the player comes this close, on the ground; not again until the player has
// walked this far off; and a remark he's given before at most this often (as Bottles).
#define MOTZAND_REMIND_FRAMES (60 * 30) // A minute.
#define MOTZAND_TALK_RADIUS 70.0f
#define MOTZAND_REARM_RADIUS 250.0f
#define MOTZAND_TALK_FEET_ABOVE 40.0f
#define MOTZAND_TALK_FEET_BELOW 40.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
// Placing him: the floor is looked for from this far above his spot, trusted this close to it.
#define MOTZAND_FLOOR_PROBE 20.0f
#define MOTZAND_FLOOR_MAX_STEP 40.0f
// His Golden Banana floats this high over the floor.
#define REWARD_HEIGHT 12.5f
#define MOTZAND_REWARD 0x71

// DK64 player states (Actor control_state) he talks from.
#define STATE_IDLE 0x0C
#define STATE_WALK 0x0D
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10
// The stick has to go this far to pick the next key, and come back inside this to pick again.
#define STICK_PICK 40
#define STICK_REST 20

// The text box (as jiggy_mumbo.c draws it, with a 64x64 portrait), in screen pixels, and the
// keyboard over it.
#define BOX_WIDTH 288
#define BOX_HEIGHT 94
#define BOX_BOTTOM_MARGIN 44 // Clear of the cutscene bar along the bottom.
#define BOX_BORDER 2
#define BOX_PAD 8
#define PORTRAIT_SIZE 64
#define PORTRAIT_MAX_FRAMES 7
#define PORTRAIT_MAX_TEXELS (64 * 64)
#define PORTRAIT_BAND_TEXELS 2048 // 4 KiB of RGBA16.
#define PORTRAIT_TALK_FRAMES 7    // Frames 0 to 6 move his fingers.
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
#define KEY_WIDTH 28
#define KEY_HEIGHT 44
#define KEY_GAP 2
#define KEYBOARD_GAP 6 // Between the keyboard and the box.
// A key stays lit this long (game frames) after it's played.
#define KEY_LIT_FRAMES 10

// Where he is, map by map. Creepy Castle: the open top of the hill (DK64's banana bunches ring it,
// at y 1731), on its west side away from the castle door (1556, 1734, 1258), his Golden Banana
// beside him, both on unbroken floor and clear of the Tee-Hees' path to the north.
typedef struct {
    Maps map;
    f32 x, y, z;
    s16 yaw;                // Which way he faces with no one about (12-bit angle units, 0 is +z).
    f32 reward_x, reward_z; // Where his Golden Banana appears.
} MotzandPlacement;
static const MotzandPlacement motzand_spots[] = {
    {MAP_CASTLE, 1300.0f, 1731.0f, 1360.0f, 1024, 1370.0f, 1330.0f},
};

// What he says, in rhyme as in Banjo-Kazooie (its lines first). Each page is one press of A;
// lines are broken to fit the box when shown.
static const char *const motzand_meet[] = {
    "MOTZAND PLAYS HIS ORGAN WITH EASE, BUT CAN YOU FOLLOW HIM ON THE KEYS?",
    "WATCH MY FINGERS, HEAR MY TUNE, THEN PLAY IT BACK AND PLAY IT SOON!",
    "STICK TO PICK A KEY, THEN A TO PLAY. PRESS B IF YOU'D RATHER RUN AWAY!",
};
static const char *const motzand_retry[] = {
    "ONCE AGAIN I'LL PLAY MY TUNE, KEEP ON TRYING, YOU'LL GET IT SOON!",
};
static const char *const motzand_halfway[] = {
    "NOT BAD, BEAR, BUT NOW WATCH ME, COPY THIS TUNE AND AMAZED I'LL BE!",
};
static const char *const motzand_miss[] = {
    "I CAN'T BELIEVE YOU'RE SO SLOW, BACK TO THE START WE MUST GO!",
};
static const char *const motzand_quit[] = {
    "OFF YOU GO, YOU'VE HAD ENOUGH. COME BACK WHEN YOU'RE MADE OF STERNER STUFF!",
};
static const char *const motzand_complete[] = {
    "YOUR MUSIC SKILLS ARE MIGHTY FINE, SO TAKE THIS GOLDEN PIECE OF MINE!",
};
static const char *const motzand_after_waiting[] = {
    "MY GOLDEN PIECE IS WAITING THERE, GO AND GRAB IT, IF YOU DARE!",
};
static const char *const motzand_after[] = {
    "THE ORGAN'S QUIET, MY TUNE IS DONE. HAUNTING'S DULL, BUT THAT WAS FUN!",
};
static const char *const motzand_listen[] = {"WATCH AND LISTEN..."};
static const char *const motzand_your_turn[] = {"YOUR TURN! PLAY IT BACK. (B TO STOP)"};
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// Save key (jiggy_save_get): MOTZAND_SAVE_* bits.
#define MOTZAND_SAVE_MAP_KEY(map) (0x56010000 | (u32)(map))
#define MOTZAND_SAVE_MET 0x1 // He's given his first talk.
#define MOTZAND_SAVE_WON 0x2 // Both tunes played back: his Golden Banana is due.

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
    MOTZAND_UNLOADED,
    MOTZAND_READY,
    MOTZAND_UNAVAILABLE, // No Banjo-Kazooie ROM, or the model or the portrait didn't convert.
} MotzandLoadState;

typedef enum {
    GAME_NONE,     // Idling: no organ game.
    GAME_TALK,     // A talk that leads into the game (or out of it), then GAME_PLAYING or none.
    GAME_PLAYING,  // He plays the tune.
    GAME_LISTENING, // The player plays it back.
} MotzandGame;

typedef enum {
    TALK_MEET,
    TALK_RETRY,
    TALK_HALFWAY,
    TALK_MISS,
    TALK_QUIT,
    TALK_COMPLETE,
    TALK_AFTER_WAITING,
    TALK_AFTER,
} MotzandTalk;

static u8 motzand_block[MOTZAND_BLOCK_MAX] __attribute__((aligned(16)));
static Gfx motzand_gfx[2][MOTZAND_GFX_MAX];
static Mtx motzand_mtx[2][MOTZAND_MTX_MAX];
static JiggyBkPose motzand_pose;
static u16 portrait_texels[PORTRAIT_MAX_FRAMES][PORTRAIT_MAX_TEXELS] __attribute__((aligned(8)));
static s16 portrait_width[PORTRAIT_MAX_FRAMES];
static s16 portrait_height[PORTRAIT_MAX_FRAMES];
static s32 portrait_frames;
static MotzandLoadState motzand_state = MOTZAND_UNLOADED;

static s32 motzand_last_map = -1;
static u32 motzand_last_timer;
static const MotzandPlacement *motzand_spot; // This map's, or NULL.
static s32 motzand_placed;
static f32 motzand_pos[3]; // His floor spot (he's drawn MOTZAND_HOVER over it).
static f32 reward_spot[3];
static f32 motzand_yaw; // 12-bit angle units, as a float so the turn eases.
static s32 motzand_animation;
static f32 motzand_progress; // 0 to 1 through the animation.
static u32 motzand_saved;    // MOTZAND_SAVE_* bits.
static s32 motzand_armed;    // The player has been away since his last talk.
static u32 motzand_last_talk; // object_timer when his last talk began.
static s32 motzand_reward_jingle;
static u32 motzand_random_state = 0x4D4F545A;

// The organ game.
static MotzandGame game = GAME_NONE;
static MotzandTalk game_talk;  // The talk on screen in GAME_TALK.
static s32 game_tune;          // 0 or 1.
static s32 game_note;          // The note he's playing, or the player's next.
static f32 game_beat;          // 0 to 1 through his current note.
static s32 game_cursor;        // The key the player has picked.
static s32 game_key_lit = -1;  // The key lit up, and for how long.
static s32 game_key_lit_frames;
static s32 game_b_held = TRUE;
static s32 game_stick_ready;   // The stick has come back to rest since the last pick.
static s32 game_c_held = TRUE;
static s32 organ_handle;       // The sounding note's handle, or 0.
static s32 organ_frames;

// The dialogue (as jiggy_bottles.c keeps it).
static const char *const *talk_pages;
static s32 talk_page_count;
static s32 talk_page;
static s32 talk_open;
static s32 talk_status; // A status line in the game: shown whole, A does nothing.
static char talk_lines[TEXT_MAX_LINES][TEXT_LINE_MAX];
static s32 talk_line_count;
static s32 talk_length; // Characters in the page, counting a space between lines.
static s32 talk_shown;
static s32 talk_alpha;
static s32 talk_ticks;
static s32 talk_a_held = TRUE;

static f32 motzand_random(void) {
    motzand_random_state = motzand_random_state * 1664525 + 1013904223;
    return (motzand_random_state >> 8) * (1.0f / 16777216.0f);
}

static void motzand_load(void) {
    s32 size;
    s32 count;
    s32 i;

    motzand_state = MOTZAND_UNAVAILABLE;
    if (!jiggy_bk_available()) {
        return;
    }
    size = jiggy_bk_model_build(MOTZAND_MODEL, motzand_block, sizeof(motzand_block));
    if (size <= 0 || size > (s32)sizeof(motzand_block)) {
        return;
    }
    count = jiggy_bk_sprite_frame_count(MOTZAND_PORTRAIT);
    if (count <= 0) {
        return;
    }
    portrait_frames = 0;
    for (i = 0; i < count && i < PORTRAIT_TALK_FRAMES; i++) {
        s32 got = jiggy_bk_sprite_frame_rgba16(MOTZAND_PORTRAIT, i, portrait_texels[portrait_frames], sizeof(portrait_texels[0]));
        s32 width = (got >> 16) & 0xFFFF;
        s32 height = got & 0xFFFF;

        if (got <= 0 || width == 0 || height == 0 || width * height > PORTRAIT_MAX_TEXELS ||
            width > PORTRAIT_BAND_TEXELS) {
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
    // rest hidden.
    for (i = 0; i < JIGGY_BK_SELECTOR_COUNT; i++) {
        motzand_pose.selectors[i] = 0;
    }
    motzand_pose.selectors[1] = 1;
    motzand_state = MOTZAND_READY;
}

static f32 distance_xz(f32 ax, f32 az, f32 bx, f32 bz) {
    f32 dx = ax - bx;
    f32 dz = az - bz;

    return _sqrtf(dx * dx + dz * dz);
}

// The floor under (x, z), looked for from above `y`; `y` if there's none near.
static f32 motzand_floor(f32 x, f32 y, f32 z) {
    f32 probe = y + MOTZAND_FLOOR_PROBE;
    f32 floor = probe;

    if (func_global_asm_80667110(x, z, &floor) && floor != probe && floor - y < MOTZAND_FLOOR_MAX_STEP &&
        y - floor < MOTZAND_FLOOR_MAX_STEP) {
        return floor;
    }
    return y;
}

static void motzand_place(void) {
    motzand_pos[0] = motzand_spot->x;
    motzand_pos[1] = motzand_floor(motzand_spot->x, motzand_spot->y, motzand_spot->z);
    motzand_pos[2] = motzand_spot->z;
    reward_spot[0] = motzand_spot->reward_x;
    reward_spot[1] = motzand_floor(motzand_spot->reward_x, motzand_pos[1], motzand_spot->reward_z);
    reward_spot[2] = motzand_spot->reward_z;
    motzand_yaw = motzand_spot->yaw & 0xFFF;
    motzand_animation = MOTZAND_ANIM_IDLE;
    motzand_progress = motzand_random();
    motzand_reward_jingle = FALSE;
    motzand_placed = TRUE;
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

// The player can't move while he talks or plays (as jiggy_bottles.c holds them).
static void motzand_lock_player(s32 locked) {
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
    motzand_lock_player(TRUE);
}

static void motzand_save(u32 bits) {
    motzand_saved |= bits;
    jiggy_save_set(current_file, MOTZAND_SAVE_MAP_KEY(current_map), motzand_saved);
}

// A talk of the organ game (or after it).
static void game_say(MotzandTalk kind) {
    game = GAME_TALK;
    game_talk = kind;
    switch (kind) {
        case TALK_MEET:
            talk_show(motzand_meet, ARRAY_COUNT_OF(motzand_meet), FALSE);
            break;
        case TALK_RETRY:
            talk_show(motzand_retry, ARRAY_COUNT_OF(motzand_retry), FALSE);
            break;
        case TALK_HALFWAY:
            talk_show(motzand_halfway, ARRAY_COUNT_OF(motzand_halfway), FALSE);
            break;
        case TALK_MISS:
            talk_show(motzand_miss, ARRAY_COUNT_OF(motzand_miss), FALSE);
            break;
        case TALK_QUIT:
            talk_show(motzand_quit, ARRAY_COUNT_OF(motzand_quit), FALSE);
            break;
        case TALK_COMPLETE:
            talk_show(motzand_complete, ARRAY_COUNT_OF(motzand_complete), FALSE);
            break;
        case TALK_AFTER_WAITING:
            talk_show(motzand_after_waiting, ARRAY_COUNT_OF(motzand_after_waiting), FALSE);
            break;
        default:
            talk_show(motzand_after, ARRAY_COUNT_OF(motzand_after), FALSE);
            break;
    }
}

static const u8 *game_tune_keys(s32 *count) {
    if (game_tune == 0) {
        *count = ARRAY_COUNT_OF(tune_one);
        return tune_one;
    }
    *count = ARRAY_COUNT_OF(tune_two);
    return tune_two;
}

static void organ_stop(void) {
    if (organ_handle > 0) {
        jiggy_bk_sound_stop(organ_handle);
        organ_handle = 0;
    }
}

// Sounds a key of the organ and lights it up.
static void organ_play(s32 key) {
    s32 semitones = organ_semitones[key] - ORGAN_BASE_SEMITONE;
    f32 pitch = 1.0f;
    s32 i;

    // 2 to the power of semitones / 12, a semitone at a time.
    for (i = 0; i < semitones; i++) {
        pitch *= 1.0594631f;
    }
    for (i = 0; i > semitones; i--) {
        pitch /= 1.0594631f;
    }
    organ_stop();
    organ_handle = jiggy_bk_sfx_play(ORGAN_SFX_NOTE, 28000.0f / 32767.0f, pitch);
    organ_frames = ORGAN_NOTE_FRAMES;
    game_key_lit = key;
    game_key_lit_frames = KEY_LIT_FRAMES;
}

// He plays tune `tune` from the start.
static void game_start_tune(s32 tune) {
    game = GAME_PLAYING;
    game_tune = tune;
    game_note = 0;
    game_beat = 0.0f;
    motzand_animation = MOTZAND_ANIM_PLAY;
    motzand_progress = 0.0f;
    talk_show(motzand_listen, 1, TRUE);
}

// The game is over (left, or won): the player can move again.
static void game_end(void) {
    game = GAME_NONE;
    talk_open = FALSE;
    organ_stop();
    game_key_lit = -1;
    motzand_animation = MOTZAND_ANIM_IDLE;
    motzand_lock_player(FALSE);
}

// What the last page of a talk leads to.
static void game_talk_done(void) {
    switch (game_talk) {
        case TALK_MEET:
            motzand_save(MOTZAND_SAVE_MET);
            game_start_tune(0);
            break;
        case TALK_RETRY:
        case TALK_MISS:
            game_start_tune(0);
            break;
        case TALK_HALFWAY:
            game_start_tune(1);
            break;
        case TALK_COMPLETE:
            // His Golden Banana appears, with Banjo-Kazooie's Jiggy jingle.
            motzand_reward_jingle = TRUE;
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
        if (talk_shown / MOTZAND_TALK_SFX_EVERY != before / MOTZAND_TALK_SFX_EVERY && talk_shown < talk_length) {
            jiggy_bk_sfx_play(MOTZAND_SFX_VOICE, 20000.0f / 32767.0f, 0.95f + motzand_random() * 0.12f);
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

// A frame of the organ game while it's his turn or the player's.
static void game_update(u32 frames) {
    OSContPad *pad = &D_global_asm_807ECD58;
    s32 b_held = (pad->button & B_BUTTON) != 0;
    s32 b_pressed = b_held && !game_b_held;
    s32 a_held = (pad->button & A_BUTTON) != 0;
    s32 a_pressed = a_held && !talk_a_held;
    s32 c_held = (pad->button & (L_CBUTTONS | R_CBUTTONS)) != 0;
    s32 count;
    const u8 *keys = game_tune_keys(&count);
    f32 before;

    game_b_held = b_held;
    if (game_key_lit_frames > 0) {
        game_key_lit_frames -= frames;
        if (game_key_lit_frames <= 0) {
            game_key_lit = -1;
        }
    }
    if (organ_frames > 0) {
        organ_frames -= frames;
        if (organ_frames <= 0) {
            organ_stop();
        }
    }
    if (game == GAME_PLAYING) {
        talk_a_held = a_held;
        before = game_beat;
        game_beat += frames / GAME_FPS / MOTZAND_NOTE_SECONDS;
        motzand_progress = game_beat;
        if (before < MOTZAND_NOTE_AT && game_beat >= MOTZAND_NOTE_AT) {
            organ_play(keys[game_note]);
        }
        if (game_beat >= 1.0f) {
            game_beat = 0.0f;
            game_note++;
            if (game_note >= count) {
                // The player's turn, from the middle of the keyboard.
                game = GAME_LISTENING;
                game_note = 0;
                game_cursor = ORGAN_KEYS / 2;
                game_stick_ready = FALSE;
                game_c_held = TRUE;
                motzand_animation = MOTZAND_ANIM_IDLE;
                talk_show(motzand_your_turn, 1, TRUE);
            }
        }
        return;
    }
    // GAME_LISTENING.
    if (b_pressed) {
        organ_stop();
        game_key_lit = -1;
        motzand_animation = MOTZAND_ANIM_IDLE;
        game_say(TALK_QUIT);
        return;
    }
    {
        s32 step = 0;

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
        } else if (game_cursor >= ORGAN_KEYS) {
            game_cursor = ORGAN_KEYS - 1;
        }
    }
    talk_a_held = a_held;
    if (!a_pressed) {
        return;
    }
    organ_play(game_cursor);
    if (game_cursor != keys[game_note]) {
        // A wrong key: the buzzer, and back to the first tune (organMinigame_setState(4)).
        jiggy_bk_jingle_play(JIGGY_BK_JINGLE_BUZZER, 22000.0f / 32767.0f);
        game_say(TALK_MISS);
        return;
    }
    game_note++;
    if (game_note < count) {
        return;
    }
    if (game_tune == 0) {
        game_say(TALK_HALFWAY);
        return;
    }
    // Both tunes: he's beaten (organMinigame_setState(6)). Saved now, so the win stands whatever
    // happens before the Golden Banana's collected.
    motzand_save(MOTZAND_SAVE_WON);
    jiggy_bk_jingle_play(JINGLE_MOTZAND_BEATEN, 1.0f);
    motzand_animation = MOTZAND_ANIM_PLAY;
    motzand_progress = 0.0f;
    game_say(TALK_COMPLETE);
}

// Whether the player is close enough, and level enough with him, to talk.
static s32 motzand_player_near(Actor *player, f32 radius) {
    f32 dx = motzand_pos[0] - player->x_position;
    f32 dy = motzand_pos[1] - player->y_position;
    f32 dz = motzand_pos[2] - player->z_position;

    return dx * dx + dz * dz <= radius * radius && dy >= -MOTZAND_TALK_FEET_ABOVE && dy <= MOTZAND_TALK_FEET_BELOW;
}

// Turns a share of the way toward `target` (12-bit angle units), over `frames` frames.
static void motzand_turn_to(s16 target, u32 frames) {
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = (target & 0xFFF) - motzand_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        motzand_yaw += turn * MOTZAND_TURN_RATE;
        if (motzand_yaw < 0.0f) {
            motzand_yaw += 4096.0f;
        } else if (motzand_yaw >= 4096.0f) {
            motzand_yaw -= 4096.0f;
        }
    }
}

// What he'd say to the player now, outside the game.
static MotzandTalk motzand_next_talk(void) {
    if (!(motzand_saved & MOTZAND_SAVE_WON)) {
        return (motzand_saved & MOTZAND_SAVE_MET) ? TALK_RETRY : TALK_MEET;
    }
    return jiggy_reward_collected(MOTZAND_REWARD) ? TALK_AFTER : TALK_AFTER_WAITING;
}

// A frame of his behavior, `frames` game frames on.
static void motzand_update(Actor *player, u32 frames) {
    f32 seconds = frames / GAME_FPS;
    s16 to_player = func_global_asm_80665DE0(player->x_position, player->z_position, motzand_pos[0], motzand_pos[2]);
    MotzandTalk next;
    s32 is_new;

    if (frames == 0) {
        return;
    }
    if (talk_open || distance_xz(motzand_pos[0], motzand_pos[2], player->x_position, player->z_position) <= MOTZAND_LOOK_RADIUS) {
        motzand_turn_to(to_player, frames);
    } else {
        motzand_turn_to(motzand_spot->yaw, frames);
    }
    if (game != GAME_PLAYING) {
        // The beaten tune's playing goes on through his last rhyme; otherwise he idles.
        motzand_progress += seconds / (motzand_animation == MOTZAND_ANIM_PLAY ? MOTZAND_NOTE_SECONDS : MOTZAND_IDLE_SECONDS);
        while (motzand_progress >= 1.0f) {
            motzand_progress -= 1.0f;
        }
    }
    // His Golden Banana, while it's due.
    if ((motzand_saved & MOTZAND_SAVE_WON) && game == GAME_NONE) {
        if (!jiggy_reward_update(MOTZAND_REWARD, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], player,
                                 frames) &&
            motzand_reward_jingle) {
            jiggy_bk_jingle_play(JINGLE_JIGGY_SPAWN, 1.0f);
        }
        motzand_reward_jingle = FALSE;
    }
    if (game == GAME_PLAYING || game == GAME_LISTENING) {
        motzand_lock_player(TRUE);
        game_update(frames);
        if (talk_open) {
            talk_update();
        }
        return;
    }
    if (talk_open) {
        motzand_lock_player(TRUE);
        talk_update();
        return;
    }
    next = motzand_next_talk();
    is_new = next == TALK_MEET;
    if (!motzand_armed) {
        motzand_armed = !motzand_player_near(player, MOTZAND_REARM_RADIUS) &&
                        (is_new || next == TALK_RETRY || object_timer - motzand_last_talk >= MOTZAND_REMIND_FRAMES);
        return;
    }
    if (motzand_player_near(player, MOTZAND_TALK_RADIUS) && player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT &&
        (player->control_state == STATE_IDLE || (is_new && player->control_state == STATE_WALK))) {
        s16 to_motzand = func_global_asm_80665DE0(motzand_pos[0], motzand_pos[2], player->x_position, player->z_position);

        motzand_armed = FALSE;
        motzand_last_talk = object_timer;
        // The player turns to him (func_8028F94C(2, this->position)).
        player->y_rotation = to_motzand & 0xFFF;
        player->unkEE = to_motzand & 0xFFF;
        game_b_held = TRUE;
        game_say(next);
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and
// draws Motzand once a frame, with the player.
Gfx *jiggy_motzand_draw(Gfx *dl, Actor *actor) {
    s32 buffer = D_global_asm_807444FC;
    f32 rotate[4][4];
    f32 scale[4][4];
    f32 progress;
    f32 lean;
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over).
    if ((s32)current_map != motzand_last_map || object_timer < motzand_last_timer) {
        // Leaving the map mid-talk or mid-game lets the player go.
        if (talk_open || game != GAME_NONE) {
            game_end();
        }
        motzand_last_map = current_map;
        motzand_placed = FALSE;
        motzand_spot = NULL;
        game = GAME_NONE;
        talk_open = FALSE;
        for (i = 0; i < ARRAY_COUNT_OF(motzand_spots); i++) {
            if (motzand_spots[i].map == current_map) {
                motzand_spot = &motzand_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        motzand_saved = motzand_spot != NULL ? jiggy_save_get(current_file, MOTZAND_SAVE_MAP_KEY(current_map)) : 0;
    }
    if (motzand_spot == NULL) {
        motzand_last_timer = object_timer;
        return dl;
    }
    if (motzand_state == MOTZAND_UNLOADED) {
        motzand_load();
    }
    if (motzand_state != MOTZAND_READY) {
        return dl;
    }
    // Placed the first time the player stands on the map (after any entrance jump or fall).
    if (!motzand_placed) {
        motzand_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        motzand_place();
        motzand_armed = !motzand_player_near(actor, MOTZAND_REARM_RADIUS);
    }
    frames = object_timer - motzand_last_timer;
    motzand_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // He waits while someone else's cutscene holds the player.
    if ((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && !talk_open && game == GAME_NONE) {
        frames = 0;
    }
    motzand_update(actor, frames);

    // His hands, floating over his spot, leaning toward the key being played.
    lean = 0.0f;
    if (game_key_lit >= 0) {
        lean = (game_key_lit - (ORGAN_KEYS - 1) * 0.5f) * MOTZAND_KEY_SHIFT;
    }
    guScaleF(scale, BK_TO_DK64 * MOTZAND_SCALE, BK_TO_DK64 * MOTZAND_SCALE, BK_TO_DK64 * MOTZAND_SCALE);
    guRotateF(rotate, motzand_yaw * (360.0f / 4096.0f), 0.0f, 1.0f, 0.0f);
    guMtxCatF(scale, rotate, motzand_pose.world);
    // Facing the player (along x = sin, z = cos of his yaw), the player's right is his left:
    // (cos, -sin), so the keys lean the way they're laid out on the screen.
    motzand_pose.world[3][0] = motzand_pos[0] + lean * func_global_asm_80612790((s16)motzand_yaw);
    motzand_pose.world[3][1] = motzand_pos[1] + MOTZAND_HOVER;
    motzand_pose.world[3][2] = motzand_pos[2] - lean * func_global_asm_80612794((s16)motzand_yaw);
    progress = motzand_progress;
    if (progress > 0.9999f) {
        progress = 0.9999f;
    } else if (progress < 0.0f) {
        progress = 0.0f;
    }
    motzand_pose.model = MOTZAND_MODEL;
    motzand_pose.block = motzand_block;
    motzand_pose.animation = motzand_animation;
    motzand_pose.progress = progress;
    motzand_pose.gfx = motzand_gfx[buffer];
    motzand_pose.gfx_capacity = MOTZAND_GFX_MAX;
    motzand_pose.mtx = motzand_mtx[buffer];
    motzand_pose.mtx_capacity = MOTZAND_MTX_MAX;
    if (jiggy_bk_model_pose(&motzand_pose) > 0) {
        gDPPipeSync(dl++);
        gSPDisplayList(dl++, motzand_gfx[buffer]);
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

// The organ's keys over the text box: black and white, the one played lit up, the player's
// pick underlined.
static Gfx *keyboard_draw(Gfx *dl, s32 box_x, s32 box_y) {
    s32 width = ORGAN_KEYS * (KEY_WIDTH + KEY_GAP) - KEY_GAP;
    s32 x0 = box_x + (BOX_WIDTH - width) / 2;
    s32 y0 = box_y - KEYBOARD_GAP - KEY_HEIGHT;
    s32 key;

    dl = box_fill(dl, x0 - 4, y0 - 4, x0 + width + 4, y0 + KEY_HEIGHT + 8, 0x10, 0x10, 0x28,
                  BOX_ALPHA * talk_alpha / 0xFF);
    for (key = 0; key < ORGAN_KEYS; key++) {
        s32 kx = x0 + key * (KEY_WIDTH + KEY_GAP);
        u8 r, g, b;

        if (key == game_key_lit) {
            r = 0xF8;
            g = 0xC8;
            b = 0x40;
        } else if (organ_black[key]) {
            r = g = b = 0x30;
        } else {
            r = g = b = 0xE8;
        }
        dl = box_fill(dl, kx, y0, kx + KEY_WIDTH, y0 + KEY_HEIGHT, r, g, b, talk_alpha);
        if (game == GAME_LISTENING && key == game_cursor) {
            dl = box_fill(dl, kx, y0 + KEY_HEIGHT + 2, kx + KEY_WIDTH, y0 + KEY_HEIGHT + 6, 0xF0, 0x80, 0x20, talk_alpha);
        }
    }
    return dl;
}

// Called once per frame from the HUD pass of the general gfx loop (patches_ui.c): his text box
// and, through the organ game, the keyboard (drawn as jiggy_mumbo_hud_draw draws Mumbo's box).
Gfx *jiggy_motzand_hud_draw(Gfx *dl) {
    s32 box_x, box_y;
    s32 text_x, text_y;
    s32 frame;
    s32 width, height;
    s32 band;
    s32 remaining;
    s32 line;
    char shown[TEXT_LINE_MAX];

    if (!talk_open || motzand_state != MOTZAND_READY || cc_number_of_players != 1) {
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
    if (game == GAME_PLAYING || game == GAME_LISTENING) {
        dl = keyboard_draw(dl, box_x, box_y);
    }

    // His portrait, its fingers moving while the text types out or he plays: in bands of rows,
    // each one TMEM load.
    frame = 0;
    if (talk_shown < talk_length || game == GAME_PLAYING) {
        frame = (talk_ticks / PORTRAIT_TICKS_PER_FRAME) % (2 * portrait_frames - 2 > 0 ? 2 * portrait_frames - 2 : 1);
        if (frame >= portrait_frames) {
            frame = 2 * portrait_frames - 2 - frame;
        }
    }
    width = portrait_width[frame];
    height = portrait_height[frame];
    band = PORTRAIT_BAND_TEXELS / width;
    gDPPipeSync(dl++);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, talk_alpha);
    {
        s32 px = box_x + BOX_BORDER + BOX_PAD + (PORTRAIT_SIZE - width) / 2;
        s32 py = box_y + (BOX_HEIGHT - height) / 2;
        s32 row;

        for (row = 0; row < height; row += band) {
            s32 rows = height - row < band ? height - row : band;

            gDPLoadTextureTile(dl++, portrait_texels[frame], G_IM_FMT_RGBA, G_IM_SIZ_16b,
                width, height,
                0, row, width - 1, row + rows - 1,
                0,
                G_TX_CLAMP, G_TX_CLAMP,
                G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(dl++,
                px << 2, (py + row) << 2, (px + width) << 2, (py + row + rows) << 2,
                G_TX_RENDERTILE,
                0, row << 5, 1 << 10, 1 << 10);
        }
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
    dl = printStyledText(dl, TEXT_STYLE, text_x * 4, TEXT_Y(box_y + TEXT_NAME_Y) * 4, (u8 *)"MOTZAND", 1);
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
