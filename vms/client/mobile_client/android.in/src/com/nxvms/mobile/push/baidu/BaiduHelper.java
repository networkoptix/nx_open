// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.push.baidu;

import android.app.Activity;
import android.text.TextUtils;
import android.os.Bundle;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import com.baidu.android.pushservice.PushManager;
import com.baidu.android.pushservice.PushConstants;

import com.nxvms.mobile.utils.Logger;
import com.nxvms.mobile.utils.QnWindowUtils;

public class BaiduHelper
{
    // Binding to the c++ function.
    public static native void storeTokenData(
        String token,
        String userId);

    public static native void resetTokenData();

    private static String kApiKey;
    private static String kLogTag = "Baidu helper";

    static private boolean ensureHasApiKey(Context context)
    {
        if (!TextUtils.isEmpty(kApiKey))
            return true;

        if (context == null)
        {
            Logger.error(kLogTag, "Can't get an API key since specified context is null!");
            return false;
        }

        final String packageName = context.getPackageName();
        try
        {
            final ApplicationInfo info = context.getPackageManager().getApplicationInfo(
                    packageName, PackageManager.GET_META_DATA);

            if (info == null)
            {
                Logger.error(kLogTag, "Can't get an API key since the app info is not available!");
                return false;
            }

            if (info.metaData == null)
            {
                Logger.error(kLogTag, "Can't get an API key since the metadata is not available!");
                return false;
            }

            kApiKey = info.metaData.getString("baidu_api_key");
        }
        catch (NameNotFoundException e)
        {
            Logger.error(kLogTag, "Can't get an API key since the value is not found in metadata.");
            return false;
        }

        if (TextUtils.isEmpty(kApiKey))
        {
            Logger.error(kLogTag, "The API key is empty, something went wrong!");
            return false;
        }

        Logger.info(kLogTag, "Api key is available.");
        return true;
    }

    static public void requestToken()
    {
        final Activity activity = QnWindowUtils.activity();
        final Context context = activity.getApplicationContext();
        if (!ensureHasApiKey(context))
        {
            Logger.error(kLogTag, "Can't request a token since the API key is not specified.");
            return;
        }

        Logger.info(kLogTag, "Requesting the token data.");
        PushManager.startWork(
            context,
            PushConstants.LOGIN_TYPE_API_KEY,
            kApiKey);
    }

    static public void resetToken()
    {
        Logger.info(kLogTag, "Resetting the token data.");

        final Activity activity = QnWindowUtils.activity();
        if (PushManager.isPushEnabled(activity))
            PushManager.stopWork(activity.getApplicationContext());
        else
            BaiduHelper.resetTokenData();
    }
}
