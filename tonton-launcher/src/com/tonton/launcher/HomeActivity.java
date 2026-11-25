package com.tonton.launcher;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.provider.MediaStore;
import android.provider.Settings;
import android.text.Editable;
import android.text.InputType;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.text.Collator;
import java.text.Normalizer;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * Tonton Launcher : écran d'accueil personnalisé dans le style de TontonOS.
 * Écrit sans bibliothèque externe, uniquement avec l'API Android de base.
 */
public class HomeActivity extends Activity {

    /* ---------- Thème ---------- */
    static final int[][] WALLPAPERS = {
        { 0xFF0E2A6E, 0xFF6E32A0 },   /* Nuit violette (TontonOS) */
        { 0xFF0A5A64, 0xFF14A078 },   /* Lagon */
        { 0xFF963C28, 0xFFF0AA3C },   /* Coucher de soleil */
        { 0xFF191C24, 0xFF3C4252 },   /* Ardoise */
    };
    static final String[] WALLPAPER_NAMES = { "Nuit violette", "Lagon", "Coucher de soleil", "Ardoise", "Fond d'écran du téléphone" };
    static final int[] ACCENTS = { 0xFFFF8C32, 0xFF2563EB, 0xFFDC2626, 0xFF16A34A, 0xFF9333EA, 0xFFEC4899 };
    static final String[] ACCENT_NAMES = { "Orange Tonton", "Bleu", "Rouge", "Vert", "Violet", "Rose" };
    static final int MAX_FAVS = 8;

    SharedPreferences prefs;
    int accent;
    int columns;

    /* ---------- Vues ---------- */
    FrameLayout root;
    LinearLayout home, drawer, favGrid, dock;
    TextView clock, date, greeting, favHint;
    EditText search;
    GridView grid;
    AppAdapter adapter;
    boolean drawerOpen;
    int insetTop, insetBottom;

    /* ---------- Données ---------- */
    final List<AppInfo> allApps = new ArrayList<>();
    final List<AppInfo> shown = new ArrayList<>();
    final Handler ui = new Handler();

    static class AppInfo {
        String label, pkg, cls, key;
        Drawable icon;
        ComponentName component() { return new ComponentName(pkg, cls); }
    }

    /* ================================================================ */

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = getSharedPreferences("tonton", MODE_PRIVATE);
        readPrefs();

