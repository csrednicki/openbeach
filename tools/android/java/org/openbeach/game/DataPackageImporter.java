package org.openbeach.game;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Unpacks a PC-converted OpenBeach data package (.zip) into the app's userdata directory (docs/build/android.md).
 * Accepts zips whose entries start at packs/base/, userdata/packs/base/, or any single parent folder, replaces packs/base
 * and remaster atomically on success, and keeps existing saves.
 */
public final class DataPackageImporter {
    public interface Progress {
        void onProgress(int files, long bytesRead, long totalBytes);
    }

    public static final class MissingPackException extends IOException {
        public MissingPackException() {
            super("missing packs/base/pack.json");
        }
    }

    private DataPackageImporter() {}

    /**
     * Maps a zip entry path to its relative path inside userdata/, or null when the entry is unrelated or unsafe.
     */
    public static String normalizeEntryPath(String rawName) {
        if (rawName == null || rawName.isEmpty()) return null;
        String clean = rawName.replace('\\', '/');
        String[] rawParts = clean.split("/");
        List<String> parts = new ArrayList<>();
        for (String part : rawParts) {
            if (part.isEmpty() || ".".equals(part)) continue;
            if ("..".equals(part) || "__MACOSX".equals(part)) return null;
            parts.add(part);
        }
        if (parts.isEmpty()) return null;

        for (int i = 0; i < parts.size(); ++i) {
            String lower = parts.get(i).toLowerCase(Locale.ROOT);
            if ("packs".equals(lower) && i + 2 < parts.size() && "base".equals(parts.get(i + 1).toLowerCase(Locale.ROOT))) {
                return join("packs/base", parts, i + 2);
            }
            if ("remaster".equals(lower) && i + 1 < parts.size()) {
                return join("remaster", parts, i + 1);
            }
            if ("savegames".equals(lower) && i + 1 < parts.size()) {
                return join("SaveGames", parts, i + 1);
            }
        }
        String file = parts.get(parts.size() - 1);
        String lowerFile = file.toLowerCase(Locale.ROOT);
        // Only accept top-level INI files (or directly under userdata/<file>), never an INI inside another folder.
        if (parts.size() <= 3 && "beachlife.ini".equals(lowerFile)) return "BeachLife.ini";
        if (parts.size() <= 3 && "openbeach.ini".equals(lowerFile)) return "openbeach.ini";
        return null;
    }

    private static String join(String prefix, List<String> parts, int from) {
        StringBuilder out = new StringBuilder(prefix);
        for (int i = from; i < parts.size(); ++i) {
            out.append('/').append(parts.get(i));
        }
        return out.toString();
    }

    public static int importZip(InputStream source, long totalBytes, File userdataDir, Progress progress) throws IOException {
        File parent = userdataDir.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("cannot create " + parent);
        }
        File staging = new File(parent, userdataDir.getName() + ".staging");
        deleteTree(staging);
        if (!staging.mkdirs()) throw new IOException("cannot create " + staging);

