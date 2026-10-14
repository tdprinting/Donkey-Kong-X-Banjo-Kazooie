package io.github.tdprinting.jiggyjungle;

import android.hardware.input.InputManager;
import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;
import android.view.InputDevice;
import android.view.View;
import android.view.ViewGroup;

import org.libsdl.app.SDLActivity;

/**
 * Runs the game: SDL's activity loads libmain.so (DK64 Recompiled with the Jiggy Bridge,
 * SDL statically linked in) and calls its SDL_main on SDL's own thread. On-screen touch
 * controls sit on top, hidden while a physical controller is connected.
 */
public final class GameActivity extends SDLActivity implements InputManager.InputDeviceListener {
    private static final String TAG = "JiggyJungle";

    private TouchControls touchControls;
    private InputManager inputManager;

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
        if (mBrokenLibraries || mLayout == null) {
            return; // SDL has already shown its error dialog.
        }
        touchControls = new TouchControls(this);
        mLayout.addView(touchControls, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        inputManager = (InputManager) getSystemService(INPUT_SERVICE);
        inputManager.registerInputDeviceListener(this, null);
        updateTouchControls();
    }

    @Override
    protected void onDestroy() {
        if (inputManager != null) {
            inputManager.unregisterInputDeviceListener(this);
        }
        super.onDestroy();
    }

    @Override
    protected String[] getLibraries() {
        // SDL is linked into libmain.so rather than shipped as its own libSDL2.so.
        return new String[] { "main" };
    }

    @Override public void onInputDeviceAdded(int deviceId) { updateTouchControls(); }
    @Override public void onInputDeviceRemoved(int deviceId) { updateTouchControls(); }
    @Override public void onInputDeviceChanged(int deviceId) { updateTouchControls(); }

    private void updateTouchControls() {
        if (touchControls != null) {
            touchControls.setVisibility(hasGamepad() ? View.GONE : View.VISIBLE);
        }
    }

    private static boolean hasGamepad() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device != null && !device.isVirtual()
                    && (device.getSources() & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
                return true;
            }
        }
        return false;
    }
}
