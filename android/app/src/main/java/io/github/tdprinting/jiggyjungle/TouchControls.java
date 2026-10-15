package io.github.tdprinting.jiggyjungle;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.SparseArray;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * An N64 controller drawn over the game: analog stick, A, B, Z, L, R, Start and the four C
 * buttons. Presses go to libmain.so, which feeds them to an SDL virtual game controller, so
 * the game reads them like any other controller (see host/src/android/touch_controls.cpp).
 * Touches that miss every control fall through to the game.
 */
final class TouchControls extends View {
    // SDL_GameControllerButton / SDL_GameControllerAxis values, laid out the way the game's
    // default controller mapping reads them (recompinput's input_mapping.cpp).
    private static final int BUTTON_A = 0;       // N64 A
    private static final int BUTTON_X = 2;       // N64 B
    private static final int BUTTON_BACK = 4;    // Game menu
    private static final int BUTTON_START = 6;
    private static final int BUTTON_LEFTSHOULDER = 9; // N64 L
    private static final int AXIS_LEFTX = 0;
    private static final int AXIS_LEFTY = 1;
    private static final int AXIS_RIGHTX = 2;    // C left / right
    private static final int AXIS_RIGHTY = 3;    // C up / down
    private static final int AXIS_TRIGGERLEFT = 4;  // N64 Z
    private static final int AXIS_TRIGGERRIGHT = 5; // N64 R

    static native void nativeSetButton(int button, boolean pressed);
    static native void nativeSetAxis(int axis, float value);

    /** A round control. x, y and radius are fractions of the view's height, from its edges. */
    private abstract static class Control {
        final String label;
        final int color;
        final float x, y, radius; // x < 0 measures from the right edge.
        float cx, cy, r;          // In pixels, set by layout().
        boolean pressed;

        Control(String label, int color, float x, float y, float radius) {
            this.label = label;
            this.color = color;
            this.x = x;
            this.y = y;
            this.radius = radius;
        }

        void layout(int width, int height) {
            cx = x >= 0 ? x * height : width + x * height;
            cy = y * height;
            r = radius * height;
        }

        boolean hits(float px, float py, float slop) {
            float dx = px - cx, dy = py - cy;
            return dx * dx + dy * dy <= (r + slop) * (r + slop);
        }

        abstract void press(boolean down);
        void move(float px, float py) {}
    }

    private static final class Button extends Control {
        final int button;
        Button(String label, int color, float x, float y, float radius, int button) {
            super(label, color, x, y, radius);
            this.button = button;
        }
        @Override void press(boolean down) {
            pressed = down;
            nativeSetButton(button, down);
        }
    }

    /** A button the game reads from an axis: the triggers (0 to 1) and C buttons (-1 or 1). */
    private static final class AxisButton extends Control {
        final int axis;
        final float value;
        AxisButton(String label, int color, float x, float y, float radius, int axis, float value) {
            super(label, color, x, y, radius);
            this.axis = axis;
            this.value = value;
        }
        @Override void press(boolean down) {
            pressed = down;
            nativeSetAxis(axis, down ? value : 0f);
        }
    }

    private static final class Stick extends Control {
        float knobX, knobY; // -1 to 1.
        Stick(float x, float y, float radius) {
            super("", 0x80FFFFFF, x, y, radius);
        }
        @Override void press(boolean down) {
            pressed = down;
            if (!down) {
                knobX = knobY = 0;
                nativeSetAxis(AXIS_LEFTX, 0);
                nativeSetAxis(AXIS_LEFTY, 0);
            }
        }
        @Override void move(float px, float py) {
            float dx = (px - cx) / r, dy = (py - cy) / r;
            float length = (float) Math.sqrt(dx * dx + dy * dy);
            if (length > 1) {
                dx /= length;
                dy /= length;
            }
            knobX = dx;
            knobY = dy;
            // SDL's Y axis points down, like the screen.
            nativeSetAxis(AXIS_LEFTX, dx);
            nativeSetAxis(AXIS_LEFTY, dy);
        }
    }

    private static final int BLUE = 0x903060E0;
    private static final int GREEN = 0x9030A040;
    private static final int YELLOW = 0x90E0C020;
    private static final int GRAY = 0x80808080;
    private static final int RED = 0x90C03030;

