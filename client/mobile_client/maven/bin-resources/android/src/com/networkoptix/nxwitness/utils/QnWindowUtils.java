package com.networkoptix.nxwitness.utils;

import android.view.View;
import android.view.Window;
import android.view.Display;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.graphics.Color;
import android.graphics.Rect;
import android.content.res.Resources;
import android.content.res.Configuration;
import android.util.DisplayMetrics;
import android.app.Activity;
import android.os.Build;
import android.util.Log;
import android.annotation.TargetApi;
import org.qtproject.qt5.android.QtNative;

public class QnWindowUtils {

    static int statusBarHeight = -1;

    private static class VisibilityChanger implements Runnable {
        public enum Operation {
            Prepare,
            Show,
            Hide
        }

        Activity mActivity;
        Operation mOperation;

        public VisibilityChanger(Activity activity, Operation operation) {
            mActivity = activity;
            mOperation = operation;
        }

        @Override public void run() {
            switch (mOperation) {
            case Prepare:
                prepareSystemUi();
                break;
            case Show:
                showSystemUi();
                break;
            case Hide:
                hideSystemUi();
                break;
            }
        }

        private void showSystemUi() {
            mActivity.getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }

        private void hideSystemUi() {
            mActivity.getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION);
        }

        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        private void prepareSystemUi()
        {
            if (statusBarHeight < 0)
            {
                Resources resources = QtNative.activity().getResources();
                int resourceId = resources.getIdentifier("status_bar_height", "dimen", "android");
                if (resourceId > 0)
                    statusBarHeight = resources.getDimensionPixelSize(resourceId);
            }

            Window window = mActivity.getWindow();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
            {
                window.setFlags(
                    WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS | WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION,
                    WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS | WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
                window.setStatusBarColor(Color.TRANSPARENT);
            }
            showSystemUi();
        }
    }

    public static void prepareSystemUi() {
        Activity activity = QtNative.activity();
        activity.runOnUiThread(new VisibilityChanger(activity, VisibilityChanger.Operation.Prepare));
    }

    public static void showSystemUi() {
        Activity activity = QtNative.activity();
        activity.runOnUiThread(new VisibilityChanger(activity, VisibilityChanger.Operation.Show));
    }

    public static void hideSystemUi() {
        Activity activity = QtNative.activity();
        activity.runOnUiThread(new VisibilityChanger(activity, VisibilityChanger.Operation.Hide));
    }

    public static boolean isLeftSideNavigationBar()
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N)
            return false;

        View view = QtNative.activity().getWindow().getDecorView();
        return view.getRootWindowInsets().getSystemWindowInsetLeft() > 0;
    }

    public static boolean hasNavigationBar() {
        Activity activity = QtNative.activity();
        Display d = activity.getWindow().getWindowManager().getDefaultDisplay();

        DisplayMetrics realDisplayMetrics = new DisplayMetrics();
        d.getRealMetrics(realDisplayMetrics);

        int realHeight = realDisplayMetrics.heightPixels;
        int realWidth = realDisplayMetrics.widthPixels;

        DisplayMetrics displayMetrics = new DisplayMetrics();
        d.getMetrics(displayMetrics);

        int displayHeight = displayMetrics.heightPixels;
        int displayWidth = displayMetrics.widthPixels;

        return (realWidth - displayWidth) > 0 || (realHeight - displayHeight) > 0;
    }

    public static int getNavigationBarHeight() {
        int result = 0;

        if (hasNavigationBar()) {
            Resources resources = QtNative.activity().getResources();

            int orientation = resources.getConfiguration().orientation;
            int resourceId;

            if (isPhone())
                resourceId = resources.getIdentifier(orientation == Configuration.ORIENTATION_PORTRAIT ? "navigation_bar_height" : "navigation_bar_width", "dimen", "android");
            else
                resourceId = resources.getIdentifier(orientation == Configuration.ORIENTATION_PORTRAIT ? "navigation_bar_height" : "navigation_bar_height_landscape", "dimen", "android");

            if (resourceId > 0)
                return resources.getDimensionPixelSize(resourceId);
        }
        return result;
    }

    public static int getStatusBarHeight()
    {
        return statusBarHeight;
    }

    public static boolean isPhone() {
        return (QtNative.activity().getResources().getConfiguration().screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) < Configuration.SCREENLAYOUT_SIZE_LARGE;
    }

    public static void setKeepScreenOn(final boolean keepScreenOn) {
        final Activity activity = QtNative.activity();
        activity.runOnUiThread(new Runnable() {
            boolean mkeepScreenOn = keepScreenOn;
            Activity mActivity = activity;

            @Override public void run() {
                Window window = mActivity.getWindow();
                if (mkeepScreenOn)
                    window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                else
                    window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    public static void setScreenOrientation(final int screenOrientation)
    {
        final Activity activity = QtNative.activity();
        activity.runOnUiThread(
            new Runnable()
            {
                int mScreenOrientation = screenOrientation;
                Activity mActivity = activity;

                @Override public void run()
                {
                    mActivity.setRequestedOrientation(mScreenOrientation);
                }
            });
    }
}
