// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import android.content.Context;

public class PushNotificationStorage
{
    // Implemented in push_notification_storage_android.cpp.
    static { System.loadLibrary("push_notification_storage"); }

    public static native void addUserNotification(
        Context context,
        String user,
        String title,
        String description,
        String url,
        String cloudSystemId,
        String imageId);
}
