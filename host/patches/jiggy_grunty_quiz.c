// Grunty's Furnace Fun, the Hideout Helm lobby's way: Gruntilda hovers on her broomstick at
// the lobby's entrance ledge and quizzes the player, as she does on Banjo-Kazooie's Furnace
// Fun board (src/lair/code_5ED0.c, gcquiz in src/core2/code_91E10.c), on both games this time.
//
// The first time the player walks up to her she introduces the game in rhyme; then (and on
// every later visit) she asks whether they'll play. Each question comes with Banjo-Kazooie's
// question sound (SFX_12C_FF_QUESTION_START) and three answers in shuffled order, picked with
// the stick and A (jiggy_gruntilda.c). A right answer gets the quiz's ding (COMUSIC_2B_DING_B)
// and her audience's boos; a wrong one the buzzer (COMUSIC_2C_BUZZER), her audience's cheers,
// her laugh and a hurt: a quarter of a melon off the player's health (DK64's own hurt,
// func_global_asm_806C9974, with no knockback off the ledge into the lava). The hurt never
// takes the last quarter, so the quiz alone can't kill. Five right answers win: she grumbles a
// rhyme and a Jiggy (Golden Banana drawn as a Jiggy, jiggy_rewards.c reward 0x80) appears on
// the ledge beside her. Questions don't repeat until all have been asked.
//
// Saved with the file (jiggy_save key 0x58000000 | map): whether she's introduced the game and
// whether the player has won it. A won quiz stays won; she only taunts the player after.
//
// Drawn right after the player's own model (patches_draw.c); her text box is
// jiggy_gruntilda_hud_draw. Without a Banjo-Kazooie ROM nothing is drawn and nothing happens.

#include "common_structs.h"
#include "jiggy.h"
#include "jiggy_gruntilda.h"

// Where she hovers: the Hideout Helm lobby's entrance ledge (floor 190-194, from x 220 to 680 and
// z 280 to 360, found in the map's floors), at its east end, clear of the tag barrel (240, 231,
// 273) and the entrance (458, 191, 139). Her Jiggy appears at the ledge's west end. Both are
// placed over the floor found there at run time.
typedef struct {
    Maps map;
    f32 x, y, z;          // Where she hovers over (y the ledge's floor, if the floor isn't found).
    f32 reward_x, reward_z;
} QuizPlacement;
static const QuizPlacement quiz_spots[] = {
    {MAP_HELM_LOBBY, 590.0f, 193.0f, 322.0f, 360.0f, 318.0f},
};
#define QUIZ_HOVER 18.0f
#define QUIZ_BOB 4.0f
#define QUIZ_FLOOR_PROBE 40.0f
#define QUIZ_FLOOR_MAX_STEP 40.0f
#define REWARD_HEIGHT 12.5f
// She talks when the player stands this close, and again only after they've walked this far off.
#define QUIZ_TALK_RADIUS 85.0f
#define QUIZ_REARM_RADIUS 150.0f
#define QUIZ_TALK_HEIGHT 60.0f
#define PLAYER_AIRBORNE_HEIGHT 6.0f
#define QUIZ_TURN_RATE 0.12f
#define QUIZ_TO_WIN 5
// A wrong answer's hurt: a quarter melon (DK64's health is four a melon).
#define QUIZ_HURT 1
// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CUTSCENE_LOCK 0x10

#define QUIZ_SAVE_INTRODUCED 0x1
#define QUIZ_SAVE_WON 0x2