    private final List<Control> controls = new ArrayList<>();
    private final SparseArray<Control> held = new SparseArray<>();
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Stick stick = new Stick(0.30f, 0.70f, 0.20f);

    TouchControls(Context context) {
        super(context);
        controls.add(stick);
        controls.add(new Button("A", BLUE, -0.26f, 0.78f, 0.085f, BUTTON_A));
        controls.add(new Button("B", GREEN, -0.42f, 0.66f, 0.085f, BUTTON_X));
        controls.add(new AxisButton("C▲", YELLOW, -0.18f, 0.36f, 0.06f, AXIS_RIGHTY, -1f));
        controls.add(new AxisButton("C▼", YELLOW, -0.18f, 0.56f, 0.06f, AXIS_RIGHTY, 1f));
        controls.add(new AxisButton("C◀", YELLOW, -0.28f, 0.46f, 0.06f, AXIS_RIGHTX, -1f));
        controls.add(new AxisButton("C▶", YELLOW, -0.08f, 0.46f, 0.06f, AXIS_RIGHTX, 1f));
        controls.add(new AxisButton("Z", GRAY, 0.12f, 0.36f, 0.07f, AXIS_TRIGGERLEFT, 1f));
        controls.add(new Button("L", GRAY, 0.12f, 0.12f, 0.06f, BUTTON_LEFTSHOULDER));
        controls.add(new AxisButton("R", GRAY, -0.12f, 0.12f, 0.07f, AXIS_TRIGGERRIGHT, 1f));
        controls.add(new Button("START", RED, -0.62f, 0.88f, 0.06f, BUTTON_START));
        controls.add(new Button("MENU", GRAY, 0.62f, 0.08f, 0.05f, BUTTON_BACK));
        text.setColor(0xE0FFFFFF);
        text.setTextAlign(Paint.Align.CENTER);
        text.setFakeBoldText(true);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        for (Control control : controls) {
            control.layout(w, h);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        for (Control control : controls) {
            fill.setColor(control.color);
            fill.setAlpha(control.pressed ? 0xD0 : Math.min(0x90, fill.getAlpha()));
            if (control == stick) {
                fill.setStyle(Paint.Style.STROKE);
                fill.setStrokeWidth(control.r * 0.06f);
                canvas.drawCircle(control.cx, control.cy, control.r, fill);
                fill.setStyle(Paint.Style.FILL);
                canvas.drawCircle(control.cx + stick.knobX * control.r, control.cy + stick.knobY * control.r,
                        control.r * 0.45f, fill);
                continue;
            }
            fill.setStyle(Paint.Style.FILL);
            canvas.drawCircle(control.cx, control.cy, control.r, fill);
            text.setTextSize(control.r * (control.label.length() > 2 ? 0.45f : 0.8f));
            canvas.drawText(control.label, control.cx, control.cy - (text.ascent() + text.descent()) / 2, text);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int index = event.getActionIndex();
        int id = event.getPointerId(index);
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                Control control = controlAt(event.getX(index), event.getY(index));
                if (control == null) {
                    // Not ours: let the game have it (only possible for the first finger).
                    return event.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN;
                }
                held.put(id, control);
                control.press(true);
                control.move(event.getX(index), event.getY(index));
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_MOVE:
                for (int i = 0; i < event.getPointerCount(); i++) {
                    Control control = held.get(event.getPointerId(i));
                    if (control != null) {
                        control.move(event.getX(i), event.getY(i));
                    }
                }
                invalidate();
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                release(id);
                return true;
            case MotionEvent.ACTION_CANCEL:
                while (held.size() > 0) {
                    release(held.keyAt(0));
                }
                return true;
            default:
                return false;
        }
    }

    private void release(int pointerId) {
        Control control = held.get(pointerId);
        if (control != null) {
            held.remove(pointerId);
            // Another finger may still hold the same control.
            if (held.indexOfValue(control) < 0) {
                control.press(false);
            }
            invalidate();
        }
    }

    private Control controlAt(float x, float y) {
        float slop = getHeight() * 0.02f;
        // The stick takes any touch near it, so a slightly-off thumb still steers.
        if (stick.hits(x, y, stick.r * 0.5f)) {
            return stick;
        }
        for (Control control : controls) {
            if (control != stick && control.hits(x, y, slop)) {
                return control;
            }
        }
        return null;
    }
}
