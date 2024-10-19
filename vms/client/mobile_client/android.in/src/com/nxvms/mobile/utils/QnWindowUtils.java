// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import android.annotation.TargetApi;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.Manifest;
import android.os.Build;
import android.os.Vibrator;
import android.provider.Settings;
import android.text.format.DateFormat;
import android.view.View;
import android.view.Window;
import android.view.Display;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowManager.LayoutParams;
import android.util.DisplayMetrics;
import android.widget.Toast;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import java.lang.Math;
import java.lang.ref.WeakReference;

import com.nxvms.mobile.utils.Logger;

public class QnWindowUtils
{
    private static class VisibilityChanger implements Runnable
    {
        private enum Operation
        {
            Prepare,
            Show,
            Hide
        }

        Activity mActivity;
        Operation mOperation;

        public VisibilityChanger(Activity activity, Operation operation)
        {
            mActivity = activity;
            mOperation = operation;
        }

        @Override public void run()
        {
            switch (mOperation)
            {
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

        private void showSystemUi()
        {
            mActivity.getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }

        private void hideSystemUi()
        {
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
            Window window = mActivity.getWindow();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
            {
                final int kFlags =
                    WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS
                    | WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION;
                window.setFlags(kFlags, kFlags);
                window.setStatusBarColor(Color.TRANSPARENT);
                window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
            }

            QnWindowUtils.handleOrientationChanged();

            showSystemUi();
        }
    }

    private static WeakReference<Activity> m_activity = null;

    public static void setActivity(Activity activity)
    {
        m_activity = new WeakReference<>(activity);
    }

    public static Activity activity()
    {
        return m_activity != null ? m_activity.get() : null;
    }

    public static void handleOrientationChanged()
    {
        setCutoutEnabled(getOrientation() == Configuration.ORIENTATION_PORTRAIT);
    }

    private static void setCutoutEnabled(boolean enabled)
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P)
            return;

        final Window window = activity().getWindow();
        final WindowManager.LayoutParams attributes = window.getAttributes();
        attributes.layoutInDisplayCutoutMode = enabled
            ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
        window.setAttributes(attributes);
    }

    private static boolean isFullscreenMode()
    {
        return (activity().getWindow().getDecorView().getSystemUiVisibility()
            & View.SYSTEM_UI_FLAG_FULLSCREEN) != 0;
    }

    private static int getOrientation()
    {
        final Resources resources = activity().getResources();
        return resources.getConfiguration().orientation;
    }

    public static void prepareSystemUi()
    {
        activity().runOnUiThread(
            new VisibilityChanger(activity(), VisibilityChanger.Operation.Prepare));
    }

    public static void showSystemUi()
    {
        activity().runOnUiThread(
            new VisibilityChanger(activity(), VisibilityChanger.Operation.Show));
    }

    public static void hideSystemUi()
    {
        activity().runOnUiThread(
            new VisibilityChanger(activity(), VisibilityChanger.Operation.Hide));
    }

