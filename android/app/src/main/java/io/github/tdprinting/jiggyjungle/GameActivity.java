package io.github.tdprinting.jiggyjungle;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

/**
 * Runs the game: SDL's activity loads libmain.so (DK64 Recompiled with the Jiggy Bridge,
 * SDL statically linked in) and calls its SDL_main on SDL's own thread.
 */
public final class GameActivity extends SDLActivity {
    private static final String TAG = "JiggyJungle";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // The engine keeps its ROMs, saves and config in APP_FOLDER_PATH, and finds its
        // assets there too (MainActivity unpacks them before starting the game). It has to
        // be set before SDLActivity loads the library and starts the game thread.
        try {
            Os.setenv("APP_FOLDER_PATH", getFilesDir().getAbsolutePath(), true);
        } catch (ErrnoException e) {
            Log.e(TAG, "Couldn't set APP_FOLDER_PATH", e);
        }
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        // SDL is linked into libmain.so rather than shipped as its own libSDL2.so.
        return new String[] { "main" };
    }
}
