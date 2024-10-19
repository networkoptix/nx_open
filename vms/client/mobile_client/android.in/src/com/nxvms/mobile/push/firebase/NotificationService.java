// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.push.firebase;

import java.util.Date;
import java.util.Map;

import android.content.Context;

import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

import org.qtproject.qt.android.QtNative;

import com.nxvms.mobile.utils.Logger;
import com.nxvms.mobile.push.PushMessageManager;

public class NotificationService extends FirebaseMessagingService
{
    private static String kLogTag = "Firebase notification service";

    @Override
    public void onCreate()
    {
        Logger.updateContext(this);
        Logger.info(kLogTag, "Service creation: started");

        super.onCreate();
        PushMessageManager.ensureInitialized(this);
        Logger.info(kLogTag, "Service creation: finished");
    }

    @Override
    public void onMessageReceived(RemoteMessage message)
    {
        Logger.info(kLogTag, String.format(
            "Message received, ttl=%d, priority=%d, orig.priority=%d, type=<%s>, sent=%s",
            message.getTtl(), message.getPriority(), message.getOriginalPriority(),
            message.getMessageType(),
            (new Date(message.getSentTime())).toString()));

        final Map<String, String> payload = message.getData();
        PushMessageManager.PushContext pushContext = new PushMessageManager.PushContext();
        pushContext.context = this;
        pushContext.caption = payload.get("caption");
        pushContext.description = payload.get("description");
        pushContext.targets = payload.get("targets");
        pushContext.url = payload.get("url");
        pushContext.cloudSystemId = payload.get("systemId");
        pushContext.imageUrl = payload.get("imageUrl");

        PushMessageManager.handleMessage(pushContext);
    }
}
