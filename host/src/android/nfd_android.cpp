// Android has no desktop file dialogs, so nativefiledialog-extended can't build there.
// This stands in for it: every dialog reports "cancelled". ROMs are picked with Android's
// own document picker in the launcher app (android/) and handed to the game as files.

#include "nfd.h"

extern "C" {

static const char* no_dialogs = "File dialogs are not available on Android.";

nfdresult_t NFD_Init(void) { return NFD_OKAY; }
void NFD_Quit(void) {}

const char* NFD_GetError(void) { return no_dialogs; }
void NFD_ClearError(void) {}

void NFD_FreePathN(nfdnchar_t*) {}
void NFD_FreePathU8(nfdu8char_t*) {}

nfdresult_t NFD_OpenDialogN(nfdnchar_t** outPath, const nfdnfilteritem_t*, nfdfiltersize_t, const nfdnchar_t*) {
    *outPath = nullptr;
    return NFD_CANCEL;
}

nfdresult_t NFD_OpenDialogU8(nfdu8char_t** outPath, const nfdu8filteritem_t*, nfdfiltersize_t, const nfdu8char_t*) {
    *outPath = nullptr;
    return NFD_CANCEL;
}

nfdresult_t NFD_OpenDialogMultipleN(const nfdpathset_t** outPaths, const nfdnfilteritem_t*, nfdfiltersize_t,
                                    const nfdnchar_t*) {
    *outPaths = nullptr;
    return NFD_CANCEL;
}

nfdresult_t NFD_SaveDialogN(nfdnchar_t** outPath, const nfdnfilteritem_t*, nfdfiltersize_t, const nfdnchar_t*,
                            const nfdnchar_t*) {
    *outPath = nullptr;
    return NFD_CANCEL;
}

nfdresult_t NFD_PickFolderN(nfdnchar_t** outPath, const nfdnchar_t*) {
    *outPath = nullptr;
    return NFD_CANCEL;
}

nfdresult_t NFD_PathSet_GetCount(const nfdpathset_t*, nfdpathsetsize_t* count) {
    *count = 0;
    return NFD_OKAY;
}

nfdresult_t NFD_PathSet_GetPathN(const nfdpathset_t*, nfdpathsetsize_t, nfdnchar_t** outPath) {
    *outPath = nullptr;
    return NFD_ERROR;
}

void NFD_PathSet_FreePathN(const nfdnchar_t*) {}
void NFD_PathSet_Free(const nfdpathset_t*) {}

} // extern "C"
