// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile;

import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.net.Uri;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.hardware.SensorManager;
import android.provider.Settings;
import android.provider.Settings.System;
import android.provider.Settings.SettingNotFoundException;
import android.view.inputmethod.EditorInfo;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.OrientationEventListener;
import android.database.ContentObserver;
import java.io.File;
import java.lang.reflect.Method;
import java.lang.reflect.Field;

import org.qtproject.qt.android.bindings.QtActivity;
import com.nxvms.mobile.utils.Logger;
import com.nxvms.mobile.utils.QnWindowUtils;
import com.nxvms.mobile.utils.Android10BackGestureWorkaround;
import com.nxvms.mobile.push.PushMessageManager;

public class QnActivity extends QtActivity
{
    private void deleteFileObject(File fileObject)
    {
        if (fileObject == null)
            return;

        if (fileObject.isDirectory())
        {
            String[] children = fileObject.list();
            for (int i = 0; i < children.length; i++)
                deleteFileObject(new File(fileObject, children[i]));
        }

        fileObject.delete();
    }

    private void clearAppCache()
    {
        try
        {
            deleteFileObject(getCacheDir());
        }
        catch(Exception e)
        {
        }
    }

    private OrientationEventListener listener;

    private ContentObserver autoRotationSettingObserver;

    private void tryEnableAutoRotationForActivity()
    {
        try
        {
            final String setting = Settings.System.ACCELEROMETER_ROTATION;
            if (Settings.System.getInt(getContentResolver(), setting) == 1)
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
        }
        catch (SettingNotFoundException e)
        {
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        QnWindowUtils.setActivity(this);
        QnWindowUtils.prepareSystemUi();

        // In some cases cache existence leads to the crash after application version upgrade.
        clearAppCache();

        PushMessageManager.ensureInitialized(this);

        autoRotationSettingObserver = new ContentObserver(new Handler())
        {
            @Override
            public void onChange(boolean selfChange)
            {
                super.onChange(selfChange);

                tryEnableAutoRotationForActivity();
            }

            @Override
            public boolean deliverSelfNotifications() { return true; }
        };

        listener = new OrientationEventListener(this, SensorManager.SENSOR_DELAY_UI)
        {
            @Override
            public void onOrientationChanged (int orientation)
            {
                final int requested = getRequestedOrientation();
                final boolean isPortrait = (orientation >= 135 && orientation <= 225)
                    || orientation > 315
                    || orientation < 45;

                // Cancel screen lock after programmatical rotation (just in case).
                if ((requested == ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT) == isPortrait
                    && requested != ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED)
                {
                    // Avoid glitches.
                    final Handler handler = new Handler(Looper.getMainLooper());
                    handler.postDelayed(new Runnable()
                    {
                        @Override
                        public void run()
                        {
                            tryEnableAutoRotationForActivity();
                        }
                    }, 500);
                }
            }
        };

        if (listener.canDetectOrientation())
            listener.enable();
        else
            listener = null;

        initializeTextControlWorkaround();
    }

    /**
     *  Workarounds QTBUG-134417.
     *  This is a workaround preventing showing of overlapping fullscreen native
     *  text field in landscape mode and leaving our custom control usage.
     *  Native control looks bad, has wrong backgound and standard button style and
     *  does not correspond to our UX.
     */
    private void initializeTextControlWorkaround()
    {
        final View rootView = getWindow().getDecorView();
        rootView.getViewTreeObserver().addOnGlobalLayoutListener(
            new ViewTreeObserver.OnGlobalLayoutListener()
            {
                @Override
                public void onGlobalLayout()
                {
                    // Call on any view tree changes.
                    tryDenyFullscreenTextControls(rootView);
                }

                private void tryDenyFullscreenTextControls(View view)
                {
                    if (view instanceof ViewGroup)
                    {
                        ViewGroup group = (ViewGroup) view;
                        for (int i = 0; i < group.getChildCount(); i++)
                            tryDenyFullscreenTextControls(group.getChildAt(i));

                        return;
                    }

                    /**
                     * QtEditText control is responsible for all text-management controls in Qt.
                     * It has specific field and function which can be used to specify the desired
                     * bahavior. If "view" does not have - it's ok, we just skip this object and
                     * look further.
                     */
                    try
                    {
                        Field imeOptions = view.getClass().getDeclaredField("m_imeOptions");
                        imeOptions.setAccessible(true);
                        int flags = imeOptions.getInt(view);

                        Method setImeOptions = view.getClass().getDeclaredMethod(
                            "setImeOptions", int.class);
                        setImeOptions.setAccessible(true);
                        setImeOptions.invoke(view, flags | EditorInfo.IME_FLAG_NO_EXTRACT_UI);
                    }
                    catch (Exception e)
                    {
                        // Do nothing, that's expected situation.
                    }
                }
            });
    }

    @Override
    protected void onDestroy()
    {
        QnWindowUtils.setActivity(null);

        if (listener != null)
            listener.disable();

        super.onDestroy();
    }

    @Override
    public void onNewIntent(Intent intent)
    {
        setIntent(intent);

        super.onNewIntent(intent);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig)
    {
        super.onConfigurationChanged(newConfig);

        QnWindowUtils.handleOrientationChanged();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event)
    {

        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.P
            && event.getActionMasked() == MotionEvent.ACTION_CANCEL)
        {
            // We run workaround on any motion cancel event.
            // TODO: #ynikitenkov Check if the newest version of Qt fixes it.
            Android10BackGestureWorkaround.workaround();
        }

        return super.dispatchTouchEvent(event);
    }
}
