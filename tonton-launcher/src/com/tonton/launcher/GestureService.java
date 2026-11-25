package com.tonton.launcher;

import android.accessibilityservice.AccessibilityService;
import android.view.accessibility.AccessibilityEvent;

/**
 * Service d'accessibilité minimal : il ne lit rien, il sert seulement à demander
 * au système d'ouvrir les notifications ou de verrouiller l'écran.
 */
public class GestureService extends AccessibilityService {
    static GestureService instance;

    /* Constante GLOBAL_ACTION_LOCK_SCREEN (Android 9 et plus) */
    static final int ACTION_LOCK_SCREEN = 8;

    @Override
    protected void onServiceConnected() {
        super.onServiceConnected();
        instance = this;
    }

    @Override
    public void onDestroy() {
        instance = null;
        super.onDestroy();
    }

    @Override public void onAccessibilityEvent(AccessibilityEvent event) { }
    @Override public void onInterrupt() { }

    static boolean notifications() {
        return instance != null && instance.performGlobalAction(GLOBAL_ACTION_NOTIFICATIONS);
    }

    static boolean lockScreen() {
        return instance != null && android.os.Build.VERSION.SDK_INT >= 28
                && instance.performGlobalAction(ACTION_LOCK_SCREEN);
    }
}
