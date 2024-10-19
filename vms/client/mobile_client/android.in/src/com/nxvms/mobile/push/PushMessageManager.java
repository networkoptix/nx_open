// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.push;

import com.nxvms.mobile.utils.HttpDigestAuth;
import com.nxvms.mobile.utils.Logger;
import com.nxvms.mobile.utils.OAuthHelper;
import com.nxvms.mobile.utils.PushIpcData;
import com.nxvms.mobile.utils.QnWindowUtils;

import java.io.InputStream;
import java.lang.Exception;
import java.net.URL;
import java.net.HttpURLConnection;
import java.util.UUID;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;

import android.os.AsyncTask;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.app.PendingIntent;
import android.app.NotificationManager;
import android.app.NotificationChannel;
import android.app.Notification.BigPictureStyle;
import android.net.Uri;
import android.text.TextUtils;
import android.content.Intent;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.Manifest;
import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationCompat.Builder;
import androidx.core.app.NotificationManagerCompat;
import android.service.notification.StatusBarNotification;

public class PushMessageManager
{
    private static final boolean kUseDefaultChannel =
        android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.O;
    private static final String kNotificationChannelId = kUseDefaultChannel
        ? NotificationChannel.DEFAULT_CHANNEL_ID
        : "GENERAL_NOTIFICATION_CHANNEL";
    private static int notificationIconResourceId = 0;

    public enum AuthMethod
    {
        digest,
        bearer
    };

    public static class PushContext
    {
        public Context context;
        public long when = 0;
        public String tag;
        public String caption;
        public String description;
        public String targets;
        public String url;
        public String cloudSystemId;
        public String imageUrl;
    };

    public static void requestPushNotificationPermission()
    {
        final String[] kPermissions = {android.Manifest.permission.POST_NOTIFICATIONS};
        ActivityCompat.requestPermissions(QnWindowUtils.activity(), kPermissions, 0);
    }

    public static boolean notificationsEnabled()
    {
        final String kLogTag = "notification availability check";

        final Context context = QnWindowUtils.activity();
        if (context == null)
        {
            Logger.infoToSystemOnly(kLogTag, "no context, returning: 'false'.");
            return false;
        }

        final boolean globallyEnabled =
            NotificationManagerCompat.from(context).areNotificationsEnabled();

        if (kUseDefaultChannel)
        {
            Logger.infoToSystemOnly(kLogTag,
                "using the default channel, returning '" + globallyEnabled + "'.");
            return globallyEnabled;
        }

        if(TextUtils.isEmpty(kNotificationChannelId))
        {
            Logger.infoToSystemOnly(kLogTag, "the channel id is empty, returning 'false'.");
            return false;
        }

        final NotificationManager manager = context.getSystemService(NotificationManager.class);
        final NotificationChannel channel = manager.getNotificationChannel(kNotificationChannelId);
        if (channel == null)
        {
            Logger.infoToSystemOnly(kLogTag, "the channel is 'null', channel id is '"
                + kNotificationChannelId + "', returning 'false'.");
            return false;
        }

        final boolean result =
            globallyEnabled && channel.getImportance() != NotificationManager.IMPORTANCE_NONE;
        Logger.infoToSystemOnly(kLogTag, "returning '" + result + "'.");

        return result;
    }

    public static void ensureInitialized(Context context)
    {
        if (notificationIconResourceId != 0)
            return; //< Initialization has done already.

        final Resources resources = context.getResources();
        notificationIconResourceId = resources.getIdentifier(
            "notification_icon", "drawable", context.getPackageName());

        // Create the NotificationChannel, but only on API 26+ because the NotificationChannel
        // class is new and not in the support library.
        if (kUseDefaultChannel)
            return;

        final NotificationManager manager = context.getSystemService(NotificationManager.class);
        NotificationChannel channel = manager.getNotificationChannel(kNotificationChannelId);
        if (channel != null)
            return; //< We've created the channel already.

        channel = new NotificationChannel(
            kNotificationChannelId,
            "General push notifications",
            NotificationManager.IMPORTANCE_HIGH);

        channel.setShowBadge(true);
        manager.createNotificationChannel(channel);
    }

    static public void handleMessage(PushContext context)
    {
        Logger.updateContext(context.context);

        final String kLogTag = "handle push message";

        ensureInitialized(context.context);

        Logger.info(kLogTag, "handling incomming data message.");

        if (TextUtils.isEmpty(context.caption))
        {
            Logger.error(kLogTag, "the title is null, cancelling the notification.");
            return;
        }

        if (TextUtils.isEmpty(context.targets))
        {
            Logger.error(kLogTag, "the targets are not specified, omitting notification.");
            return;
        }

        if (TextUtils.isEmpty(context.cloudSystemId))
        {
            Logger.error(kLogTag, "the targets are not specified, omitting notification.");
            return;
        }

        final PushIpcData.LoginInfo localData = PushIpcData.load(context.context);
        if (localData == null || TextUtils.isEmpty(localData.user))
        {
            Logger.error(kLogTag, "user is not logged into the cloud, omitting notification.");
            return;
        }

        if (context.targets.indexOf("\"" + localData.user + "\"") == -1)
        {
            Logger.error(kLogTag, "current user is not in targets, omitting notification.");
            return;
        }

        showNotificationInternal(context, null);
        if (!TextUtils.isEmpty(context.imageUrl))
        {
            Logger.info(kLogTag, "show notification with preview, loading content.");
            updateNotificationImage(context, localData);
        }
    }

