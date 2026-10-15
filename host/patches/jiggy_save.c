// Jiggy Jungle's own save values (jiggy_save_get/set, kept by the host beside DK64's save
// data) belong to a DK64 save file, so they're forgotten when the game starts that file
// afresh: DK64 clears a file the same way for a new game and for erasing it.
// TODO: carry the values across when the game copies a file.

#include "patches.h"
#include "jiggy.h"

extern s16 D_global_asm_807ECEA0;
extern u8 D_global_asm_807ECEA8[];
extern u8 current_file;
u8 getEEPROMSaveSlot(s32 fileIndex);
void func_global_asm_8060C648(s32 arg0, u8 arg1, u8 arg2, u8 fileIndex, u32 arg4);
void func_global_asm_8060B8F8(s32 arg0);

// The size of a save file, as saveFile.c works it out.
#define FILE_SIZE (((D_global_asm_807ECEA0 + 0x6B7 & -64) + 0x27) & -8)
#define FILE_SIZE_BYTES (FILE_SIZE / 8)

// Clears a save file (saveFile.c).
RECOMP_PATCH void func_global_asm_8060C758(u8 fileIndex, s32 arg1) {
    u8 slot;
    u8 *addr;

    slot = getEEPROMSaveSlot(fileIndex);
    addr = &D_global_asm_807ECEA8[slot * FILE_SIZE_BYTES];
    current_file = fileIndex;
    bzero(addr, FILE_SIZE_BYTES);
    func_global_asm_8060C648(0xC, 0, 0, fileIndex, 1);
    func_global_asm_8060C648(0xF, 0, 0, fileIndex, fileIndex);
    jiggy_save_clear(fileIndex); // @jiggy
    if (arg1) {
        func_global_asm_8060B8F8(1);
    }
}
