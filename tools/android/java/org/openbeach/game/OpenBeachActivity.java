package org.openbeach.game;

import android.content.res.AssetManager;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

/** OpenBeach on Android (docs/build/android.md): SDL's activity, with SDL3 linked into the game's libmain.so. */
public class OpenBeachActivity extends SDLActivity {
    private static final String TAG = "OpenBeach";

    @Override
    protected String[] getLibraries() {
        return new String[] {"main"};
    }

    // SDL calls this on its main thread just before SDL_main. The game directory is the app's external files directory, which a
    // USB cable or adb reaches; without a converted copy (packs/base) the L0 demo plays in l0/, as launch.sh does on handhelds.
    @Override
    protected String[] getArguments() {
        File root = getExternalFilesDir(null);
        if (root == null) root = getFilesDir(); // no shared storage mounted
        File game = root;
        try {
            // SDL sets the orientation from the window's size when it creates or fullscreens it, over the manifest's.
            Os.setenv("SDL_ORIENTATIONS", "LandscapeLeft LandscapeRight", true);
            copyDemo(new File(root, "l0"));
            if (!new File(root, "packs/base/pack.json").isFile()) {
                Os.setenv("OBL_PACK", "l0", true);
                game = new File(root, "l0");
            }
        } catch (Exception e) {
            Log.e(TAG, "the L0 demo could not be laid out", e);
        }
        Log.i(TAG, "game directory " + game);
        return new String[] {"--game-dir", game.getAbsolutePath(), "--no-settings"};
    }

    // The demo's pack is in the APK's assets (assets/l0/packs/l0); it is copied once per installed version and its saves stay.
    private void copyDemo(File demo) throws Exception {
        String version = Long.toString(getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime);
        File marker = new File(demo, "packs/l0/.installed");
        if (marker.isFile() && new String(Files.readAllBytes(marker.toPath()), StandardCharsets.UTF_8).equals(version)) return;
        copyAsset(getAssets(), "l0/packs/l0", new File(demo, "packs/l0"));
        Files.write(marker.toPath(), version.getBytes(StandardCharsets.UTF_8));
    }

    private static void copyAsset(AssetManager assets, String path, File target) throws IOException {
        String[] children = assets.list(path);
        if (children != null && children.length > 0) {
            if (!target.isDirectory() && !target.mkdirs()) throw new IOException("cannot create " + target);
            for (String child : children) copyAsset(assets, path + "/" + child, new File(target, child));
            return;
        }
        try (InputStream in = assets.open(path); OutputStream out = new FileOutputStream(target)) {
            byte[] buffer = new byte[65536];
            for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
        }
    }
}
