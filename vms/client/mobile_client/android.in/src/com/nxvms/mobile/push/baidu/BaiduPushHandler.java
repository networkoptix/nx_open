// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.push.baidu;

import java.util.List;
import org.json.JSONObject;
import org.json.JSONException;

import android.content.Context;

import com.baidu.android.pushservice.PushMessageReceiver;

import com.nxvms.mobile.utils.Logger;
import com.nxvms.mobile.push.PushMessageManager;
import com.nxvms.mobile.push.baidu.BaiduHelper;

public class BaiduPushHandler extends PushMessageReceiver
{
    private static String kLogTag = "Baidu push handler";

    @Override
    public void onBind(
        Context context,
        int errorCode,
        String appid,
        String userId,
        String channelId,
        String requestId)
    {
        if (errorCode != 0)
        {
            Logger.error(kLogTag, "Error while binding the receiver: " + errorCode);
            BaiduHelper.resetTokenData();
        }
        else
        {
            Logger.info(kLogTag, "The receiver has been bound.");
            BaiduHelper.storeTokenData(channelId, userId);
        }
    }

    @Override
    public void onUnbind(
        Context context,
        int errorCode,
        String requestId)
    {
        if (errorCode == 0)
            Logger.info(kLogTag, "The receiver has been unbound");
        else
            Logger.error(kLogTag, "Error while unbinding receiver: " + errorCode);

        BaiduHelper.resetTokenData();
    }

    @Override
    public void onMessage(
        Context context,
        String message,
        String customContentString,
        int notifyId,
        int source)
    {
        try
        {
            Logger.info(kLogTag, "Baidu data message has arrived");

            final JSONObject json = new JSONObject(message);
            final JSONObject customContent = json.getJSONObject("custom_content");

            PushMessageManager.PushContext pushContext = new PushMessageManager.PushContext();

            pushContext.context = context;
            pushContext.caption = customContent.optString("caption", "");
            pushContext.description = customContent.optString("description", "");
            pushContext.targets = customContent.optString("targets", "");
            pushContext.url = customContent.optString("url", "");
            pushContext.cloudSystemId = customContent.optString("systemId", "");
            pushContext.imageUrl = customContent.optString("imageUrl", "");

            PushMessageManager.handleMessage(pushContext);
        }
        catch(JSONException error)
        {
            Logger.error(kLogTag, "Can't handle the data message, error is: " + error);
        }
    }

    @Override
    public void onNotificationArrived(
        Context context, String title,
        String description,
        String customContentString)
    {
        // Empty implementation. We don't use this functionality.
    }

    @Override
    public void onNotificationClicked(
        Context context,
        String title,
        String description,
        String customContentString)
    {
        // Empty implementation. We don't use this functionality.
    }

    @Override
    public void onListTags(
        Context context,
        int errorCode,
        List<String> tags,
        String requestId)
    {
        // Empty implementation. We don't use this functionality.
    }

    @Override
    public void onSetTags(
        Context context,
        int errorCode,
        List<String> successTags,
        List<String> failTags,
        String requestId)
    {
        // Empty implementation. We don't use this functionality.
    }

    @Override
    public void onDelTags(
        Context context,
        int errorCode,
        List<String> successTags,
        List<String> failTags,
        String requestId)
    {
        // Empty implementation. We don't use this functionality.
    }
}