    // TODO: #ynikitenkov Get rid of this function when minimal Android support version will be 7.0.
    public static boolean isLeftSideNavigationBar()
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N)
            return false;

        View view = activity().getWindow().getDecorView();
        return view.getRootWindowInsets().getStableInsetLeft() > 0;
    }

    public static int keyboardHeight()
    {
        final View view = activity().getWindow().getDecorView();
        WindowInsets insets = view.getRootWindowInsets();
        if (insets == null)
            return 0;

        final int kMinKeyboardHeight = 80;
        final int bottom = insets.getSystemWindowInsetBottom();
        return bottom < kMinKeyboardHeight
            ? 0
            : bottom;
    }

    public static int navigationBarType(
        final int noBarValue,
        final int bottomBarValue,
        final int leftBarValue,
        final int rightBarValue)
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N)
        {
            return isLeftSideNavigationBar()
                ? leftBarValue
                : rightBarValue;
        }

        final View view = activity().getWindow().getDecorView();
        final WindowInsets insets = view.getRootWindowInsets();

        final int bottomInset = insets.getStableInsetBottom();
        final int leftInset = insets.getStableInsetLeft();
        final int rightInset = insets.getStableInsetRight();

        if (bottomInset > 0)
            return bottomBarValue;

        if (leftInset > 0)
            return leftBarValue;

        if (rightInset > 0)
            return rightBarValue;

        return noBarValue;
    }

    public static boolean hasNavigationBar()
    {
        final Display display = activity().getWindow().getWindowManager().getDefaultDisplay();

        final DisplayMetrics realDisplayMetrics = new DisplayMetrics();
        display.getRealMetrics(realDisplayMetrics);

        final int realHeight = realDisplayMetrics.heightPixels;
        final int realWidth = realDisplayMetrics.widthPixels;

        final DisplayMetrics displayMetrics = new DisplayMetrics();
        display.getMetrics(displayMetrics);

        final int displayHeight = displayMetrics.heightPixels;
        final int displayWidth = displayMetrics.widthPixels;

        return (realWidth - displayWidth) > 0 || (realHeight - displayHeight) > 0;
    }

    public static void showOsPushSettingsScreen()
    {
        final Activity context = activity();

        final Intent intent = new Intent();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
        {
            intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
            intent.putExtra(Settings.EXTRA_APP_PACKAGE, context.getPackageName());
        }
        else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP)
        {
            intent.setAction("android.settings.APP_NOTIFICATION_SETTINGS");
            intent.putExtra("app_package", context.getPackageName());
            intent.putExtra("app_uid", context.getApplicationInfo().uid);
        }
        else
        {
            // Unsupported OS version
            return;
        }

        context.startActivity(intent);
    }

    public static int getNavigationBarSize()
    {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N)
            return getModernNavigationBarSize();

        if (hasNavigationBar())
        {
            final Resources resources = activity().getResources();
            final int orientation = getOrientation();

            int resourceId = 0;

            if (isPhone())
            {
                final String tag =
                    orientation == Configuration.ORIENTATION_PORTRAIT
                        ? "navigation_bar_height"
                        : "navigation_bar_width";
                resourceId = resources.getIdentifier(tag, "dimen", "android");
            }
            else
            {
                // TODO: #ynikitenkov Get rid of this section when minimal support Android
                // version will be 7.0.
                final String tag =
                    orientation == Configuration.ORIENTATION_PORTRAIT
                        ? "navigation_bar_height"
                        : "navigation_bar_height_landscape";
                resourceId = resources.getIdentifier(tag, "dimen", "android");
            }

            if (resourceId > 0)
                return resources.getDimensionPixelSize(resourceId);
        }

        return 0;
    }

    private static int getModernNavigationBarSize()
    {
        final View view = activity().getWindow().getDecorView();
        final WindowInsets insets = view.getRootWindowInsets();

        // In modern android sometimes there are no sidebars but small swipe panel
        // at the bottom of the screen.
        final int bottomInset = insets.getStableInsetBottom();
        if (bottomInset != 0)
            return bottomInset;

        return Math.max(insets.getStableInsetLeft(), insets.getStableInsetRight());
    }

    public static int getStatusBarHeight()
    {
        final Resources resources = activity().getResources();
        final int resourceId = resources.getIdentifier("status_bar_height", "dimen", "android");
        return resourceId > 0
            ? resources.getDimensionPixelSize(resourceId)
            : 0;
    }

    public static boolean isPhone()
    {
        final int layoutMask = activity().getResources().getConfiguration().screenLayout;
        return (layoutMask & Configuration.SCREENLAYOUT_SIZE_MASK) < Configuration.SCREENLAYOUT_SIZE_LARGE;
    }

    public static boolean is24HoursTimeFormat()
    {
        return DateFormat.is24HourFormat(activity());
    }

    public static void setKeepScreenOn(final boolean keepScreenOn)
    {
        activity().runOnUiThread(new Runnable()
        {
            final boolean mkeepScreenOn = keepScreenOn;
            final Activity mActivity = activity();

            @Override public void run()
            {
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
        activity().runOnUiThread(
            new Runnable()
            {
                final int mScreenOrientation = screenOrientation;
                final Activity mActivity = activity();

                @Override public void run()
                {
                    mActivity.setRequestedOrientation(mScreenOrientation);
                }
            });
    }

    private static final int kVibrationDurationMs = 20;
    public static void makeShortVibration()
    {
        final Vibrator vibrator = (Vibrator) activity().getSystemService(Context.VIBRATOR_SERVICE);
        if (vibrator != null)
            vibrator.vibrate(kVibrationDurationMs);
    }

    public static void requestRecordAudioPermissionIfNeeded()
    {
        final String kLogTag = "request audio";

        Logger.infoToSystemOnly(kLogTag, "start.");
        final String permission = Manifest.permission.RECORD_AUDIO;
        final int status = ContextCompat.checkSelfPermission(activity(), permission);

        if (status == PackageManager.PERMISSION_GRANTED)
        {
            Logger.infoToSystemOnly(kLogTag, "permission has granted already.");
            return;
        }

        final int kRequestCode = 1; // Any code greater than 0.
        ActivityCompat.requestPermissions(activity(), new String[]{permission}, kRequestCode);
        Logger.infoToSystemOnly(kLogTag, "finished, requested audio record permission.");
    }
}
