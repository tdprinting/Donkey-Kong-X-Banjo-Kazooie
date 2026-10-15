#ifndef __JIGGY_GRUNTILDA_H__
#define __JIGGY_GRUNTILDA_H__

// Gruntilda, shared by her Furnace Fun quiz in the Hideout Helm lobby (jiggy_grunty_quiz.c)
// and her broomstick fight in Hideout Helm (jiggy_grunty_boss.c): her model, her portrait,
// her sounds and her text box (jiggy_gruntilda.c). The two are on different maps, so they
// share one copy of each.

// Include after common_structs.h (which has no include guard).

// Banjo-Kazooie's final boss Gruntilda on her broomstick (ASSET_53D_MODEL_GRUNTILDA_FINAL_BOSS),
// and her animations (chFinalBossAnimationInfo, src/fight/chfinalboss.c) with how long each plays.
#define GRUNTY_ANIM_FLY 0x1C5    // Flying (travel, turn, wait)
#define GRUNTY_ANIM_SWOOP 0x25C  // Phase 1 swoop
#define GRUNTY_ANIM_CAST 0x25A   // Throwing a spell
#define GRUNTY_ANIM_OW 0x258     // Hit (phase 2)
#define GRUNTY_ANIM_WEEEGH 0x259 // Hit hard
#define GRUNTY_ANIM_FALL 0x263   // Falling off her broom (phase 4)
#define GRUNTY_ANIM_LAND 0x25D   // Crash landing (phase 4)
#define GRUNTY_FLY_SECONDS 0.6f
#define GRUNTY_SWOOP_SECONDS 1.0f
#define GRUNTY_CAST_SECONDS 1.1f
#define GRUNTY_OW_SECONDS 1.0f
#define GRUNTY_WEEEGH_SECONDS 1.4f
#define GRUNTY_FALL_SECONDS 0.5f
#define GRUNTY_LAND_SECONDS 7.0f
// Where through the cast the spell leaves her hand (chfinalboss throws it about halfway).
#define GRUNTY_CAST_RELEASE_AT 0.55f

// Her size: the model is 515 Banjo-Kazooie units tall at rest, from her broom's underside (the
// model's origin) up; scaled down from Banjo-Kazooie's 0.25 so she fits Helm's rooms (about 80
// DK64 units tall).
#define GRUNTY_SCALE 0.16f
#define GRUNTY_HEIGHT 82.0f

// Banjo-Kazooie sounds (sfx_e) and music (comusic_e) she uses.
#define GRUNTY_SFX_LAUGH_1 0xEA        // SFX_EA_GRUNTY_LAUGH_1
#define GRUNTY_SFX_LAUGH_2 0xEB        // SFX_EB_GRUNTY_LAUGH_2
#define GRUNTY_SFX_AH 0x12A            // SFX_12A_GRUNTY_AH
#define GRUNTY_SFX_QUESTION 0x12C      // SFX_12C_FF_QUESTION_START
#define GRUNTY_SFX_CHEERING 0x124      // SFX_124_AUDIENCE_CHEERING_1 (her audience: a wrong answer)
#define GRUNTY_SFX_BOOING 0x126        // SFX_126_AUDIENCE_BOOING (a right answer)
#define GRUNTY_SFX_ECHOING_CRY 0x130   // SFX_130_GRUNTY_ECHOING_CRY
#define GRUNTY_SFX_WEEEGH 0x131        // SFX_131_GRUNTY_WEEEGH
#define GRUNTY_SFX_YOW 0x132           // SFX_132_GRUNTY_YOW
#define GRUNTY_SFX_OHW 0x133           // SFX_133_GRUNTY_OHW
#define GRUNTY_SFX_LAUGH_3 0x142       // SFX_142_GRUNTY_LAUGH_3
#define GRUNTY_SFX_SPELL_1 0x146       // SFX_146_GRUNTY_SPELL_ATTACK_1
#define GRUNTY_SFX_SPELL_2 0x147       // SFX_147_GRUNTY_SPELL_ATTACK_2
#define GRUNTY_SFX_SPELL_LANDING 0x148 // SFX_148_GRUNTY_SPELL_LANDING
#define GRUNTY_SFX_LAUGH_4 0x14B       // SFX_14B_GRUNTY_LAUGH_4
#define GRUNTY_SFX_FALLING_OFF 0x17C   // SFX_17C_GRUNTY_FALLING_OFF_1
#define GRUNTY_SFX_DAMAGE 0x179        // SFX_179_GRUNTY_DAMAGE
#define GRUNTY_SFX_HIT_ENEMY 0x1F      // SFX_1F_HITTING_AN_ENEMY_3
#define GRUNTY_SFX_BROOM_BREAK 0xD9    // SFX_D9_WOODEN_CRATE_BREAKING_1
#define GRUNTY_SFX_THUD 0x5B           // SFX_5B_HEAVY_STUFF_FALLING
#define GRUNTY_SFX_BANJO_AYE 0x38      // SFX_38_BANJO_AYE_1
#define GRUNTY_JINGLE_DING 0x2B        // COMUSIC_2B_DING_B
#define GRUNTY_JINGLE_BUZZER 0x2C      // COMUSIC_2C_BUZZER
#define GRUNTY_JINGLE_JIGGY_SPAWN 0x3D // COMUSIC_3D_JIGGY_SPAWN
#define GRUNTY_JINGLE_MENACING 0x8D    // JINGLE_MENACING_GRUNTILDA_A
#define GRUNTY_JINGLE_FALLING 0x91     // COMUSIC_91_GRUNTY_FALLING