    static private String kNotificationTagPrefix = "mobile_client_push_notification_";
    static private String makeTag(int id)
    {
        return String.format("%s%d", kNotificationTagPrefix, id);
    }

    static private boolean notificationExists(PushContext context)
    {
        final StatusBarNotification[] notifications = context.context.getSystemService(
            NotificationManager.class).getActiveNotifications();
        for (int i = 0; i < notifications.length; ++i)
        {
            if (notifications[i].getTag().equals(context.tag))
                return true;
        }

        return false;
    }

    static public void showNotificationInternal(
        PushContext context,
        Bitmap image)
    {
        final String kLogTag = "show notification";

        Logger.info(kLogTag, "started.");

        final boolean firstTimePresented = TextUtils.isEmpty(context.tag);
        if (firstTimePresented)
        {
            context.tag = kNotificationTagPrefix + UUID.randomUUID().toString();
            context.when = System.currentTimeMillis();
        }
        else if (!PushMessageManager.notificationExists(context))
        {
            Logger.info(kLogTag, "can't update image - notification does not exist.");
            return;
        }

        final NotificationCompat.Builder builder = new NotificationCompat.Builder(
            context.context, kNotificationChannelId)
            .setContentTitle(context.caption)
            .setSmallIcon(notificationIconResourceId)
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)
            .setWhen(context.when)
            .setAutoCancel(true)
            .setOnlyAlertOnce(true);

        if (!TextUtils.isEmpty(context.description))
            builder.setContentText(context.description);

        if (image != null)
        {
            builder.setLargeIcon(image);
            builder.setStyle(
                new NotificationCompat.BigPictureStyle()
                .bigPicture(image)
                .bigLargeIcon((Bitmap) null));
        }
        else if (!TextUtils.isEmpty(context.description))
        {
            builder.setStyle(new NotificationCompat.BigTextStyle().bigText(context.description));
        }

