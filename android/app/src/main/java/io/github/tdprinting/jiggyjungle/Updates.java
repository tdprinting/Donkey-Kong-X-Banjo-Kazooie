package io.github.tdprinting.jiggyjungle;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.os.Build;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Checks this repository's GitHub Releases and installs a newer APK through Android's own
 * installer, which asks the player to confirm. Blocking: call from a background thread.
 */
final class Updates {
    static final class Available {
        final String version;
        final String apkUrl;
        final long apkSize;
        final String pageUrl;

        Available(String[] fields) {
            version = fields[0];
            apkUrl = fields[1];
            apkSize = Long.parseLong(fields[2]);
            pageUrl = fields[3];
        }
    }

    interface Progress {
        void onProgress(long done, long total);
    }

    private Updates() {}

    /** Null when up to date (or nothing is published yet). Throws when the check itself fails. */
    static Available check() throws IOException {
        HttpURLConnection conn = open(Native.latestReleaseUrl(), "application/vnd.github+json");
        try {
            int status = conn.getResponseCode();
            if (status == 404) {
                return null; // No releases yet.
            }
            if (status != 200) {
                throw new IOException("GitHub answered HTTP " + status);
            }
            String json = new String(readAll(conn.getInputStream(), null, -1), "UTF-8");
            String[] fields = Native.findUpdate(json, BuildConfig.VERSION_NAME);
            return fields == null ? null : new Available(fields);
        } finally {
            conn.disconnect();
        }
    }

    /** Downloads the APK and starts an install session; Android then asks the player to confirm. */
    static void downloadAndInstall(Context context, Available update, Progress progress) throws IOException {
        PackageInstaller installer = context.getPackageManager().getPackageInstaller();
        PackageInstaller.SessionParams params =
                new PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL);
        if (update.apkSize > 0) {
            params.setSize(update.apkSize);
        }
        int sessionId = installer.createSession(params);
        PackageInstaller.Session session = installer.openSession(sessionId);
        boolean committed = false;
        try {
            HttpURLConnection conn = open(update.apkUrl, "application/octet-stream");
            try {
                int status = conn.getResponseCode();
                if (status != 200) {
                    throw new IOException("download answered HTTP " + status);
                }
                long written = 0;
                try (InputStream in = conn.getInputStream();
                     OutputStream out = session.openWrite("JiggyJungle.apk", 0, update.apkSize > 0 ? update.apkSize : -1)) {
                    byte[] buffer = new byte[1 << 16];
                    int n;
                    while ((n = in.read(buffer)) > 0) {
                        out.write(buffer, 0, n);
                        written += n;
                        if (progress != null) {
                            progress.onProgress(written, update.apkSize);
                        }
                    }
                    session.fsync(out);
                }
                if (update.apkSize > 0 && written != update.apkSize) {
                    throw new IOException("the download was incomplete");
                }
            } finally {
                conn.disconnect();
            }

            Intent intent = new Intent(context, InstallResultReceiver.class);
            int flags = PendingIntent.FLAG_UPDATE_CURRENT;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                // The installer fills in the status extras, so the intent must be mutable.
                flags |= PendingIntent.FLAG_MUTABLE;
            }
            PendingIntent pending = PendingIntent.getBroadcast(context, sessionId, intent, flags);
            session.commit(pending.getIntentSender());
            committed = true;
        } finally {
            if (!committed) {
                session.abandon();
            }
            session.close();
        }
    }

    private static HttpURLConnection open(String url, String accept) throws IOException {
        HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
        conn.setRequestProperty("Accept", accept);
        // GitHub's API requires a User-Agent.
        conn.setRequestProperty("User-Agent", "JiggyJungle/" + BuildConfig.VERSION_NAME);
        conn.setConnectTimeout(10_000);
        conn.setReadTimeout(30_000);
        conn.setInstanceFollowRedirects(true);
        return conn;
    }

    static byte[] readAll(InputStream in, Progress progress, long total) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream(total > 0 ? (int) Math.min(total, Integer.MAX_VALUE) : 1 << 16);
        byte[] buffer = new byte[1 << 16];
        long done = 0;
        int n;
        while ((n = in.read(buffer)) > 0) {
            out.write(buffer, 0, n);
            done += n;
            if (progress != null) {
                progress.onProgress(done, total);
            }
        }
        return out.toByteArray();
    }
}