typedef struct {
    const char *question;
    const char *answers[3]; // The first is right; they're shuffled when asked.
} QuizQuestion;
static const QuizQuestion quiz_questions[] = {
    {"WHAT'S THE NAME OF BANJO'S LITTLE SISTER?", {"TOOTY", "TIPTUP", "TINY"}},
    {"WHO TEACHES BANJO AND KAZOOIE THEIR MOVES?", {"BOTTLES", "MUMBO", "CRANKY"}},
    {"WHAT COLOR ARE DIDDY'S BANANAS?", {"RED", "BLUE", "PURPLE"}},
    {"WHICH KONG PLAYS THE SAXOPHONE?", {"TINY", "LANKY", "CHUNKY"}},
    {"WHAT DOES MUMBO TAKE TO CAST A SPELL?", {"MUMBO TOKENS", "JIGGIES", "NOTES"}},
    {"WHO BREWS THE KONGS' POTIONS?", {"CRANKY", "FUNKY", "CANDY"}},
    {"WHAT KIND OF BIRD IS KAZOOIE?", {"A BREEGULL", "A PARROT", "A CROW"}},
    {"HOW MANY JINJOS HIDE IN EACH WORLD?", {"FIVE", "FOUR", "SEVEN"}},
    {"WHAT'S K. ROOL'S BIG GUN CALLED?", {"BLAST-O-MATIC", "BOOM-O-MATIC", "ZAP-O-TRON"}},
    {"WHICH KONG DOES THE ORANGSTAND?", {"LANKY", "DIDDY", "CHUNKY"}},
    {"WHAT'S THE NAME OF MY NICER SISTER?", {"BRENTILDA", "BLOBBELDA", "KLUNGILDA"}},
    {"WHAT DOES DK'S GUN SHOOT?", {"COCONUTS", "PEANUTS", "GRAPES"}},
    {"WHO IS MY LOYAL, DIM MINION?", {"KLUNGO", "KLUMP", "KRUSHA"}},
    {"WHAT DOES CHUNKY'S GUN SHOOT?", {"PINEAPPLES", "ORANGES", "FEATHERS"}},
    {"WHERE DOES MUMBO JUMBO LIVE?", {"IN A SKULL", "IN A TREE", "IN A BOOT"}},
    {"WHAT DOES BANJO CARRY KAZOOIE IN?", {"A BACKPACK", "A BASKET", "A BUCKET"}},
};
#define QUESTION_COUNT (sizeof(quiz_questions) / sizeof(quiz_questions[0]))
#define ARRAY_COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

// What she says, a page a press of A.
static const char *const quiz_intro[] = {
    "WELCOME, APE, TO FURNACE FUN, WHERE BRAINLESS KONGS GET OVERDONE!",
    "I ASK, YOU ANSWER, QUICK AND TRUE, GET ONE WRONG AND I'LL ZAP YOU!",
    "FIVE RIGHT ANSWERS WIN A PRIZE, A JIGGY FOR YOUR BEADY EYES!",
};
static const char *const quiz_ready_prompt = "SO, YOU HAIRY LUMP, WILL YOU PLAY MY GAME, OR RUN AWAY?";
static const char *const quiz_ready_answers[] = {"LET'S PLAY!", "NOT NOW."};
static const char *const quiz_declined[] = {
    "SCARED ALREADY? WHAT A SHAME! COME BACK WHEN YOU'RE NOT SO LAME!",
};
static const char *const quiz_right_1[] = {"GRR! THAT'S RIGHT, I MUST ADMIT, BUT DON'T GET COCKY, YOU HAIRY TWIT!"};
static const char *const quiz_right_2[] = {"LUCKY GUESS, YOU BANANA CHUMP, THE NEXT ONE'S SURE TO MAKE YOU SLUMP!"};
static const char *const quiz_right_3[] = {"RIGHT AGAIN? IT CAN'T BE SO! MY QUESTIONS MUST BE TOO EASY, THOUGH!"};
static const char *const quiz_wrong_1[] = {"WRONG! WRONG! WHAT A LOVELY SOUND! NOW TAKE THIS ZAP AND HIT THE GROUND!"};
static const char *const quiz_wrong_2[] = {"HA HA! YOUR BRAIN'S A PEANUT SHELL, AND NOW MY ZAP WILL MAKE YOU YELL!"};
static const char *const quiz_wrong_3[] = {"NOT EVEN CLOSE, YOU FOOLISH KONG, HOW SWEET IT IS WHEN YOU GET IT WRONG!"};
static const char *const *const quiz_right_lines[] = {quiz_right_1, quiz_right_2, quiz_right_3};
static const char *const *const quiz_wrong_lines[] = {quiz_wrong_1, quiz_wrong_2, quiz_wrong_3};
static const char *const quiz_won[] = {
    "CURSES! FIVE RIGHT? THIS CAN'T BE TRUE! MY FURNACE FUN WAS BEAT BY YOU!",
    "TAKE YOUR JIGGY, NOW GO AWAY, IN HELM I'LL MAKE YOU RUE THIS DAY!",
};
static const char *const quiz_after[] = {
    "YOU WON MY QUIZ, BUT DON'T BE PROUD, IN HELM I'LL BLAST YOU TO A CLOUD!",
};