        if (!TextUtils.isEmpty(context.url))
        {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(context.url));
            builder.setContentIntent(PendingIntent.getActivity(context.context, 0, intent,
                PendingIntent.FLAG_IMMUTABLE));
        }

        if (firstTimePresented)
            discardExcessiveNotifications(context.context);

        final NotificationManagerCompat notificationManager =
            NotificationManagerCompat.from(context.context);
        final int kNoMatterIntId = 0;
        notificationManager.notify(context.tag, kNoMatterIntId, builder.build());
    }

    static private int kMaxVisiblePushNotifications = 24;
    static private void discardExcessiveNotifications(Context context)
    {
        final NotificationManager manager = context.getSystemService(NotificationManager.class);
        StatusBarNotification[] notifications = manager.getActiveNotifications();
        if (notifications.length < kMaxVisiblePushNotifications)
            return;

        Arrays.sort(notifications, new Comparator<StatusBarNotification>()
            {
                @Override
                public int compare(StatusBarNotification left, StatusBarNotification right)
                {
                    return Long.compare(left.getNotification().when, right.getNotification().when);
                }
            });

        // Remove all excessive push notifications.
        int toRemove = notifications.length - kMaxVisiblePushNotifications + 1;
        for (int i = 0; i < notifications.length && toRemove > 0; ++i)
        {
            StatusBarNotification notification = notifications[i];

            final String tag = notification.getTag();
            if (!tag.startsWith(kNotificationTagPrefix))
                continue;

            manager.cancel(notification.getTag(), notification.getId());
            --toRemove;
        }
    }

    static public void updateNotificationImage(
        PushContext context,
        PushIpcData.LoginInfo data)
    {
        PushIpcData.TokenInfo tokenInfo =
            PushIpcData.accessToken(context.context, context.cloudSystemId);

        final String serverUrl = OAuthHelper.hostWithPath(context.imageUrl, null);
        OAuthHelper.AccessTokenStatus status = tokenInfo == null
            ? OAuthHelper.AccessTokenStatus.invalid
            : OAuthHelper.systemAccessTokenStatus(tokenInfo.accessToken, serverUrl);

        if (status == OAuthHelper.AccessTokenStatus.unsupported)
        {
            // 4.2 and below servers do not support token authorization.
            NotificationPresenter presenter =
                new NotificationPresenter(context, data, AuthMethod.digest);
            presenter.execute();
            return;
        }

        if (status == OAuthHelper.AccessTokenStatus.invalid)
        {
            tokenInfo = OAuthHelper.requestSystemAccessToken(
                data.user, data.cloudRefreshToken, context.cloudSystemId);

            if (tokenInfo != null)
            {
                status = OAuthHelper.systemAccessTokenStatus(tokenInfo.accessToken, serverUrl);

                if (!TextUtils.isEmpty(tokenInfo.accessToken) && status != OAuthHelper.AccessTokenStatus.valid)
                {
                    Logger.info("gathering token", "resetting new access token since it is not valid.");
                    tokenInfo = null; //< Most likely it is server with version < 5.0.
                }
            }
        }

        if (tokenInfo != null)
        {
            PushIpcData.setAccessToken(
                context.context, context.cloudSystemId, tokenInfo.accessToken, tokenInfo.expiresAt);
        }

        if (tokenInfo != null && !TextUtils.isEmpty(tokenInfo.accessToken)) //< Definitely supports OAuth.
            NotificationPresenter.presentNotification(context, data, AuthMethod.bearer);
        else if (status == OAuthHelper.AccessTokenStatus.unsupported) //< Try to use digest auth.
            NotificationPresenter.presentNotification(context, data, AuthMethod.digest);
        else
            showNotificationInternal(context, null);
    }

    public static final int kHTTPTemporaryRedirect = 307; // Temporary Redirect (since HTTP/1.1)
    public static final int kHTTPPermanentRedirect = 308; // Permanent Redirect (RFC 7538)

    static private class NotificationPresenter extends AsyncTask<Void, Void, Bitmap>
    {
        PushContext context;
        PushIpcData.LoginInfo data;
        AuthMethod method;

        static public void presentNotification(
            PushContext context,
            PushIpcData.LoginInfo data,
            AuthMethod method)
        {
            NotificationPresenter presenter = new NotificationPresenter(context, data, method);
            presenter.execute();
        }

        private NotificationPresenter(
            PushContext targetContext,
            PushIpcData.LoginInfo targetData,
            AuthMethod targetMethod)
        {
            context = targetContext;
            data = targetData;
            method = targetMethod;
        }

        @Override
        protected Bitmap doInBackground(Void... dummy)
        {
            String kLogTag = String.format("%s image loader", method.toString());

            if (TextUtils.isEmpty(data.user))
            {
                Logger.info(kLogTag, "can't load an image, user is empty.");
                return null;
            }

            try
            {
                for (int tryNumber = 0; tryNumber != 3; ++tryNumber)
                {
                    Logger.info(kLogTag, String.format("Trying to load image by url %s, try %d",
                        context.imageUrl, tryNumber + 1));

                    if (TextUtils.isEmpty(context.imageUrl))
                    {
                        Logger.info(kLogTag, "can't load an image, url is empty.");
                        return null;
                    }

                    Logger.info(kLogTag, "loading image.");

                    final URL imageUrl = new URL(context.imageUrl);
                    HttpURLConnection connection = (HttpURLConnection) imageUrl.openConnection();
                    connection.setInstanceFollowRedirects(false);

                    if (method == AuthMethod.bearer)
                    {
                        final PushIpcData.TokenInfo tokenInfo =
                            PushIpcData.accessToken(context.context, context.cloudSystemId);
                        final String accessToken = tokenInfo == null ? null : tokenInfo.accessToken;
                        if (TextUtils.isEmpty(accessToken))
                        {
                            Logger.info(kLogTag, "can't load an image, access token is empty.");
                            return null;
                        }

                        String auth = String.format("Bearer %s", accessToken);
                        connection.setRequestProperty("Authorization", auth);
                    }
                    else if (!TextUtils.isEmpty(data.password))
                    {
                        connection = HttpDigestAuth.tryAuth(connection, data.user, data.password);
                    }
                    else
                    {
                        Logger.info(kLogTag, "can't load an image, password is empty.");
                        return null;
                    }

                    if (connection == null)
                    {
                        Logger.error(kLogTag, "can't load an image, authorization failed.");
                        return null;
                    }

                    final int responseCode = connection.getResponseCode();
                    Logger.info(kLogTag, String.format("Response code is %d", responseCode));

                    switch (responseCode)
                    {
                        case kHTTPTemporaryRedirect:
                        case kHTTPPermanentRedirect:
                        case HttpURLConnection.HTTP_MOVED_PERM:
                        case HttpURLConnection.HTTP_MOVED_TEMP:
                            context.imageUrl = connection.getHeaderField("Location");
                            continue;
                    }

                    final Bitmap result = BitmapFactory.decodeStream(connection.getInputStream());
                    Logger.info(kLogTag, result == null
                        ? "image was not decoded."
                        : "image was decoded.");

                    return result;
                }
            }
            catch (Exception e)
            {
                Logger.error(kLogTag, "can't load an image.");
                return null;
            }

            return null;
        }

        @Override
        protected void onPostExecute(Bitmap image)
        {
            PushMessageManager.showNotificationInternal(context, image);
        }
    }
}
