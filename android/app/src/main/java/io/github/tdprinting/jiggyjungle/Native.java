package io.github.tdprinting.jiggyjungle;

/** The C++ shared with the desktop launcher (see app/src/main/cpp/jiggy_jni.cpp). */
final class Native {
    static final int GAME_DK64 = 0;
    static final int GAME_BANJO = 1;

    static {
        System.loadLibrary("jiggyjungle");
    }

    private Native() {}

    /** Checks a picked ROM and stores it in {@code dir}. Returns { "1" or "0", message }. */
    static native String[] importRom(byte[] rom, int game, String dir);

    static native String storedRomName(int game);

    /** { version, apk url, apk size, release page } when there's a newer release; otherwise null. */
    static native String[] findUpdate(String latestReleaseJson, String currentVersion);

    static native String latestReleaseUrl();
}