        CountingInputStream counting = new CountingInputStream(new BufferedInputStream(source, 65536));
        int extracted = 0;
        long lastReportMs = 0;
        byte[] buffer = new byte[65536];
        try {
            try (ZipInputStream zip = new ZipInputStream(counting)) {
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    if (entry.isDirectory()) {
                        zip.closeEntry();
                        continue;
                    }
                    String rel = normalizeEntryPath(entry.getName());
                    if (rel == null) {
                        zip.closeEntry();
                        continue;
                    }
                    File outFile = new File(staging, rel);
                    File outParent = outFile.getParentFile();
                    if (outParent != null && !outParent.isDirectory() && !outParent.mkdirs()) {
                        throw new IOException("cannot create " + outParent);
                    }
                    try (OutputStream out = new FileOutputStream(outFile)) {
                        int n;
                        while ((n = zip.read(buffer)) > 0) out.write(buffer, 0, n);
                    }
                    zip.closeEntry();
                    ++extracted;
                    long now = System.currentTimeMillis();
                    if (progress != null && (now - lastReportMs >= 100 || extracted == 1)) {
                        lastReportMs = now;
                        progress.onProgress(extracted, counting.bytesRead(), totalBytes);
                    }
                }
            }
            if (!new File(staging, "packs/base/pack.json").isFile()) {
                throw new MissingPackException();
            }
            installStaging(staging, userdataDir);
            if (progress != null) progress.onProgress(extracted, totalBytes > 0 ? totalBytes : counting.bytesRead(), totalBytes);
            return extracted;
        } finally {
            deleteTree(staging);
        }
    }

    private static void installStaging(File staging, File target) throws IOException {
        if (!target.isDirectory() && !target.mkdirs()) throw new IOException("cannot create " + target);

        File packsDir = new File(target, "packs");
        if (!packsDir.isDirectory() && !packsDir.mkdirs()) throw new IOException("cannot create " + packsDir);
        replaceDirectory(new File(staging, "packs/base"), new File(packsDir, "base"));

        File stagedRemaster = new File(staging, "remaster");
        if (stagedRemaster.isDirectory()) {
            replaceDirectory(stagedRemaster, new File(target, "remaster"));
        }

        // Keep existing saves on the device; only copy save slots from the zip that do not already exist here.
        File stagedSaves = new File(staging, "SaveGames");
        if (stagedSaves.isDirectory()) {
            mergeMissingFiles(stagedSaves, new File(target, "SaveGames"));
        }

        // Copy INI files if the device has none yet (or copy BeachLife.ini on a fresh install).
        File stagedBeachIni = new File(staging, "BeachLife.ini");
        File targetBeachIni = new File(target, "BeachLife.ini");
        if (stagedBeachIni.isFile() && !targetBeachIni.isFile()) {
            copyFile(stagedBeachIni, targetBeachIni);
        }
        ensureDefaultIni(targetBeachIni);

        File stagedOpenIni = new File(staging, "openbeach.ini");
        File targetOpenIni = new File(target, "openbeach.ini");
        if (stagedOpenIni.isFile() && !targetOpenIni.isFile()) {
            copyFile(stagedOpenIni, targetOpenIni);
        }
    }

    // The game ignores BeachLife.ini without [Beach Life Options] Version=1, and widescreen 1280x720 fits phones better than 4:3.
    private static void ensureDefaultIni(File ini) throws IOException {
        String text = ini.isFile() ? new String(Files.readAllBytes(ini.toPath()), StandardCharsets.UTF_8) : "";
        String lower = text.toLowerCase(Locale.ROOT);
        StringBuilder out = new StringBuilder(text);
        if (!text.isEmpty() && !text.endsWith("\n")) out.append('\n');
        boolean changed = false;
        if (!lower.contains("[beach life options]")) {
            out.append("[Beach Life Options]\nVersion=1\n");
            changed = true;
        }
        if (!lower.contains("reswidth")) {
            out.append("[Video]\nResWidth=1280\nResHeight=720\n");
            changed = true;
        }
        if (changed || !ini.isFile()) {
            Files.write(ini.toPath(), out.toString().getBytes(StandardCharsets.UTF_8));
        }
    }

    private static void replaceDirectory(File source, File destination) throws IOException {
        deleteTree(destination);
        if (!source.renameTo(destination)) {
            copyTree(source, destination);
        }
    }

    private static void mergeMissingFiles(File sourceDir, File targetDir) throws IOException {
        if (!targetDir.isDirectory() && !targetDir.mkdirs()) throw new IOException("cannot create " + targetDir);
        File[] children = sourceDir.listFiles();
        if (children == null) return;
        for (File child : children) {
            File dest = new File(targetDir, child.getName());
            if (child.isDirectory()) {
                mergeMissingFiles(child, dest);
            } else if (!dest.exists()) {
                copyFile(child, dest);
            }
        }
    }

    private static void copyTree(File source, File destination) throws IOException {
        if (source.isDirectory()) {
            if (!destination.isDirectory() && !destination.mkdirs()) throw new IOException("cannot create " + destination);
            File[] children = source.listFiles();
            if (children != null) {
                for (File child : children) copyTree(child, new File(destination, child.getName()));
            }
        } else {
            copyFile(source, destination);
        }
    }

    private static void copyFile(File source, File destination) throws IOException {
        try (InputStream in = new FileInputStream(source); OutputStream out = new FileOutputStream(destination)) {
            byte[] buffer = new byte[65536];
            for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
        }
    }

    private static void deleteTree(File file) {
        if (file == null || !file.exists()) return;
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) deleteTree(child);
        }
        file.delete();
    }

    private static final class CountingInputStream extends FilterInputStream {
        private long count;

        CountingInputStream(InputStream in) {
            super(in);
        }

        long bytesRead() {
            return count;
        }

        @Override
        public int read() throws IOException {
            int value = super.read();
            if (value >= 0) ++count;
            return value;
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            int n = super.read(b, off, len);
            if (n > 0) count += n;
            return n;
        }
    }
}
