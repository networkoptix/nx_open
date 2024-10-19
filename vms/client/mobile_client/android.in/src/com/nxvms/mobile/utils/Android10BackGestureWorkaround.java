// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import android.os.SystemClock;
import android.view.MotionEvent;
import com.nxvms.mobile.utils.QnWindowUtils;

public class Android10BackGestureWorkaround
{
    public static native void notifyBackGestureStarted();

    private static void sendFakeMouseEvent(int action, long downTime, long eventTime)
    {
        final float x = 1;
        final float y = 1;
        final int metaState = 0;

        MotionEvent motionEvent = MotionEvent.obtain(downTime, eventTime, action, x, y, metaState);

        // Dispatches touch event to view.
        QnWindowUtils.activity().getWindow().getDecorView().dispatchTouchEvent(motionEvent);
    }

    public static void workaround()
    {
        try
        {
            notifyBackGestureStarted();

            // Emulate click to avoid need of excessive tap after each back gesture.
            final long downTime = SystemClock.uptimeMillis();
            final long upTime = downTime + 50;
            sendFakeMouseEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
            sendFakeMouseEvent(MotionEvent.ACTION_UP, downTime, upTime);
        }
        catch (UnsatisfiedLinkError e)
        {
        }
    }
}
