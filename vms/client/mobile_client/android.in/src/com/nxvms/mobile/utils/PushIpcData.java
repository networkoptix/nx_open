// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import com.nxvms.mobile.utils.Logger;
import com.nxvms.mobile.utils.CloudLoggerOptions;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;
import android.util.Log;
import androidx.security.crypto.MasterKeys;
import androidx.security.crypto.EncryptedSharedPreferences;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import org.json.JSONObject;

public class PushIpcData
{
    private static final String kPushUserKey = "push_user";
    private static final String kCloudRefreshToken = "cloud_refresh_token";

    // Older versions saved the cloud password here for digest authentication. The key is kept only
    // to remove that password.
    private static final String kPushPasswordKey = "push_user_password";

    private static final String kAccessTokensKey = "access_tokens";

    private static final String kLogSessionIdTag = "log_session_id";
    private static final String kLogSessionEndTimeTag = "session_end_time_ms";

    public static class LoginInfo
    {
        public String user;
        public String cloudRefreshToken;
    };

    public static class TokenInfo
    {
        public String accessToken;
        public long expiresAt;

        public TokenInfo(String accessToken, long expiresAt)
        {
            this.accessToken = accessToken;
            this.expiresAt = expiresAt;
        }
    };

    public static void store(
        Context context,
        String user,
        String cloudRefreshToken)
    {
        Logger.updateContext(context);

        String kLogTag = "store IPC data";
        Logger.info(kLogTag, "started.");

        SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ true);
        if (preferences == null)
            return;
        try
        {
            Logger.info(kLogTag, "opening the editor.");
            SharedPreferences.Editor editor = preferences.edit();
            editor.putString(kPushUserKey, user);
            editor.putString(kCloudRefreshToken, cloudRefreshToken);
            editor.remove(kPushPasswordKey);
            editor.commit();
            Logger.info(kLogTag, "data was commited.");
        }
        catch(Exception e)
        {
            Logger.error(kLogTag, "error while storing the data.");
        }
    }

    public static PushIpcData.LoginInfo load(Context context)
    {
        Logger.updateContext(context);

        String kLogTag = "load IPC data";

        Logger.info(kLogTag, "started.");

        SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ true);
        if (preferences == null)
            return null;

        try
        {
            PushIpcData.LoginInfo data = new PushIpcData.LoginInfo();
            data.user = preferences.getString(kPushUserKey, "");
            data.cloudRefreshToken = preferences.getString(kCloudRefreshToken, "");

            Logger.info(kLogTag, "IPC data loaded successfully.");

            return data;
        }
        catch(Exception e)
        {
            Logger.error(kLogTag, "error while loading the data: " + e);
            return null;
        }
    }

    public static void clear(Context context)
    {
        Logger.updateContext(context);

        String kLogTag = "clear IPC data";

        SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ true);
        if (preferences == null)
            return;

        try
        {
            SharedPreferences.Editor editor = preferences.edit();
            editor.remove(kPushUserKey);
            editor.remove(kCloudRefreshToken);
            editor.remove(kPushPasswordKey);
            editor.commit();
            Logger.info(kLogTag, "success.");
        }
        catch(Exception e)
        {
            Logger.error(kLogTag, "error while clearing the data: " + e);
        }
    }

    public static TokenInfo accessToken(
        Context context,
        String cloudSystemId,
        String user)
    {
        Logger.updateContext(context);

        String kLogTag = "load access token";

        if (TextUtils.isEmpty(cloudSystemId) || TextUtils.isEmpty(user))
            return null;

        SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ true);
        if (preferences == null)
            return null;

        try
        {
            String tokensData = preferences.getString(kAccessTokensKey, "");
            if (!TextUtils.isEmpty(tokensData))
            {
                JSONObject tokens = new JSONObject(tokensData);
                if (tokens.has(cloudSystemId))
                {
                    JSONObject tokenInfo = tokens.getJSONObject(cloudSystemId);
                    if (tokenInfo != null
                        && tokenInfo.has("accessToken")
                        && tokenInfo.has("expiresAt"))
                    {
                        // The token is issued for one user and must not be used by another one.
                        if (!user.equalsIgnoreCase(tokenInfo.optString("user")))
                        {
                            Logger.info(kLogTag, "token owner does not match.");
                            return null;
                        }

                        final long expiresAt = tokenInfo.getLong("expiresAt");
                        if (expiresAt <= System.currentTimeMillis())
                        {
                            Logger.info(kLogTag, "token has expired.");
                            return null;
                        }

                        Logger.info(kLogTag, "token was loaded.");
                        return new TokenInfo(tokenInfo.getString("accessToken"), expiresAt);
                    }
                }
            }

            Logger.info(kLogTag, "token was not found.");
            return null;
        }
        catch(Exception e)
        {
            Logger.error(kLogTag, "error while loading access tokens: " + e);
            return null;
        }
    }

    // Methods that change access tokens load, change and save all of them, so they are
    // synchronized. The app and its push handlers use them from one process.
    public static synchronized void setAccessToken(
        Context context,
        String cloudSystemId,
        String user,
        String accessToken,
        long expiresAt)
    {
        Logger.updateContext(context);

        String kLogTag = "set access token";

        // An empty token must not remove the stored one, and a token without the user can't be
        // read.
        if (TextUtils.isEmpty(cloudSystemId)
            || TextUtils.isEmpty(user)
            || TextUtils.isEmpty(accessToken))
        {
            return;
        }

        SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ true);
        if (preferences == null)
            return;

        try
        {
            String jsonData = preferences.getString(kAccessTokensKey, "");
            JSONObject tokens = TextUtils.isEmpty(jsonData)
                ? new JSONObject()
                : new JSONObject(jsonData);

            removeExpiredTokens(tokens, kLogTag);

            JSONObject tokenInfo = new JSONObject();
            tokenInfo.put("accessToken", accessToken);
            tokenInfo.put("expiresAt", expiresAt);
            tokenInfo.put("user", user);
            tokens.put(cloudSystemId, tokenInfo);
            Logger.info(kLogTag, "token was updated.");

            SharedPreferences.Editor editor = preferences.edit();
            editor.putString(kAccessTokensKey, tokens.toString());
            editor.commit();
            Logger.info(kLogTag, "acess token data is committed.");
        }
        catch(Exception e)
        {
            Logger.error(kLogTag, "error while loading the data: " + e);
        }
    }

    /** Drops expired tokens of all systems. Otherwise the storage grows with every new system. */
    private static void removeExpiredTokens(JSONObject tokens, String logTag)
    {
        final long now = System.currentTimeMillis();
        final List<String> expiredSystemIds = new ArrayList<String>();
        final Iterator<String> systemIds = tokens.keys();
        while (systemIds.hasNext())
        {
            final String systemId = systemIds.next();
            final JSONObject tokenInfo = tokens.optJSONObject(systemId);
            if (tokenInfo == null || tokenInfo.optLong("expiresAt", 0) <= now)
                expiredSystemIds.add(systemId);
        }

        for (String systemId: expiredSystemIds)
            tokens.remove(systemId);

        if (expiredSystemIds.isEmpty())
            return;

        Logger.info(logTag, String.format("%d expired tokens were removed.",
            expiredSystemIds.size()));
    }

    public static synchronized void removeAccessToken(
        Context context,
        String cloudSystemId,
        String accessToken)
    {
        Logger.updateContext(context);

        String kLogTag = "remove access token";

        if (TextUtils.isEmpty(cloudSystemId) || TextUtils.isEmpty(accessToken))
            return;

        SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ true);
        if (preferences == null)
            return;

        try
        {
            final String tokensData = preferences.getString(kAccessTokensKey, "");
            if (TextUtils.isEmpty(tokensData))
                return;

            final JSONObject tokens = new JSONObject(tokensData);
            final JSONObject tokenInfo = tokens.optJSONObject(cloudSystemId);

            // Another token may have been saved after this one was loaded. It must be kept.
            if (tokenInfo == null || !accessToken.equals(tokenInfo.optString("accessToken")))
            {
                Logger.info(kLogTag, "token was already replaced, nothing to do.");
                return;
            }

            tokens.remove(cloudSystemId);

            SharedPreferences.Editor editor = preferences.edit();
            editor.putString(kAccessTokensKey, tokens.toString());
            editor.commit();
            Logger.info(kLogTag, "token was removed.");
        }
        catch(Exception e)
        {
            Logger.error(kLogTag, "error while removing the token: " + e);
        }
    }

    public static synchronized void clearAccessTokens(Context context)
    {
        Logger.updateContext(context);

        String kLogTag = "clear access tokens";

        SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ true);
        if (preferences == null)
            return;

        try
        {
            SharedPreferences.Editor editor = preferences.edit();
            editor.remove(kAccessTokensKey);
            editor.commit();
            Logger.info(kLogTag, "success.");
        }
        catch(Exception e)
        {
            Logger.error(kLogTag, "error while clearing the data: " + e);
        }
    }

    public static CloudLoggerOptions cloudLoggerOptions(
        Context context)
    {
        Logger.updateContext(context);

        String kLogTag = "load cloud logger options";

        Logger.infoToSystemOnly(kLogTag, "started.");

        final SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ false);
        if (preferences == null)
        {
            Logger.infoToSystemOnly(kLogTag, "preferences are null");
            return null;
        }

        try
        {
            CloudLoggerOptions result = new CloudLoggerOptions();
            result.logSessionId = preferences.getString(kLogSessionIdTag, "");
            result.sessionEndTimeMs = preferences.getLong(kLogSessionEndTimeTag, 0);

            Logger.infoToSystemOnly(kLogTag, "cloud logger option loaded successfully.");

            return result;
        }
        catch(Exception e)
        {
            Logger.errorToSystemOnly(kLogTag, "error while loading cloud logger options: " + e);
            return null;
        }
    }

    public static void setCloudLoggerOptions(
        Context context,
        String logSessionId,
        long sessionEndTimeMs)
    {
        Logger.updateContext(context);

        String kLogTag = "cloud logger options";
        Logger.infoToSystemOnly(kLogTag, "started.");

        final SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ false);
        if (preferences == null)
            return;

        try
        {
            Logger.infoToSystemOnly(kLogTag, "opening the editor.");
            SharedPreferences.Editor editor = preferences.edit();
            editor.putString(kLogSessionIdTag, logSessionId);
            editor.putLong(kLogSessionEndTimeTag, sessionEndTimeMs);
            editor.commit();
            Logger.infoToSystemOnly(kLogTag, "data was commited.");
        }
        catch(Exception e)
        {
            Logger.errorToSystemOnly(kLogTag, "error while storing the data.");
        }
    }

    private static final String kPreferencesLogTag = "getting preferences";

    private static SharedPreferences getPreferences(
        Context context,
        boolean uploadLogs)
    {
        SharedPreferences fixedPreferences =
            getPreferencesInternal(context, /*fileName*/ "fixed_ipc_data", uploadLogs);

        if (fixedPreferences == null)
            return null;

        // Migrate properties from the old storage (if needed).
        final String kMigratedOption = "migrated";
        try
        {
            if (!fixedPreferences.getBoolean(kMigratedOption, /*defValue*/ false))
            {
                SharedPreferences oldPreferences =
                    getPreferencesInternal(context, /*fileName*/ "push_ipc_data", uploadLogs);

                SharedPreferences.Editor editor = fixedPreferences.edit();
                if (oldPreferences != null)
                {
                    // Migrate push properties.
                    editor.putString(kPushUserKey,
                        oldPreferences.getString(kPushUserKey, ""));
                    editor.putString(kCloudRefreshToken,
                        oldPreferences.getString(kCloudRefreshToken, ""));

                    // Migrate stored access token.
                    editor.putString(kAccessTokensKey,
                        oldPreferences.getString(kAccessTokensKey, ""));

                    // Migrate cloud logger properties.
                    editor.putString(kLogSessionIdTag,
                        oldPreferences.getString(kLogSessionIdTag, ""));
                    editor.putLong(kLogSessionEndTimeTag,
                        oldPreferences.getLong(kLogSessionEndTimeTag, 0));
                }

                editor.putBoolean(kMigratedOption, true);
                editor.commit();

                Logger.info(kPreferencesLogTag, "IPC properties migrated successfully");
            }
            else
            {
                Logger.infoToSystemOnly(kPreferencesLogTag, "Skip properties migration");
            }
        }
        catch(Exception e)
        {
            Logger.error(kPreferencesLogTag, "Can't migrate properties: " + e);
        }
        return fixedPreferences;
    }

    private static SharedPreferences getPreferencesInternal(
        Context context,
        String fileName,
        boolean uploadLogs)
    {
        final String startLogMessage = "with the context " + context;
        if (uploadLogs)
            Logger.info(kPreferencesLogTag, startLogMessage);
        else
            Logger.infoToSystemOnly(kPreferencesLogTag, startLogMessage);

        try
        {
            String masterKeyAlias = MasterKeys.getOrCreate(MasterKeys.AES256_GCM_SPEC);
            return EncryptedSharedPreferences.create(
                fileName,
                masterKeyAlias,
                context,
                EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM);
        }
        catch(Exception e)
        {
            final String errorLogMessage = "error while creating the preferences for file "
                + fileName + ": " + e;
            if (uploadLogs)
                Logger.error(kPreferencesLogTag, errorLogMessage);
            else
                Logger.errorToSystemOnly(kPreferencesLogTag, errorLogMessage);

            return null;
        }
    }
}
