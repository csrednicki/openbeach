package org.openbeach.game;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Entry point on Android (docs/build/android.md). Starts OpenBeachActivity immediately when imported game data is present;
 * on the first start (or via the "Manage data" app shortcut) offers importing a PC-exported openbeach-data.zip, playing the
 * bundled L0 demo, or exporting openbeach.log.
 */
public class LauncherActivity extends Activity {
    public static final String ACTION_MANAGE_DATA = "org.openbeach.game.MANAGE_DATA";
    private static final String TAG = "OpenBeach";
    private static final int REQ_IMPORT_ZIP = 101;
    private static final int REQ_EXPORT_LOG = 102;

    private TextView statusView;
    private TextView progressText;
    private ProgressBar progressBar;
    private Button playButton;
    private Button importButton;
    private Button demoButton;
    private Button exportLogButton;
    private boolean importing;

    /**
     * Locates an imported base pack, checking internal storage first (where the .zip importer extracts) and then the external
     * app directory (where a player can copy userdata/ directly over USB or adb).
     */
    public static File findGameDataDir(Context context) {
        File internal = new File(context.getFilesDir(), "userdata");
        if (hasBasePack(internal)) return internal;
        File external = context.getExternalFilesDir(null);
        if (external != null) {
            if (hasBasePack(external)) return external;
            File externalUserdata = new File(external, "userdata");
            if (hasBasePack(externalUserdata)) return externalUserdata;
        }
        return null;
    }

    public static boolean hasBasePack(File dir) {
        return dir != null && new File(dir, "packs/base/pack.json").isFile();
    }