        Window w = getWindow();
        w.setStatusBarColor(Color.TRANSPARENT);
        w.setNavigationBarColor(Color.TRANSPARENT);
        w.getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);

        root = new FrameLayout(this);
        root.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets in) {
                insetTop = in.getSystemWindowInsetTop();
                insetBottom = in.getSystemWindowInsetBottom();
                home.setPadding(dp(20), insetTop + dp(24), dp(20), insetBottom + dp(16));
                drawer.setPadding(dp(12), insetTop + dp(12), dp(12), 0);
                grid.setPadding(0, dp(8), 0, insetBottom + dp(16));
                return in.consumeSystemWindowInsets();
            }
        });
        buildHome();
        buildDrawer();
        setContentView(root);
        applyTheme();

        IntentFilter pf = new IntentFilter();
        pf.addAction(Intent.ACTION_PACKAGE_ADDED);
        pf.addAction(Intent.ACTION_PACKAGE_REMOVED);
        pf.addAction(Intent.ACTION_PACKAGE_CHANGED);
        pf.addDataScheme("package");
        registerReceiver(packageReceiver, pf);
        IntentFilter tf = new IntentFilter();
        tf.addAction(Intent.ACTION_TIME_TICK);
        tf.addAction(Intent.ACTION_TIME_CHANGED);
        tf.addAction(Intent.ACTION_TIMEZONE_CHANGED);
        registerReceiver(timeReceiver, tf);

        loadApps();
    }

    @Override
    protected void onDestroy() {
        unregisterReceiver(packageReceiver);
        unregisterReceiver(timeReceiver);
        super.onDestroy();
    }

    @Override
    protected void onResume() {
        super.onResume();
        updateClock();
    }

    /* Bouton Accueil pressé alors que le launcher est déjà affiché */
    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        if (drawerOpen) closeDrawer();
    }

    @Override
    public void onBackPressed() {
        if (drawerOpen) closeDrawer();
        /* sur l'accueil, Retour ne fait rien : on ne quitte pas l'écran d'accueil */
    }

    void readPrefs() {
        accent = ACCENTS[Math.min(prefs.getInt("accent", 0), ACCENTS.length - 1)];
        columns = prefs.getInt("columns", 4);
    }

    /* ================================================================
     *  Accueil
     * ================================================================ */

    void buildHome() {
        home = new LinearLayout(this);
        home.setOrientation(LinearLayout.VERTICAL);
        root.addView(home, new FrameLayout.LayoutParams(-1, -1));

        greeting = text("", 16, 0xDDFFFFFF, false);
        home.addView(greeting);

        clock = text("", 76, Color.WHITE, false);
        clock.setTypeface(Typeface.create("sans-serif-light", Typeface.NORMAL));
        clock.setIncludeFontPadding(false);
        home.addView(clock);

        date = text("", 18, 0xEEFFFFFF, false);
        home.addView(date);

        /* Favoris */
        TextView favTitle = text("Favoris", 13, 0xAAFFFFFF, true);
        LinearLayout.LayoutParams ftp = new LinearLayout.LayoutParams(-2, -2);
        ftp.topMargin = dp(36);
        home.addView(favTitle, ftp);

        favGrid = new LinearLayout(this);
        favGrid.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams fgp = new LinearLayout.LayoutParams(-1, -2);
        fgp.topMargin = dp(8);
        home.addView(favGrid, fgp);

        favHint = text("Glissez vers le haut pour voir vos applis,\npuis faites un appui long sur une appli\npour l'ajouter ici.", 14, 0xBBFFFFFF, false);
        favHint.setPadding(dp(16), dp(14), dp(16), dp(14));
        favHint.setBackground(rounded(0x26FFFFFF, dp(18), 0));
        favGrid.addView(favHint);

        View spacer = new View(this);
        home.addView(spacer, new LinearLayout.LayoutParams(-1, 0, 1f));

        /* Barre de recherche */
        TextView searchPill = text("   Rechercher une appli", 16, 0xCCFFFFFF, false);
        searchPill.setGravity(Gravity.CENTER_VERTICAL);
        searchPill.setPadding(dp(18), 0, dp(18), 0);
        searchPill.setTag("pill");
        searchPill.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { openDrawer(true); }
        });
        home.addView(searchPill, new LinearLayout.LayoutParams(-1, dp(50)));

        TextView hint = text("▴  Glissez vers le haut", 12, 0x99FFFFFF, false);
        hint.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams hp = new LinearLayout.LayoutParams(-1, -2);
        hp.topMargin = dp(10);
        home.addView(hint, hp);

        /* Dock */
        dock = new LinearLayout(this);
        dock.setOrientation(LinearLayout.HORIZONTAL);
        dock.setGravity(Gravity.CENTER);
        dock.setPadding(dp(8), dp(10), dp(8), dp(10));
        dock.setBackground(rounded(0x33FFFFFF, dp(28), 0));
        LinearLayout.LayoutParams dp_ = new LinearLayout.LayoutParams(-1, -2);
        dp_.topMargin = dp(10);
        home.addView(dock, dp_);

        /* Gestes : glisser vers le haut = applis, vers le bas = notifications */
        final GestureDetector gd = new GestureDetector(this, new GestureDetector.SimpleOnGestureListener() {
            @Override public boolean onDown(MotionEvent e) { return true; }
            @Override public boolean onFling(MotionEvent a, MotionEvent b, float vx, float vy) {
                if (a == null || b == null) return false;
                float dy = b.getY() - a.getY();
                if (Math.abs(dy) < dp(60) || Math.abs(vy) < Math.abs(vx)) return false;
                if (dy < 0) openDrawer(false); else expandNotifications();
                return true;
            }
            @Override public void onLongPress(MotionEvent e) { showSettings(); }
            @Override public boolean onDoubleTap(MotionEvent e) {
                if (!GestureService.lockScreen()) askGestureService("verrouiller l'écran par un double appui");
                return true;
            }
        });
        home.setOnTouchListener(new View.OnTouchListener() {
            @Override public boolean onTouch(View v, MotionEvent e) { return gd.onTouchEvent(e); }
        });
    }

    void updateClock() {
        if (clock == null) return;
        Date now = new Date();
        clock.setText(new SimpleDateFormat("HH:mm", Locale.FRENCH).format(now));
        String d = new SimpleDateFormat("EEEE d MMMM", Locale.FRENCH).format(now);
        date.setText(d.substring(0, 1).toUpperCase(Locale.FRENCH) + d.substring(1));
        int h = Calendar.getInstance().get(Calendar.HOUR_OF_DAY);
        String name = prefs.getString("name", "Tonton");
        String hello = h < 5 ? "Bonne nuit" : h < 18 ? "Bonjour" : "Bonsoir";
        greeting.setText(TextUtils.isEmpty(name) ? hello : hello + ", " + name);
    }

    /* Applis favorites, rangées par 4 sur l'accueil */
    void refreshFavorites() {
        favGrid.removeAllViews();
        List<AppInfo> favs = new ArrayList<>();
        for (String key : favKeys()) {
            AppInfo a = findApp(key);
            if (a != null) favs.add(a);
        }
        if (favs.isEmpty()) { favGrid.addView(favHint); return; }
        LinearLayout row = null;
        for (int i = 0; i < favs.size(); i++) {
            if (i % 4 == 0) {
                row = new LinearLayout(this);
                row.setOrientation(LinearLayout.HORIZONTAL);
                favGrid.addView(row, new LinearLayout.LayoutParams(-1, -2));
            }
            row.addView(appCell(favs.get(i), true), new LinearLayout.LayoutParams(0, -2, 1f));
        }
        for (int i = favs.size() % 4; i != 0 && i < 4; i++)
            row.addView(new View(this), new LinearLayout.LayoutParams(0, 1, 1f));
    }

    /* Dock : téléphone, messages, appareil photo, navigateur */
    void refreshDock() {
        dock.removeAllViews();
        Intent[] wanted = {
            new Intent(Intent.ACTION_DIAL),
            Intent.makeMainSelectorActivity(Intent.ACTION_MAIN, Intent.CATEGORY_APP_MESSAGING),
            new Intent(MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA),
            Intent.makeMainSelectorActivity(Intent.ACTION_MAIN, Intent.CATEGORY_APP_BROWSER),
        };
        PackageManager pm = getPackageManager();
        for (Intent it : wanted) {
            ResolveInfo ri = pm.resolveActivity(it, 0);
            AppInfo a = null;
            if (ri != null && ri.activityInfo != null) a = findAppByPackage(ri.activityInfo.packageName);
            if (a == null) continue;
            View cell = appCell(a, false);
            dock.addView(cell, new LinearLayout.LayoutParams(0, -2, 1f));
        }
    }

    /* ================================================================
     *  Tiroir d'applis
     * ================================================================ */

    void buildDrawer() {
        drawer = new LinearLayout(this);
        drawer.setOrientation(LinearLayout.VERTICAL);
        drawer.setVisibility(View.GONE);
        root.addView(drawer, new FrameLayout.LayoutParams(-1, -1));

        /* poignée : touchez ou glissez vers le bas pour fermer */
        TextView handle = text("▾", 22, 0xAAFFFFFF, false);
        handle.setGravity(Gravity.CENTER);
        handle.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { closeDrawer(); }
        });
        drawer.addView(handle, new LinearLayout.LayoutParams(-1, dp(32)));

        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        drawer.addView(bar, new LinearLayout.LayoutParams(-1, dp(50)));

        search = new EditText(this);
        search.setHint("Rechercher une appli");
        search.setHintTextColor(0x99FFFFFF);
        search.setTextColor(Color.WHITE);
        search.setSingleLine(true);
        search.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        search.setPadding(dp(18), 0, dp(18), 0);
        search.setImeOptions(EditorInfo.IME_ACTION_GO);
        search.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        search.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            public void onTextChanged(CharSequence s, int a, int b, int c) {}
            public void afterTextChanged(Editable s) { filter(s.toString()); }
        });
        /* Entrée : ouvre la première appli trouvée */
        search.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override public boolean onEditorAction(TextView v, int id, KeyEvent e) {
                if (!shown.isEmpty()) launch(shown.get(0));
                return true;
            }
        });
        bar.addView(search, new LinearLayout.LayoutParams(0, -1, 1f));

        TextView settings = text("⚙", 24, Color.WHITE, false);
        settings.setGravity(Gravity.CENTER);
        settings.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) { showSettings(); }
        });
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(dp(50), -1);
        sp.leftMargin = dp(8);
        bar.addView(settings, sp);
        settings.setTag("gear");

        grid = new GridView(this);
        grid.setNumColumns(columns);
        grid.setVerticalSpacing(dp(14));
        grid.setClipToPadding(false);
        grid.setSelector(new GradientDrawable());
        grid.setVerticalScrollBarEnabled(false);
        adapter = new AppAdapter();
        grid.setAdapter(adapter);
        grid.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override public void onItemClick(AdapterView<?> p, View v, int pos, long id) { launch(shown.get(pos)); }
        });
        grid.setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override public boolean onItemLongClick(AdapterView<?> p, View v, int pos, long id) {
                appMenu(v, shown.get(pos)); return true;
            }
        });
        /* glisser vers le bas quand la liste est tout en haut : fermer */
        final float[] startY = { 0 };
        grid.setOnTouchListener(new View.OnTouchListener() {
            @Override public boolean onTouch(View v, MotionEvent e) {
                if (e.getAction() == MotionEvent.ACTION_DOWN) startY[0] = e.getY();
                if (e.getAction() == MotionEvent.ACTION_UP) {
                    boolean atTop = grid.getChildCount() == 0
                            || (grid.getFirstVisiblePosition() == 0 && grid.getChildAt(0).getTop() >= grid.getPaddingTop() - dp(2));
                    if (atTop && e.getY() - startY[0] > dp(120)) { closeDrawer(); return true; }
                }
                return false;
            }
        });
        LinearLayout.LayoutParams gp = new LinearLayout.LayoutParams(-1, 0, 1f);
        gp.topMargin = dp(8);
        drawer.addView(grid, gp);
    }

    void openDrawer(final boolean focusSearch) {
        if (drawerOpen) return;
        drawerOpen = true;
        search.setText("");
        drawer.setVisibility(View.VISIBLE);
        drawer.setAlpha(0f);
        drawer.setTranslationY(dp(80));
        drawer.animate().alpha(1f).translationY(0).setDuration(220).start();
        home.animate().alpha(0f).setDuration(180).start();
        grid.setSelection(0);
        if (focusSearch) {
            search.requestFocus();
            ui.postDelayed(new Runnable() {
                @Override public void run() {
                    InputMethodManager im = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
                    im.showSoftInput(search, InputMethodManager.SHOW_IMPLICIT);
                }
            }, 150);
        }
    }

    void closeDrawer() {
        if (!drawerOpen) return;
        drawerOpen = false;
        InputMethodManager im = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        im.hideSoftInputFromWindow(search.getWindowToken(), 0);
        search.clearFocus();
        drawer.animate().alpha(0f).translationY(dp(80)).setDuration(200).withEndAction(new Runnable() {
            @Override public void run() { if (!drawerOpen) drawer.setVisibility(View.GONE); }
        }).start();
        home.animate().alpha(1f).setDuration(220).start();
    }

    /* Recherche sans tenir compte des accents ni des majuscules */
    static String fold(String s) {
        String n = Normalizer.normalize(s, Normalizer.Form.NFD);
        return n.replaceAll("\\p{M}", "").toLowerCase(Locale.FRENCH);
    }

    void filter(String q) {
        shown.clear();
        String f = fold(q.trim());
        List<AppInfo> starts = new ArrayList<>(), contains = new ArrayList<>();
        for (AppInfo a : allApps) {
            String l = fold(a.label);
            if (f.isEmpty() || l.startsWith(f)) starts.add(a);
            else if (l.contains(f)) contains.add(a);
        }
        shown.addAll(starts);
        shown.addAll(contains);
        adapter.notifyDataSetChanged();
    }

    class AppAdapter extends BaseAdapter {
        public int getCount() { return shown.size(); }
        public Object getItem(int i) { return shown.get(i); }
        public long getItemId(int i) { return i; }
        public View getView(int i, View convert, ViewGroup parent) {
            AppCellHolder h;
            if (convert == null) {
                h = new AppCellHolder();
                convert = h.build(HomeActivity.this);
            } else h = (AppCellHolder) convert.getTag();
            h.bind(shown.get(i));
            return convert;
        }
    }

    class AppCellHolder {
        ImageView icon;
        TextView label;
        View build(Context c) {
            LinearLayout cell = new LinearLayout(c);
            cell.setOrientation(LinearLayout.VERTICAL);
            cell.setGravity(Gravity.CENTER_HORIZONTAL);
            icon = new ImageView(c);
            cell.addView(icon, new LinearLayout.LayoutParams(dp(52), dp(52)));
            label = text("", 12, Color.WHITE, false);
            label.setSingleLine(true);
            label.setEllipsize(TextUtils.TruncateAt.END);
            label.setGravity(Gravity.CENTER);
            label.setShadowLayer(dp(2), 0, dp(1), 0x99000000);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
            lp.topMargin = dp(6);
            cell.addView(label, lp);
            cell.setTag(this);
            return cell;
        }
        void bind(AppInfo a) {
            icon.setImageDrawable(a.icon);
            label.setText(a.label);
        }
    }

    View appCell(final AppInfo a, boolean withLabel) {
        AppCellHolder h = new AppCellHolder();
        View v = h.build(this);
        h.bind(a);
        if (!withLabel) h.label.setVisibility(View.GONE);
        v.setPadding(0, dp(6), 0, dp(6));
        v.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View x) { launch(a); }
        });
        v.setOnLongClickListener(new View.OnLongClickListener() {
            @Override public boolean onLongClick(View x) { appMenu(x, a); return true; }
        });
        return v;
    }

    /* ================================================================
     *  Actions sur les applis
     * ================================================================ */

    void launch(AppInfo a) {
        Intent it = new Intent(Intent.ACTION_MAIN);
        it.addCategory(Intent.CATEGORY_LAUNCHER);
        it.setComponent(a.component());
        it.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED);
        try {
            startActivity(it);
        } catch (Exception e) {
            Toast.makeText(this, "Impossible d'ouvrir " + a.label, Toast.LENGTH_SHORT).show();
        }
    }

    void appMenu(View anchor, final AppInfo a) {
        final boolean fav = favKeys().contains(a.key);
        String[] items = { fav ? "Retirer de l'accueil" : "Ajouter à l'accueil", "Infos sur l'appli", "Désinstaller" };
        new AlertDialog.Builder(this)
            .setTitle(a.label)
            .setItems(items, new DialogInterface.OnClickListener() {
                @Override public void onClick(DialogInterface d, int which) {
                    if (which == 0) {
                        List<String> keys = favKeys();
                        if (fav) keys.remove(a.key);
                        else if (keys.size() >= MAX_FAVS) {
                            Toast.makeText(HomeActivity.this, "8 favoris au maximum", Toast.LENGTH_SHORT).show();
                            return;
                        } else keys.add(a.key);
                        saveFavKeys(keys);
                        refreshFavorites();
                        if (!fav) Toast.makeText(HomeActivity.this, a.label + " ajouté à l'accueil", Toast.LENGTH_SHORT).show();
                    } else if (which == 1) {
                        startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + a.pkg)));
                    } else {
                        startActivity(new Intent(Intent.ACTION_DELETE, Uri.parse("package:" + a.pkg)));
                    }
                }
            })
            .show();
    }

    void expandNotifications() {
        /* Méthode officielle : notre service d'accessibilité.
           (L'ancienne fonction interne d'Android est bloquée depuis Android 9.) */
        if (GestureService.notifications()) return;
        askGestureService("ouvrir les notifications en glissant vers le bas");
    }

    void askGestureService(String what) {
        new AlertDialog.Builder(this)
            .setTitle("Activer les gestes")
            .setMessage("Pour " + what + ", Android demande d'activer le service « Tonton Launcher : gestes ».\n\n"
                + "1. Touchez « Ouvrir les réglages ».\n"
                + "2. Cherchez « Tonton Launcher : gestes » (parfois dans « Applis installées » ou « Services téléchargés »).\n"
                + "3. Activez-le.\n\n"
                + "Si le bouton est grisé (« paramètre restreint ») : Paramètres → Applis → Tonton Launcher → menu ⋮ en haut à droite → « Autoriser les paramètres restreints », puis recommencez.\n\n"
                + "Ce service ne lit pas le contenu de vos applis.")
            .setPositiveButton("Ouvrir les réglages", new DialogInterface.OnClickListener() {
                @Override public void onClick(DialogInterface d, int w) {
                    try { startActivity(new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)); }
                    catch (Exception e) { startActivity(new Intent(Settings.ACTION_SETTINGS)); }
                }
            })
            .setNegativeButton("Plus tard", null)
            .show();
    }

    /* ================================================================
     *  Favoris (stockés sous la forme "paquet/classe" séparés par ;)
     * ================================================================ */

    List<String> favKeys() {
        List<String> out = new ArrayList<>();
        for (String s : prefs.getString("favs", "").split(";")) if (!s.isEmpty()) out.add(s);
        return out;
    }

    void saveFavKeys(List<String> keys) {
        prefs.edit().putString("favs", TextUtils.join(";", keys)).apply();
    }

    AppInfo findApp(String key) {
        for (AppInfo a : allApps) if (a.key.equals(key)) return a;
        return null;
    }

    AppInfo findAppByPackage(String pkg) {
        for (AppInfo a : allApps) if (a.pkg.equals(pkg)) return a;
        return null;
    }

    /* ================================================================
     *  Chargement des applis (en arrière-plan : les icônes sont lentes)
     * ================================================================ */

    void loadApps() {
        new Thread(new Runnable() {
            @Override public void run() {
                PackageManager pm = getPackageManager();
                Intent main = new Intent(Intent.ACTION_MAIN);
                main.addCategory(Intent.CATEGORY_LAUNCHER);
                List<ResolveInfo> list = pm.queryIntentActivities(main, 0);
                final List<AppInfo> apps = new ArrayList<>();
                for (ResolveInfo ri : list) {
                    ActivityInfo ai = ri.activityInfo;
                    if (ai.packageName.equals(getPackageName())) continue;
                    AppInfo a = new AppInfo();
                    a.pkg = ai.packageName;
                    a.cls = ai.name;
                    a.key = a.pkg + "/" + a.cls;
                    a.label = String.valueOf(ri.loadLabel(pm));
                    a.icon = ri.loadIcon(pm);
                    apps.add(a);
                }
                final Collator col = Collator.getInstance(Locale.FRENCH);
                col.setStrength(Collator.PRIMARY);
                Collections.sort(apps, new Comparator<AppInfo>() {
                    @Override public int compare(AppInfo x, AppInfo y) { return col.compare(x.label, y.label); }
                });
                ui.post(new Runnable() {
                    @Override public void run() {
                        allApps.clear();
                        allApps.addAll(apps);
                        filter(search.getText().toString());
                        refreshFavorites();
                        refreshDock();
                    }
                });
            }
        }).start();
    }

    final BroadcastReceiver packageReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) { loadApps(); }
    };
    final BroadcastReceiver timeReceiver = new BroadcastReceiver() {
        @Override public void onReceive(Context c, Intent i) { updateClock(); }
    };

    /* ================================================================
     *  Réglages
     * ================================================================ */

    void applyTheme() {
        int wp = prefs.getInt("wallpaper", 0);
        if (wp < WALLPAPERS.length) {
            GradientDrawable g = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM, WALLPAPERS[wp]);
            root.setBackground(g);
        } else root.setBackground(null);   /* le fond d'écran du téléphone apparaît derrière */
        drawer.setBackgroundColor(wp < WALLPAPERS.length ? 0xE6101422 : 0xD9101422);
        search.setBackground(rounded(0x26FFFFFF, dp(25), accent));
        View pill = home.findViewWithTag("pill");
        if (pill != null) pill.setBackground(rounded(0x33FFFFFF, dp(25), 0));
        grid.setNumColumns(columns);
        updateClock();
    }

    void showSettings() {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setPadding(dp(24), dp(8), dp(24), 0);

        box.addView(sectionTitle("Votre prénom"));
        final EditText name = new EditText(this);
        name.setText(prefs.getString("name", "Tonton"));
        name.setSingleLine(true);
        box.addView(name);

        box.addView(sectionTitle("Fond d'écran"));
        final int[] wp = { prefs.getInt("wallpaper", 0) };
        box.addView(choiceRow(WALLPAPER_NAMES, wp, null));

        box.addView(sectionTitle("Couleur d'accent"));
        final int[] ac = { prefs.getInt("accent", 0) };
        box.addView(choiceRow(ACCENT_NAMES, ac, ACCENTS));

        box.addView(sectionTitle("Colonnes du tiroir d'applis"));
        final int[] co = { prefs.getInt("columns", 4) == 5 ? 1 : 0 };
        box.addView(choiceRow(new String[] { "4 colonnes", "5 colonnes" }, co, null));

        TextView about = text("Tonton Launcher 1.1 - dans le style de TontonOS\nGestes : " + (GestureService.instance != null ? "activés" : "désactivés"), 12, 0xFF888888, false);
        about.setPadding(0, dp(18), 0, dp(8));
        box.addView(about);

        android.widget.ScrollView sv = new android.widget.ScrollView(this);
        sv.addView(box);
        new AlertDialog.Builder(this)
            .setTitle("Réglages")
            .setView(sv)
            .setPositiveButton("Enregistrer", new DialogInterface.OnClickListener() {
                @Override public void onClick(DialogInterface d, int which) {
                    prefs.edit()
                        .putString("name", name.getText().toString().trim())
                        .putInt("wallpaper", wp[0])
                        .putInt("accent", ac[0])
                        .putInt("columns", co[0] == 1 ? 5 : 4)
                        .apply();
                    readPrefs();
                    applyTheme();
                }
            })
            .setNeutralButton("Écran d'accueil par défaut", new DialogInterface.OnClickListener() {
                @Override public void onClick(DialogInterface d, int which) {
                    try { startActivity(new Intent(Settings.ACTION_HOME_SETTINGS)); }
                    catch (Exception e) { startActivity(new Intent(Settings.ACTION_SETTINGS)); }
                }
            })
            .setNegativeButton("Annuler", null)
            .show();
    }

    TextView sectionTitle(String s) {
        TextView t = text(s, 14, accent, true);
        t.setPadding(0, dp(16), 0, dp(6));
        return t;
    }

    /* Rangée de « pastilles » à choix unique */
    View choiceRow(final String[] names, final int[] sel, final int[] colors) {
        final LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        final List<TextView> chips = new ArrayList<>();
        for (int i = 0; i < names.length; i++) {
            final int idx = i;
            TextView c = text(names[i], 15, 0xFF222222, false);
            c.setPadding(dp(14), dp(10), dp(14), dp(10));
            if (colors != null) {
                GradientDrawable dot = new GradientDrawable();
                dot.setShape(GradientDrawable.OVAL);
                dot.setColor(colors[i]);
                dot.setSize(dp(16), dp(16));
                c.setCompoundDrawablesWithIntrinsicBounds(dot, null, null, null);
                c.setCompoundDrawablePadding(dp(12));
            }
            c.setOnClickListener(new View.OnClickListener() {
                @Override public void onClick(View v) {
                    sel[0] = idx;
                    for (int k = 0; k < chips.size(); k++) styleChip(chips.get(k), k == sel[0]);
                }
            });
            chips.add(c);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(-1, -2);
            lp.bottomMargin = dp(4);
            col.addView(c, lp);
        }
        for (int k = 0; k < chips.size(); k++) styleChip(chips.get(k), k == sel[0]);
        return col;
    }

    void styleChip(TextView c, boolean on) {
        c.setBackground(rounded(on ? (accent & 0x00FFFFFF) | 0x22000000 : 0x0F000000, dp(12), on ? accent : 0));
        c.setTypeface(null, on ? Typeface.BOLD : Typeface.NORMAL);
    }

    /* ================================================================
     *  Petits outils
     * ================================================================ */

    int dp(float v) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()));
    }

    TextView text(String s, float sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        t.setTextColor(color);
        if (bold) t.setTypeface(Typeface.DEFAULT_BOLD);
        return t;
    }

    GradientDrawable rounded(int fill, int radius, int stroke) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        g.setCornerRadius(radius);
        if (stroke != 0) g.setStroke(dp(1.5f), stroke);
        return g;
    }
}