typedef enum {
    QUIZ_UNLOADED,
    QUIZ_IDLE,       // Hovering; talks when the player comes close.
    QUIZ_INTRO,      // Introducing the game.
    QUIZ_READY,      // Play or not?
    QUIZ_DECLINED,
    QUIZ_QUESTION,
    QUIZ_REACTION,   // After an answer.
    QUIZ_WON,        // The win rhyme.
    QUIZ_AFTER,      // Her taunt after it's won.
} QuizAction;

extern Actor *gPlayerPointer;
extern Maps current_map;
extern u8 cc_number_of_players;
extern u32 object_timer;
extern u8 current_file;
extern u8 D_global_asm_8076A0B1;
extern u8 D_global_asm_807FC950[]; // PlayerProgress: health at 0x2FB, pending change at 0x2FD.
s16 func_global_asm_80665DE0(f32 target_x, f32 target_z, f32 x, f32 z);
s32 func_global_asm_806C9974(u8 playerIndex, s8 amount); // Changes the player's health.
f32 func_global_asm_80612794(s16 angle);
#define PLAYER_HEALTH (*(s8 *)&D_global_asm_807FC950[0x2FB])
#define PLAYER_HEALTH_CHANGE (*(s8 *)&D_global_asm_807FC950[0x2FD])

static s32 quiz_last_map = -1;
static u32 quiz_last_timer;
static const QuizPlacement *quiz_spot;
static s32 quiz_placed;
static f32 quiz_pos[3];
static f32 quiz_floor_y;
static f32 quiz_yaw;
static f32 quiz_progress;
static s32 quiz_anim;
static f32 quiz_anim_seconds;
static QuizAction quiz_action = QUIZ_UNLOADED;
static u32 quiz_saved;
static s32 quiz_armed;
static s32 quiz_right;
// Questions left to ask, shuffled; refilled when they run out.
static u8 quiz_order[QUESTION_COUNT];
static s32 quiz_next;
static const char *quiz_shown[3]; // This question's answers in the order shown.
static s32 quiz_correct;          // Which of those is right.
static f32 reward_spot[3];
static s32 reward_jingle;

static void quiz_play(s32 animation, f32 seconds) {
    quiz_anim = animation;
    quiz_anim_seconds = seconds;
    quiz_progress = 0.0f;
}

static void quiz_save(u32 bits) {
    quiz_saved |= bits;
    jiggy_save_set(current_file, GRUNTY_SAVE_KEY(GRUNTY_SAVE_QUIZ, current_map), quiz_saved);
}

static void quiz_shuffle(void) {
    u32 i;

    for (i = 0; i < QUESTION_COUNT; i++) {
        quiz_order[i] = i;
    }
    for (i = QUESTION_COUNT - 1; i > 0; i--) {
        u32 j = (u32)(grunty_random() * (i + 1)) % (i + 1);
        u8 t = quiz_order[i];

        quiz_order[i] = quiz_order[j];
        quiz_order[j] = t;
    }
    quiz_next = 0;
}

