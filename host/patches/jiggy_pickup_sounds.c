// Banjo-Kazooie's Jiggy jingle for Golden Bananas. Golden Bananas are drawn as Jiggies
// (jiggy_collectibles.c), so collecting one plays the jingle Banjo-Kazooie plays for a Jiggy
// (COMUSIC_D_JINGLE_JIGGY_COLLECTED, from the player's Banjo-Kazooie ROM) in place of DK64's
// Golden Banana fanfare. Without a Banjo-Kazooie ROM, DK64's fanfare plays as before.
//
// DK64 picks each pickup's sound in func_global_asm_806F79E0 (code_F9450.c), by the
// collected object's type. This is that function as the decomp has it, changed only where
// marked @jiggy. Boss Keys share the Golden Banana fanfare and keep it.

#include "patches.h"
#include "common_structs.h"
#include "jiggy.h"

extern u8 D_global_asm_80745838;
extern Actor *gPlayerPointer;
extern PlayerAdditionalActorData *extra_player_info_pointer;
extern Maps current_map;
void playSong(s32 song, f32 volume);
s16 playSound(s16 arg0, s32 arg1, f32 arg2, f32 arg3, u8 arg4, u8 arg5);
u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex);
void func_global_asm_80687C48(void);
void func_global_asm_80687CC8(void);

// DK64's songs (MUSIC_E) played here.
#define MUSIC_GB_KEY_GET 18
#define MUSIC_BANANA_COIN_GET 23
#define MUSIC_MINECART_COIN_GET 32
#define MUSIC_MELON_SLICE_GET 33
#define MUSIC_CRYSTAL_COCONUT_GET 35
#define MUSIC_BLUEPRINT_GET 69
#define MUSIC_RAINBOW_COIN_GET 145

// Banjo-Kazooie's Jiggy jingle, at its own volume (bs/jig.c); DK64's Golden Banana fanfare if
// there's no Banjo-Kazooie ROM to play it from.
static void jiggy_play_golden_banana_song(void) {
    if (jiggy_bk_jingle_play(JIGGY_BK_JINGLE_JIGGY, 28000.0f / 32767.0f) <= 0) {
        playSong(MUSIC_GB_KEY_GET, 1.0f);
    }
}

RECOMP_PATCH void func_global_asm_806F79E0(s16 arg0, u8 arg1) {
    s32 var_v0;

    switch (arg0) {
        case 0x8E:
            playSong(MUSIC_CRYSTAL_COCONUT_GET, 1.0 - (D_global_asm_80745838 * 0.3));
            break;
        case 0x57:
            playSong(MUSIC_MELON_SLICE_GET, 1.0 - (D_global_asm_80745838 * 0.3));
            break;
        case 0x56:
            playSound(0x147, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
        case 0x11:
        case 0x8F:
            playSound(0x157, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
        case 0x98:
            playSound(0x263, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
        case 0xDD:
        case 0xDE:
        case 0xDF:
        case 0xE0:
        case 0xE1:
            playSong(MUSIC_BLUEPRINT_GET, 1.0f);
            break;
        case 0x90:
            jiggy_play_golden_banana_song(); // @jiggy
            func_global_asm_80687C48();
            if ((gPlayerPointer->y_position - gPlayerPointer->floor) < 100.0f) {
                if (gPlayerPointer->control_state != 0x63) {
                    if (!(gPlayerPointer->unk6A & 4) || !((gPlayerPointer->unkAC - gPlayerPointer->y_position) > 20.0f)) {
                        setAction(0x29, 0, extra_player_info_pointer->unk1A4);
                    }
                }
            }
            break;
        case 0x74:
        case 0x288:
            jiggy_play_golden_banana_song(); // @jiggy
            if ((gPlayerPointer->y_position - gPlayerPointer->floor) < 100.0f) {
                if (gPlayerPointer->control_state != 0x63) {
                    if (!(gPlayerPointer->unk6A & 4) || !((gPlayerPointer->unkAC - gPlayerPointer->y_position) > 20.0f)) {
                        setAction(0x29, 0, extra_player_info_pointer->unk1A4);
                    }
                }
            }
            break;
        case 0xA:
        case 0xD:
        case 0x16:
        case 0x1E:
        case 0x1F:
        case 0x1CF:
        case 0x1D0:
            playSound(0x2A0, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
        case 0x1C:
        case 0x1D:
        case 0x23:
        case 0x24:
        case 0x27:
            playSong(MUSIC_BANANA_COIN_GET, 1.0 - (D_global_asm_80745838 * 0.3));
        case 0x2B:
            break;
        case 0xB7:
            playSong(MUSIC_RAINBOW_COIN_GET, 1.0 - (D_global_asm_80745838 * 0.3));
            break;
        case 0xEC:
        case 0x1D2:
            playSong(MUSIC_MINECART_COIN_GET, 1.0 - (D_global_asm_80745838 * 0.3));
            break;
        case 0x13C:
            playSong(MUSIC_GB_KEY_GET, 1.0f);
            var_v0 = 0x41;
            if (current_map == MAP_HELM) {
                var_v0 = 0x29;
            }
            setAction(var_v0, 0, extra_player_info_pointer->unk1A4);
            break;
        case 0x18D:
            playSong(MUSIC_GB_KEY_GET, 1.0f);
            setAction(0x42, 0, extra_player_info_pointer->unk1A4);
            func_global_asm_80687CC8();
            break;
        case 0x1D1:
            playSound(0xAE, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
    }
}
