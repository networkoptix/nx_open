// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import android.os.Build;
import android.provider.Settings.Global;
import android.content.ContentResolver;
import android.app.Activity;
import com.nxvms.mobile.utils.QnWindowUtils;

public class DeviceInfo
{
    private static final String kDefaultDeviceName = "Android device";

    public static String name()
    {
        final Activity activity = QnWindowUtils.activity();
        if (activity == null)
            return kDefaultDeviceName;

        final ContentResolver resolver = activity.getContentResolver();
        if (resolver == null)
            return kDefaultDeviceName;

        final String deviceName = Global.getString(resolver, "device_name");
        return deviceName == null
            ? kDefaultDeviceName
            : deviceName;
    }

    public static String model()
    {
        return android.os.Build.BRAND.toUpperCase()
            + " " + android.os.Build.MODEL;
    }
}
