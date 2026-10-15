package io.github.tdprinting.jiggyjungle;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/**
 * Unpacks the engine's files (UI assets, controller database) from the APK's engine/ folder
 * into app storage, where the engine reads them. Done once per installed build.
 */
final class EngineFiles {
    private static final String ROOT = "engine";
    private static final String MARKER = ".engine-files";

    private EngineFiles() {}

    static void unpackIfNeeded(Context context) throws IOException {
        File target = context.getFilesDir();
        File marker = new File(target, MARKER);
        String build = buildStamp(context);
        if (marker.isFile() && build.equals(new String(Files.readAllBytes(marker.toPath()), StandardCharsets.UTF_8))) {
            return;
        }
        copyTree(context.getAssets(), ROOT, target);
        try (OutputStream out = new FileOutputStream(marker)) {
            out.write(build.getBytes(StandardCharsets.UTF_8));
        }
    }

    // Changes with every install, including debug builds that keep the same version.
    private static String buildStamp(Context context) {
        try {
            long installed = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).lastUpdateTime;
            return BuildConfig.VERSION_NAME + "/" + installed;
        } catch (PackageManager.NameNotFoundException e) {
            return BuildConfig.VERSION_NAME;
        }
    }

    private static void copyTree(AssetManager assets, String path, File destination) throws IOException {
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            // A file (the asset manager lists files as having no children).
            File parent = destination.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
                throw new IOException("couldn't create " + parent);
            }
            try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(destination)) {
                byte[] buffer = new byte[64 * 1024];
                int read;
                while ((read = in.read(buffer)) > 0) {
                    out.write(buffer, 0, read);
                }
            }
            return;
        }
        for (String child : children) {
            copyTree(assets, path + "/" + child, new File(destination, child));
        }
    }
}
