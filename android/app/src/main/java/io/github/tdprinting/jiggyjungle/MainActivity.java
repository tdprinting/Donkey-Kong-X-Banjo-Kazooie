package io.github.tdprinting.jiggyjungle;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * The launcher: pick the two ROMs (checked and stored by the same C++ the desktop launcher
 * uses), check for updates, and start the game once the Android engine is ready.
 */
public final class MainActivity extends Activity {
    private static final int PICK_DK64 = 1;
    private static final int PICK_BANJO = 2;

    private final ExecutorService worker = Executors.newSingleThreadExecutor();

    private Button dk64Button;
    private Button banjoButton;
    private Button startButton;
    private Button updateButton;
    private TextView status;
    private Updates.Available pendingUpdate;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        buildUi();
        refreshRomButtons();
        checkForUpdates();
    }

    @Override
    protected void onDestroy() {
        worker.shutdownNow();
        super.onDestroy();
    }

    private void buildUi() {
        int pad = dp(24);
        LinearLayout column = new LinearLayout(this);
        column.setOrientation(LinearLayout.VERTICAL);
        column.setGravity(Gravity.CENTER_HORIZONTAL);
        column.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(this);
        title.setText("Banjo & Kong: Jiggy Jungle");
        title.setTextSize(26);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER);
        column.addView(title);

        TextView version = new TextView(this);
        version.setText("Version " + BuildConfig.VERSION_NAME);
        version.setGravity(Gravity.CENTER);
        version.setPadding(0, 0, 0, dp(16));
        column.addView(version);

        dk64Button = addButton(column, v -> pickRom(PICK_DK64));
        banjoButton = addButton(column, v -> pickRom(PICK_BANJO));
        startButton = addButton(column, v -> status.setText(
                "Both ROMs are ready. The game engine for Android is the next milestone, so it can't start yet."));
        startButton.setText("Start Game");
        updateButton = addButton(column, v -> installUpdate());
        updateButton.setText("Checking for Updates...");
        updateButton.setEnabled(false);

        status = new TextView(this);
        status.setPadding(0, dp(16), 0, 0);
        status.setGravity(Gravity.CENTER);
        column.addView(status);

        TextView legal = new TextView(this);
        legal.setText("Jiggy Jungle needs your own NTSC-U (US) 1.0 N64 dumps of Donkey Kong 64 and "
                + "Banjo-Kazooie. No game data comes with the app.");
        legal.setTextSize(12);
        legal.setGravity(Gravity.CENTER);
        legal.setPadding(0, dp(24), 0, 0);
        column.addView(legal);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(column);
        setContentView(scroll);
    }

    private Button addButton(LinearLayout parent, View.OnClickListener listener) {
        Button button = new Button(this);
        button.setOnClickListener(listener);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.topMargin = dp(8);
        parent.addView(button, params);
        return button;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private File romFile(int game) {
        return new File(getFilesDir(), Native.storedRomName(game));
    }

    private void refreshRomButtons() {
        boolean haveDk64 = romFile(Native.GAME_DK64).isFile();
        boolean haveBanjo = romFile(Native.GAME_BANJO).isFile();
        dk64Button.setText(haveDk64 ? "Donkey Kong 64 ROM Loaded" : "Load Donkey Kong 64 ROM");
        banjoButton.setText(haveBanjo ? "Banjo-Kazooie ROM Loaded" : "Load Banjo-Kazooie ROM");
        startButton.setEnabled(haveDk64 && haveBanjo);
    }

    private void pickRom(int request) {
        // The Storage Access Framework: no storage permission needed, and works with any provider.
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, request);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        final int game = requestCode == PICK_BANJO ? Native.GAME_BANJO : Native.GAME_DK64;
        final Uri uri = data.getData();
        status.setText("Checking the ROM...");
        worker.execute(() -> {
            String message;
            try (InputStream in = getContentResolver().openInputStream(uri)) {
                if (in == null) {
                    throw new IOException("the file couldn't be opened");
                }
                byte[] bytes = Updates.readAll(in, null, -1);
                String[] result = Native.importRom(bytes, game, getFilesDir().getAbsolutePath());
                message = result[1];
            } catch (IOException | OutOfMemoryError e) {
                message = "Couldn't read that file: " + e.getMessage();
            }
            final String shown = message;
            runOnUiThread(() -> {
                status.setText(shown);
                refreshRomButtons();
            });
        });
    }

    private void checkForUpdates() {
        worker.execute(() -> {
            Updates.Available update = null;
            String failure = null;
            try {
                update = Updates.check();
            } catch (IOException e) {
                failure = e.getMessage();
            }
            final Updates.Available found = update;
            final String error = failure;
            runOnUiThread(() -> {
                pendingUpdate = found;
                if (found != null) {
                    updateButton.setText("Install Update " + found.version);
                    updateButton.setEnabled(true);
                } else if (error != null) {
                    updateButton.setText("Couldn't Check for Updates");
                } else {
                    updateButton.setText("Up to Date");
                }
            });
        });
    }

    private void installUpdate() {
        final Updates.Available update = pendingUpdate;
        if (update == null) {
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !getPackageManager().canRequestPackageInstalls()) {
            // Android asks once per app before it may install updates.
            status.setText("Allow Jiggy Jungle to install updates, then tap Install Update again.");
            startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    Uri.parse("package:" + getPackageName())));
            return;
        }
        updateButton.setEnabled(false);
        updateButton.setText("Downloading " + update.version + "...");
        worker.execute(() -> {
            String error = null;
            final int[] shownPercent = {-1};
            try {
                Updates.downloadAndInstall(getApplicationContext(), update, (done, total) -> {
                    final int percent = total > 0 ? (int) (100 * done / total) : -1;
                    if (percent >= 0 && percent != shownPercent[0]) {
                        shownPercent[0] = percent;
                        runOnUiThread(() -> updateButton.setText("Downloading " + update.version + "... " + percent + "%"));
                    }
                });
            } catch (IOException e) {
                error = e.getMessage();
            }
            final String failure = error;
            runOnUiThread(() -> {
                if (failure != null) {
                    status.setText("Couldn't download the update: " + failure);
                    updateButton.setText("Retry Update " + update.version);
                    updateButton.setEnabled(true);
                } else {
                    updateButton.setText("Installing " + update.version + "...");
                }
            });
        });
    }
}
