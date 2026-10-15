#ifndef __JIGGY_CAVES_H__
#define __JIGGY_CAVES_H__

// Crystal Caves fused with Freezeezy Peak (docs/GAME_PLAN.md): what its Banjo-Kazooie
// characters (jiggy_boggy.c, jiggy_wozza.c, jiggy_sir_slush.c) share.

// Included after common_structs.h and jiggy.h (common_structs.h has no include guard).

// Save keys (jiggy_save_get/set): Crystal Caves' prefix 0x55, the character in bits 16-23 and
// the map in the low bits.
#define CAVES_SAVE_KEY(character, map) (0x55000000u | ((u32)(character) << 16) | (u32)(map))
#define CAVES_CHARACTER_BOGGY 0
#define CAVES_CHARACTER_WOZZA 1
#define CAVES_CHARACTER_SIR_SLUSH 2

// Golden Banana rewards (jiggy_rewards.c), from Crystal Caves' range 0x60-0x6F.
#define CAVES_REWARD_BOGGY 0x60     // The presents delivered to Boggy's kids.
#define CAVES_REWARD_WOZZA 0x61     // Wozza found three times.
#define CAVES_REWARD_SIR_SLUSH 0x62 // All three Sir Slushes beaten.
#define CAVES_REWARD_JINJOS 0x63    // The five Jinjos (jiggy_jinjo_jiggy.c).

// D_global_asm_8076A0B1's cutscene lock (as jiggy_bottles.c uses it).
#define CAVES_CUTSCENE_LOCK 0x10

// Who's talking in the shared text box (jiggy_caves_talk.c): the name over the text, the
// Banjo-Kazooie portrait sprite and how many of its frames move the mouth (gczoombox's
// count), and the talking sounds gczoombox plays for them (one of `sfx`, at `volume` and
// about `pitch`).
typedef struct {
    const char *name;
    u32 portrait;
    s32 talk_frames;
    const u16 *sfx;
    u32 sfx_count;
    f32 volume;
    f32 pitch;
} CavesSpeaker;

// Opens the text box on `pages` (one press of A each), holding the player in the cutscene
// lock. With `hold`, the lock stays on after the last page until jiggy_caves_lock_player(FALSE)
// (for a cutscene after the talk). Returns FALSE (nothing opened) if the box is already open or
// the portrait doesn't decode.
s32 jiggy_caves_talk_start(const CavesSpeaker *speaker, const char *const *pages, s32 count, s32 hold);
// A frame of the open text box: call once a frame from the character that opened it. Returns
// TRUE on the frame its last page is dismissed.
s32 jiggy_caves_talk_update(void);
s32 jiggy_caves_talk_is_open(void);
// Shuts the box without a word more (the map changed) and lets the player go.
void jiggy_caves_talk_cancel(void);
// Holds the player in (or lets them out of) the game's cutscene lock, as jiggy_bottles.c does.
void jiggy_caves_lock_player(s32 locked);

#endif