// Asks the next question, its answers in a fresh order (as gcquiz shuffles them).
static void quiz_ask(Actor *player) {
    const QuizQuestion *q;
    s32 slots[3] = {0, 1, 2};
    s32 i;

    if (quiz_next >= (s32)QUESTION_COUNT) {
        quiz_shuffle();
    }
    q = &quiz_questions[quiz_order[quiz_next++]];
    for (i = 2; i > 0; i--) {
        s32 j = (s32)(grunty_random() * (i + 1)) % (i + 1);
        s32 t = slots[i];

        slots[i] = slots[j];
        slots[j] = t;
    }
    for (i = 0; i < 3; i++) {
        quiz_shown[i] = q->answers[slots[i]];
        if (slots[i] == 0) {
            quiz_correct = i;
        }
    }
    quiz_action = QUIZ_QUESTION;
    jiggy_bk_sfx_play(GRUNTY_SFX_QUESTION, 1.0f, 1.0f);
    grunty_talk_choice(q->question, quiz_shown, 3);
    quiz_play(GRUNTY_ANIM_CAST, GRUNTY_CAST_SECONDS);
}

// A wrong answer's hurt: DK64's own, without a knockback (she's over a lava pit), and never the
// last quarter of a melon.
static void quiz_hurt_player(Actor *player) {
    if (PLAYER_HEALTH + PLAYER_HEALTH_CHANGE > QUIZ_HURT) {
        func_global_asm_806C9974(0, -QUIZ_HURT);
    }
    jiggy_bk_sfx_play(GRUNTY_SFX_BANJO_AYE, 28000.0f / 32767.0f, 1.0f);
}

static void quiz_answered(Actor *player, s32 answer) {
    s32 pick = (s32)(grunty_random() * 3) % 3;

    quiz_action = QUIZ_REACTION;
    if (answer == quiz_correct) {
        quiz_right++;
        jiggy_bk_jingle_play(GRUNTY_JINGLE_DING, 28000.0f / 32767.0f);
        jiggy_bk_sfx_play(GRUNTY_SFX_BOOING, 0x7FF8 / 32767.0f, 1.0f);
        quiz_play(GRUNTY_ANIM_OW, GRUNTY_OW_SECONDS);
        grunty_sfx(quiz_pos, player, GRUNTY_SFX_AH, 1.0f, grunty_random_between(0.95f, 1.05f));
        if (quiz_right >= QUIZ_TO_WIN) {
            quiz_action = QUIZ_WON;
            quiz_save(QUIZ_SAVE_WON);
            quiz_play(GRUNTY_ANIM_WEEEGH, GRUNTY_WEEEGH_SECONDS);
            grunty_sfx(quiz_pos, player, GRUNTY_SFX_WEEEGH, 1.0f, 1.0f);
            grunty_talk_pages(quiz_won, ARRAY_COUNT_OF(quiz_won));
            return;
        }
        // "Right again?" only once one's already been right.
        if (quiz_right == 1 && pick == 2) {
            pick = (s32)(grunty_random() * 2) % 2;
        }
        grunty_talk_pages(quiz_right_lines[pick], 1);
    } else {
        jiggy_bk_jingle_play(GRUNTY_JINGLE_BUZZER, 22000.0f / 32767.0f);
        jiggy_bk_sfx_play(GRUNTY_SFX_CHEERING, 1.0f, 1.0f);
        grunty_sfx(quiz_pos, player, pick == 0 ? GRUNTY_SFX_LAUGH_1 : GRUNTY_SFX_LAUGH_2, 1.0f,
                   grunty_random_between(0.95f, 1.05f));
        quiz_hurt_player(player);
        quiz_play(GRUNTY_ANIM_CAST, GRUNTY_CAST_SECONDS);
        grunty_talk_pages(quiz_wrong_lines[pick], 1);
    }
}

static void quiz_place(Actor *player) {
    quiz_floor_y = grunty_floor(quiz_spot->x, quiz_spot->y, quiz_spot->z, QUIZ_FLOOR_PROBE, QUIZ_FLOOR_MAX_STEP);
    quiz_pos[0] = quiz_spot->x;
    quiz_pos[1] = quiz_floor_y + QUIZ_HOVER;
    quiz_pos[2] = quiz_spot->z;
    reward_spot[0] = quiz_spot->reward_x;
    reward_spot[1] = grunty_floor(quiz_spot->reward_x, quiz_spot->y, quiz_spot->reward_z, QUIZ_FLOOR_PROBE, QUIZ_FLOOR_MAX_STEP);
    reward_spot[2] = quiz_spot->reward_z;
    quiz_yaw = func_global_asm_80665DE0(player->x_position, player->z_position, quiz_pos[0], quiz_pos[2]) & 0xFFF;
    quiz_action = QUIZ_IDLE;
    quiz_armed = TRUE;
    reward_jingle = FALSE;
    quiz_play(GRUNTY_ANIM_FLY, GRUNTY_FLY_SECONDS);
    quiz_placed = TRUE;
}

