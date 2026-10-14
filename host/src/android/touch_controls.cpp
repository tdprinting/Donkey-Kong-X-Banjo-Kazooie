// On-screen touch controls for Android. The overlay (android/.../TouchControls.java) draws
// the buttons and reports presses here, and this feeds them to an SDL virtual game
// controller. The game sees it as an ordinary controller, so the existing input mapping,
// menus and rebinding all work with no changes to the engine.

#include <mutex>

#include <jni.h>

#include "SDL2/SDL.h"

namespace {

std::mutex touch_mutex;
SDL_Joystick* touch_joystick = nullptr;

// Attaches the virtual controller once SDL's joystick subsystem is up (the game starts SDL
// on its own thread, possibly after the overlay's first touch).
SDL_Joystick* touch_controller() {
    if (touch_joystick != nullptr) {
        return touch_joystick;
    }
    if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0) {
        return nullptr;
    }
    SDL_VirtualJoystickDesc desc{};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    desc.name = "Jiggy Jungle Touch Controls";
    int device_index = SDL_JoystickAttachVirtualEx(&desc);
    if (device_index < 0) {
        SDL_Log("Touch controls: couldn't attach the virtual controller: %s", SDL_GetError());
        return nullptr;
    }
    touch_joystick = SDL_JoystickOpen(device_index);
    if (touch_joystick == nullptr) {
        SDL_Log("Touch controls: couldn't open the virtual controller: %s", SDL_GetError());
    }
    return touch_joystick;
}

} // namespace

extern "C" {

// button: an SDL_GameControllerButton value.
JNIEXPORT void JNICALL Java_io_github_tdprinting_jiggyjungle_TouchControls_nativeSetButton(
        JNIEnv*, jclass, jint button, jboolean pressed) {
    std::lock_guard lock{touch_mutex};
    if (SDL_Joystick* joystick = touch_controller()) {
        SDL_JoystickSetVirtualButton(joystick, button, pressed ? SDL_PRESSED : SDL_RELEASED);
    }
}

// axis: an SDL_GameControllerAxis value; value: -1 to 1 (triggers 0 to 1).
JNIEXPORT void JNICALL Java_io_github_tdprinting_jiggyjungle_TouchControls_nativeSetAxis(
        JNIEnv*, jclass, jint axis, jfloat value) {
    std::lock_guard lock{touch_mutex};
    if (SDL_Joystick* joystick = touch_controller()) {
        float clamped = value < -1.0f ? -1.0f : (value > 1.0f ? 1.0f : value);
        SDL_JoystickSetVirtualAxis(joystick, axis, static_cast<Sint16>(clamped * SDL_JOYSTICK_AXIS_MAX));
    }
}

} // extern "C"
