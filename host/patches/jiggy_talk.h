#ifndef __JIGGY_TALK_H__
#define __JIGGY_TALK_H__

// A shared Banjo-Kazooie text box (jiggy_talk.c) for characters that talk: a portrait from the
// player's Banjo-Kazooie ROM beside DK64's dialogue font, with the character's talking sounds,
// as jiggy_bottles.c draws Bottles'. One box is open at a time; whoever opened it (an owner id
// of the caller's choosing) closes it on leaving the map.

// Include common_structs.h (it has no include guard) before this.

typedef struct {
    u32 portrait;     // Banjo-Kazooie sprite asset (ASSET_*_SPRITE_*), 32x32 or 64x64 frames.
    u8 talk_frames;   // Its first frames, which move the mouth.
    const char *name; // Shown above the text, in uppercase.
    const u16 *sfx;   // Talking sounds (sfx_e), one played every few characters.
    u8 sfx_count;
    f32 sfx_volume;   // BK's volume / 32767.0f.
    f32 sfx_pitch;
} JiggyTalker;

// How the box behaves.
#define JIGGY_TALK_LOCK 0x1 // Holds the player (the game's cutscene lock); A turns the pages.
#define JIGGY_TALK_AUTO 0x2 // Doesn't hold the player or read A: each page stays up a while.

// Opens the box with `pages` (each one press of A; uppercase; broken into lines to fit). A
// locked box replaces an auto one; otherwise a box already open is left alone and this returns
// FALSE.
s32 jiggy_talk_say(s32 owner, const JiggyTalker *who, const char *const *pages, s32 count, u32 flags);
// Opens a locked box asking `question` with up to four `options`, picked with the stick or the
// D-pad and A. jiggy_talk_answer gives the pick once the box has closed.
s32 jiggy_talk_ask(s32 owner, const JiggyTalker *who, const char *question, const char *const *options, s32 count);
// The option picked in the last question box `owner` asked, or -1.
s32 jiggy_talk_answer(s32 owner);
// Whether the box is open (for anyone, with owner -1).
s32 jiggy_talk_open(s32 owner);
// Moves the box on a frame; call each frame while it may be open. Runs once per game frame
// however many owners call it, and not while the game is paused.
void jiggy_talk_update(void);
// Closes `owner`'s box, letting the player go.
void jiggy_talk_close(s32 owner);
// The HUD pass (patches_ui.c).
Gfx *jiggy_talk_hud_draw(Gfx *dl);

// Owners.
#define JIGGY_TALK_OWNER_BRENTILDA 1
#define JIGGY_TALK_OWNER_CHEATO 2
#define JIGGY_TALK_OWNER_GRUNTY 3

// jiggy_cameo.c: whether Banjo's portrait is up (others wait for it).
s32 jiggy_cameo_showing(void);

#endif