static void quiz_turn_to_player(Actor *player, u32 frames) {
    s16 target = func_global_asm_80665DE0(player->x_position, player->z_position, quiz_pos[0], quiz_pos[2]) & 0xFFF;
    u32 f;

    for (f = 0; f < frames; f++) {
        f32 turn = target - quiz_yaw;

        if (turn > 2048.0f) {
            turn -= 4096.0f;
        } else if (turn < -2048.0f) {
            turn += 4096.0f;
        }
        quiz_yaw += turn * QUIZ_TURN_RATE;
        if (quiz_yaw < 0.0f) {
            quiz_yaw += 4096.0f;
        } else if (quiz_yaw >= 4096.0f) {
            quiz_yaw -= 4096.0f;
        }
    }
}

static void quiz_update(Actor *player, u32 frames) {
    f32 seconds = frames / GRUNTY_GAME_FPS;
    f32 distance = grunty_distance_xz(quiz_pos[0], quiz_pos[2], player->x_position, player->z_position);
    f32 dy = player->y_position - quiz_floor_y;
    s32 grounded = player->y_position - player->floor <= PLAYER_AIRBORNE_HEIGHT;
    s32 event = GRUNTY_TALK_NONE;

    quiz_progress += seconds / quiz_anim_seconds;
    if (quiz_progress >= 1.0f) {
        // One-off reactions go back to flying in place.
        if (quiz_anim != GRUNTY_ANIM_FLY) {
            quiz_play(GRUNTY_ANIM_FLY, GRUNTY_FLY_SECONDS);
        } else {
            while (quiz_progress >= 1.0f) {
                quiz_progress -= 1.0f;
            }
        }
    }
    quiz_turn_to_player(player, frames);
    quiz_pos[1] = quiz_floor_y + QUIZ_HOVER + QUIZ_BOB * func_global_asm_80612794((s16)(object_timer * 40));
    if (!quiz_armed && distance > QUIZ_REARM_RADIUS) {
        quiz_armed = TRUE;
    }
    if (grunty_talk_open()) {
        event = grunty_talk_update();
    }

    switch (quiz_action) {
        case QUIZ_IDLE:
            if (quiz_armed && distance <= QUIZ_TALK_RADIUS && dy < QUIZ_TALK_HEIGHT && dy > -QUIZ_TALK_HEIGHT && grounded) {
                quiz_armed = FALSE;
                grunty_lock_player(TRUE);
                grunty_sfx(quiz_pos, player, GRUNTY_SFX_LAUGH_3, 1.0f, 1.0f);
                if (quiz_saved & QUIZ_SAVE_WON) {
                    quiz_action = QUIZ_AFTER;
                    grunty_talk_pages(quiz_after, ARRAY_COUNT_OF(quiz_after));
                } else if (!(quiz_saved & QUIZ_SAVE_INTRODUCED)) {
                    quiz_action = QUIZ_INTRO;
                    grunty_talk_pages(quiz_intro, ARRAY_COUNT_OF(quiz_intro));
                } else {
                    quiz_action = QUIZ_READY;
                    grunty_talk_choice(quiz_ready_prompt, quiz_ready_answers, 2);
                }
            }
            break;
        case QUIZ_INTRO:
            if (event == GRUNTY_TALK_DONE) {
                quiz_save(QUIZ_SAVE_INTRODUCED);
                quiz_action = QUIZ_READY;
                grunty_talk_choice(quiz_ready_prompt, quiz_ready_answers, 2);
            }
            break;
        case QUIZ_READY:
            if (event == GRUNTY_TALK_ANSWERED) {
                if (grunty_talk_answer() == 0) {
                    quiz_right = 0;
                    quiz_ask(player);
                } else {
                    quiz_action = QUIZ_DECLINED;
                    grunty_sfx(quiz_pos, player, GRUNTY_SFX_LAUGH_4, 1.0f, 1.0f);
                    grunty_talk_pages(quiz_declined, ARRAY_COUNT_OF(quiz_declined));
                }
            }
            break;
        case QUIZ_QUESTION:
            if (event == GRUNTY_TALK_ANSWERED) {
                quiz_answered(player, grunty_talk_answer());
            }
            break;
        case QUIZ_REACTION:
            if (event == GRUNTY_TALK_DONE) {
                quiz_ask(player);
            }
            break;
        case QUIZ_WON:
            if (event == GRUNTY_TALK_DONE) {
                grunty_lock_player(FALSE);
                quiz_action = QUIZ_IDLE;
                reward_jingle = TRUE;
            }
            break;
        case QUIZ_DECLINED:
        case QUIZ_AFTER:
            if (event == GRUNTY_TALK_DONE) {
                grunty_lock_player(FALSE);
                quiz_action = QUIZ_IDLE;
            }
            break;
        default:
            break;
    }

    // Her Jiggy, on the ledge once the quiz is won, until it's collected.
    if ((quiz_saved & QUIZ_SAVE_WON) && quiz_action != QUIZ_WON) {
        if (reward_jingle && !jiggy_reward_collected(GRUNTY_REWARD_QUIZ)) {
            jiggy_bk_jingle_play(GRUNTY_JINGLE_JIGGY_SPAWN, 1.0f);
        }
        reward_jingle = FALSE;
        jiggy_reward_update(GRUNTY_REWARD_QUIZ, reward_spot[0], reward_spot[1] + REWARD_HEIGHT, reward_spot[2], player, frames);
    }
}

