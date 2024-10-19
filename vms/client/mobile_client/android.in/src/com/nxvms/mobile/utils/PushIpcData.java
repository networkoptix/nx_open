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
import java.util.Map;
import org.json.JSONObject;

public class PushIpcData
{
    private static final String kPushUserKey = "push_user";
    private static final String kCloudRefreshToken = "cloud_refresh_token";
    private static final String kPushPasswordKey = "push_user_password";

    private static final String kAccessTokensKey = "access_tokens";

    private static final String kLogSessionIdTag = "log_session_id";
    private static final String kLogSessionEndTimeTag = "session_end_time_ms";

    public static class LoginInfo
    {
        public String user;
        public String cloudRefreshToken;
        public String password;
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
        String cloudRefreshToken,
        String password)
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
            editor.putString(kPushPasswordKey, password);
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
            data.password = preferences.getString(kPushPasswordKey, "");

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
        String cloudSystemId)
    {
        Logger.updateContext(context);

        String kLogTag = "load access token";

        if (TextUtils.isEmpty(cloudSystemId))
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
                        Logger.info(kLogTag, "token was loaded.");
                        return new TokenInfo(
                            tokenInfo.getString("accessToken"), tokenInfo.getLong("expiresAt"));
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

    public static void setAccessToken(
        Context context,
        String cloudSystemId,
        String accessToken,
        long expiresAt)
    {
        Logger.updateContext(context);

        String kLogTag = "set access token";

        if (TextUtils.isEmpty(cloudSystemId))
            return;

        SharedPreferences preferences = getPreferences(context, /*uploadLogs*/ true);
        if (preferences == null)
            return;

        try
        {
            String jsonData = preferences.getString(kAccessTokensKey, "");
            JSONObject tokens = TextUtils.isEmpty(jsonData)
                ? new JSONObject()
                : new JSONObject(jsonData);

            SharedPreferences.Editor editor = preferences.edit();
            if (!TextUtils.isEmpty(accessToken))
            {
                JSONObject tokenInfo = new JSONObject();
                tokenInfo.put("accessToken", accessToken);
                tokenInfo.put("expiresAt", expiresAt);
                tokens.put(cloudSystemId, tokenInfo);
                editor.putString(kAccessTokensKey, tokens.toString());
                Logger.info(kLogTag, "token was updated.");
            }
            else if (tokens.has(cloudSystemId))
            {
                tokens.remove(cloudSystemId);
                editor.putString(kAccessTokensKey, tokens.toString());
                Logger.info(kLogTag, "token is empty and record was removed.");
            }
            else
            {
                Logger.info(kLogTag, "token is empty but no record was found, nothing to do.");
                return;
            }

            editor.commit();
            Logger.info(kLogTag, "acess token data is committed.");
        }
        catch(Exception e)
        {
            Logger.error(kLogTag, "error while loading the data: " + e);
        }
    }

    public static void removeAccessToken(
        Context context,
        String cloudSystemId)
    {
        Logger.updateContext(context);

        String kLogTag = "set access token";

        Logger.info(kLogTag, "started.");
        setAccessToken(context, cloudSystemId, null, 0);
        Logger.info(kLogTag, "finished.");
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
                    editor.putString(kPushPasswordKey,
                        oldPreferences.getString(kPushPasswordKey, ""));

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