    private File findLatestLogFile() {
        File[] candidates = new File[] {
            new File(new File(getFilesDir(), "userdata"), "openbeach.log"),
            new File(new File(getFilesDir(), "l0"), "openbeach.log"),
            getExternalFilesDir(null) != null ? new File(getExternalFilesDir(null), "openbeach.log") : null,
            getExternalFilesDir(null) != null ? new File(new File(getExternalFilesDir(null), "userdata"), "openbeach.log") : null,
            getExternalFilesDir(null) != null ? new File(new File(getExternalFilesDir(null), "l0"), "openbeach.log") : null
        };
        File newest = null;
        for (File candidate : candidates) {
            if (candidate != null && candidate.isFile()) {
                if (newest == null || candidate.lastModified() > newest.lastModified()) newest = candidate;
            }
        }
        return newest;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        boolean manage = ACTION_MANAGE_DATA.equals(getIntent() != null ? getIntent().getAction() : null);
        File existing = findGameDataDir(this);
        if (!manage && existing != null) {
            launchGame(existing, false);
            finish();
            return;
        }
        buildUi();
        refreshState(null);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!importing && statusView != null) refreshState(null);
    }

    private void buildUi() {
        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(Color.parseColor("#0F1D2C"));

        LinearLayout container = new LinearLayout(this);
        container.setOrientation(LinearLayout.VERTICAL);
        container.setGravity(Gravity.CENTER_HORIZONTAL);
        int pad = dp(24);
        container.setPadding(pad, dp(40), pad, dp(32));

        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(20), dp(22), dp(20), dp(22));
        GradientDrawable cardBg = new GradientDrawable();
        cardBg.setColor(Color.parseColor("#172A3E"));
        cardBg.setCornerRadius(dp(16));
        card.setBackground(cardBg);

        LinearLayout.LayoutParams cardParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        container.addView(card, cardParams);

        TextView title = new TextView(this);
        title.setText(R.string.app_name);
        title.setTextColor(Color.parseColor("#F5E6C4"));
        title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 28);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        card.addView(title);

        TextView subtitle = new TextView(this);
        subtitle.setText(R.string.subtitle);
        subtitle.setTextColor(Color.parseColor("#9DB7CC"));
        subtitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        subtitle.setPadding(0, dp(2), 0, dp(16));
        card.addView(subtitle);

        statusView = new TextView(this);
        statusView.setTextColor(Color.WHITE);
        statusView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        statusView.setPadding(0, 0, 0, dp(16));
        card.addView(statusView);

        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(100);
        progressBar.setVisibility(View.GONE);
        card.addView(progressBar, marginTop(dp(4)));

        progressText = new TextView(this);
        progressText.setTextColor(Color.parseColor("#BFD6E8"));
        progressText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        progressText.setPadding(0, dp(4), 0, dp(12));
        progressText.setVisibility(View.GONE);
        card.addView(progressText);

        playButton = makeButton(R.string.btn_play, "#2D8A68", Color.WHITE);
        playButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                File dir = findGameDataDir(LauncherActivity.this);
                if (dir != null) {
                    launchGame(dir, false);
                    finish();
                }
            }
        });
        card.addView(playButton, marginTop(dp(8)));

        importButton = makeButton(R.string.btn_import_zip, "#1E6FA8", Color.WHITE);
        importButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                openZipPicker();
            }
        });
        card.addView(importButton, marginTop(dp(10)));

        demoButton = makeButton(R.string.btn_demo, "#2D465E", Color.WHITE);
        demoButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                launchGame(null, true);
                finish();
            }
        });
        card.addView(demoButton, marginTop(dp(10)));

        exportLogButton = makeButton(R.string.btn_export_log, "#24394D", Color.parseColor("#D6E4F0"));
        exportLogButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                openLogExportPicker();
            }
        });
        card.addView(exportLogButton, marginTop(dp(10)));

        TextView hint = new TextView(this);
        hint.setText(R.string.hint_pc_export);
        hint.setTextColor(Color.parseColor("#8FA9BE"));
        hint.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        hint.setPadding(0, dp(18), 0, 0);
        card.addView(hint);

        scroll.addView(container);
        setContentView(scroll);
    }

    private Button makeButton(int labelRes, String bgHex, int textColor) {
        Button button = new Button(this);
        button.setText(labelRes);
        button.setAllCaps(false);
        button.setTextColor(textColor);
        button.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        button.setPadding(dp(16), dp(14), dp(16), dp(14));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(Color.parseColor(bgHex));
        bg.setCornerRadius(dp(10));
        button.setBackground(bg);
        return button;
    }

    private LinearLayout.LayoutParams marginTop(int topPx) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.topMargin = topPx;
        return lp;
    }

    private int dp(int value) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics()));
    }

    private void refreshState(String customMessage) {
        boolean ready = findGameDataDir(this) != null;
        if (customMessage != null) {
            statusView.setText(customMessage);
        } else {
            statusView.setText(ready ? R.string.status_ready : R.string.status_missing);
        }
        playButton.setVisibility(ready ? View.VISIBLE : View.GONE);
        playButton.setEnabled(!importing && ready);
        importButton.setEnabled(!importing);
        demoButton.setEnabled(!importing);
        boolean hasLog = findLatestLogFile() != null;
        exportLogButton.setVisibility(hasLog ? View.VISIBLE : View.GONE);
        exportLogButton.setEnabled(!importing && hasLog);
    }

    private void openZipPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/zip", "application/x-zip-compressed", "application/octet-stream"});
        startActivityForResult(intent, REQ_IMPORT_ZIP);
    }

    private void openLogExportPicker() {
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TITLE, "openbeach.log");
        startActivityForResult(intent, REQ_EXPORT_LOG);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        if (requestCode == REQ_IMPORT_ZIP) {
            startZipImport(uri);
        } else if (requestCode == REQ_EXPORT_LOG) {
            exportLogTo(uri);
        }
    }

    private long querySize(Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri, new String[] {OpenableColumns.SIZE}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) return cursor.getLong(0);
        } catch (Exception ignored) {
        }
        return -1;
    }

    private void startZipImport(final Uri uri) {
        importing = true;
        final long totalBytes = querySize(uri);
        progressBar.setVisibility(View.VISIBLE);
        progressBar.setIndeterminate(totalBytes <= 0);
        progressBar.setProgress(0);
        progressText.setVisibility(View.VISIBLE);
        progressText.setText(getString(R.string.import_running, 0));
        refreshState(null);

        final File target = new File(getFilesDir(), "userdata");
        new Thread(new Runnable() {
            @Override
            public void run() {
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    if (in == null) throw new IllegalStateException("cannot open selected file");
                    final int count = DataPackageImporter.importZip(in, totalBytes, target, new DataPackageImporter.Progress() {
                        @Override
                        public void onProgress(final int files, final long bytesRead, final long total) {
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    if (total > 0) {
                                        int pct = (int) Math.min(100, (bytesRead * 100L) / total);
                                        progressBar.setIndeterminate(false);
                                        progressBar.setProgress(pct);
                                        progressText.setText(getString(R.string.import_running_pct, pct, files));
                                    } else {
                                        progressText.setText(getString(R.string.import_running, files));
                                    }
                                }
                            });
                        }
                    });
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            importing = false;
                            progressBar.setVisibility(View.GONE);
                            progressText.setVisibility(View.GONE);
                            refreshState(getString(R.string.import_done, count));
                        }
                    });
                } catch (final DataPackageImporter.MissingPackException e) {
                    Log.w(TAG, "zip import missing pack.json", e);
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            importing = false;
                            progressBar.setVisibility(View.GONE);
                            progressText.setVisibility(View.GONE);
                            refreshState(getString(R.string.import_error_missing_pack));
                        }
                    });
                } catch (final Exception e) {
                    Log.e(TAG, "zip import failed", e);
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            importing = false;
                            progressBar.setVisibility(View.GONE);
                            progressText.setVisibility(View.GONE);
                            String msg = e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName();
                            refreshState(getString(R.string.import_error_generic, msg));
                        }
                    });
                }
            }
        }, "openbeach-import").start();
    }

    private void exportLogTo(Uri uri) {
        File logFile = findLatestLogFile();
        if (logFile == null) return;
        try (InputStream in = new FileInputStream(logFile); OutputStream out = getContentResolver().openOutputStream(uri)) {
            if (out == null) throw new IllegalStateException("cannot open destination");
            byte[] buffer = new byte[16384];
            for (int n; (n = in.read(buffer)) > 0;) out.write(buffer, 0, n);
            refreshState(getString(R.string.log_exported));
        } catch (Exception e) {
            Log.e(TAG, "log export failed", e);
            String msg = e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName();
            refreshState(getString(R.string.log_export_failed, msg));
        }
    }

    private void launchGame(File gameDir, boolean demo) {
        Intent intent = new Intent(this, OpenBeachActivity.class);
        if (gameDir != null) intent.putExtra(OpenBeachActivity.EXTRA_GAME_DIR, gameDir.getAbsolutePath());
        intent.putExtra(OpenBeachActivity.EXTRA_DEMO, demo);
        startActivity(intent);
    }
}