// Called after an actor is drawn (func_global_asm_80630DCC in patches_draw.c); moves and draws
// her once a frame, with the player.
Gfx *jiggy_grunty_quiz_draw(Gfx *dl, Actor *actor) {
    u32 frames;
    u32 i;

    if (actor != gPlayerPointer || cc_number_of_players != 1) {
        return dl;
    }
    // A new map, or this one loaded again (object_timer starts over).
    if ((s32)current_map != quiz_last_map || object_timer < quiz_last_timer) {
        if (quiz_spot != NULL) {
            grunty_talk_close();
        }
        quiz_last_map = current_map;
        quiz_placed = FALSE;
        quiz_spot = NULL;
        for (i = 0; i < ARRAY_COUNT_OF(quiz_spots); i++) {
            if (quiz_spots[i].map == current_map) {
                quiz_spot = &quiz_spots[i];
            }
        }
        // Read back on every map, which also catches a different save file being loaded.
        quiz_saved = quiz_spot != NULL ? jiggy_save_get(current_file, GRUNTY_SAVE_KEY(GRUNTY_SAVE_QUIZ, current_map)) : 0;
        if (quiz_spot != NULL) {
            quiz_shuffle();
        }
    }
    if (quiz_spot == NULL) {
        quiz_last_timer = object_timer;
        return dl;
    }
    if (!grunty_ready()) {
        return dl;
    }
    // Placed the first time the player stands on the map (after the entrance walk-in).
    if (!quiz_placed) {
        quiz_last_timer = object_timer;
        if (actor->y_position - actor->floor > PLAYER_AIRBORNE_HEIGHT) {
            return dl;
        }
        quiz_place(actor);
    }
    frames = object_timer - quiz_last_timer;
    quiz_last_timer = object_timer;
    // Paused, or back after a while: don't jump ahead.
    if (frames > 4) {
        frames = 1;
    }
    // She waits while the game is paused or someone else's cutscene holds the player.
    if (frames > 0 && !((D_global_asm_8076A0B1 & CUTSCENE_LOCK) && quiz_action == QUIZ_IDLE)) {
        quiz_update(actor, frames);
    }
    return grunty_draw(dl, quiz_anim, quiz_progress, quiz_pos, quiz_yaw, TRUE, quiz_anim == GRUNTY_ANIM_CAST, 1.0f);
}