// Banjo-Kazooie units to DK64 units, as jiggy_banjo3d.c scales Banjo.
#define GRUNTY_BK_TO_DK64 0.25f
#define GRUNTY_GAME_FPS 30.0f
// Save keys (docs/GAME_PLAN.md): Hideout Helm's prefix, the character in bits 16-23.
#define GRUNTY_SAVE_KEY(index, map) (0x58000000u | ((u32)(index) << 16) | ((u32)(map) & 0xFFFFu))
#define GRUNTY_SAVE_QUIZ 0
#define GRUNTY_SAVE_BOSS 1
// Jiggy rewards (jiggy_rewards.c), from Hideout Helm's range.
#define GRUNTY_REWARD_QUIZ 0x80
#define GRUNTY_REWARD_BOSS 0x81

// The model, her portrait and the text box are loaded the first time this is called; TRUE once
// they are (FALSE without a Banjo-Kazooie ROM, or if one didn't convert).
s32 grunty_ready(void);
// Draws her at pos (her broom's underside) turned to yaw (12-bit angle units). broom shows her
// broomstick; glow lights the spell in her hand; scale is a share of GRUNTY_SCALE (1 is full).
Gfx *grunty_draw(Gfx *dl, s32 animation, f32 progress, const f32 pos[3], f32 yaw, s32 broom, s32 glow, f32 scale);
// A Banjo-Kazooie sound from pos, fading out with the player's distance from it.
void grunty_sfx(const f32 pos[3], Actor *player, u32 sfx, f32 volume, f32 pitch);
f32 grunty_random(void);
f32 grunty_random_between(f32 low, f32 high);
f32 grunty_distance_xz(f32 ax, f32 az, f32 bx, f32 bz);
// The floor under (x, z), looked for from probe_above over y; y if there's none within max_step.
f32 grunty_floor(f32 x, f32 y, f32 z, f32 probe_above, f32 max_step);
// The cutscene lock that holds the player while she talks (as jiggy_bottles.c holds them).
// Callers hold it for a whole scene, across several talks.
void grunty_lock_player(s32 locked);

// Her text box. A talk is either pages of lines (A to go on) or a choice: a prompt with
// answers picked with the stick and A, as Banjo-Kazooie's quiz does (gcquiz). It isn't drawn on
// another map than the one it opened on.
typedef enum {
    GRUNTY_TALK_NONE,     // Still talking (or not at all).
    GRUNTY_TALK_DONE,     // The last page was dismissed.
    GRUNTY_TALK_ANSWERED, // An answer was picked (grunty_talk_answer).
} GruntyTalkEvent;
#define GRUNTY_ANSWERS_MAX 3
void grunty_talk_pages(const char *const *pages, s32 count);
void grunty_talk_choice(const char *prompt, const char *const *answers, s32 answer_count);
s32 grunty_talk_open(void);
// Call once a frame while a talk is open; returns a GruntyTalkEvent.
s32 grunty_talk_update(void);
s32 grunty_talk_answer(void);
// Ends any talk and lets the player go (on leaving the map, say).
void grunty_talk_close(void);

#endif
