// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import com.nxvms.mobile.utils.Logger;

import com.nxvms.mobile.utils.Branding;

import android.text.TextUtils;
import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.lang.Exception;
import java.lang.StringBuilder;
import java.net.HttpURLConnection;
import java.net.URL;
import org.json.JSONObject;

public class OAuthHelper
{
    public enum AccessTokenStatus
    {
        unsupported,
        invalid,
        valid
    };

    static public boolean isSuccessHttpCode(int code)
    {
        return code >= HttpURLConnection.HTTP_OK && code < HttpURLConnection.HTTP_MULT_CHOICE;
    }

    static public String hostWithPath(
        String source,
        String path)
    {
        if (TextUtils.isEmpty(source))
            return null;

        try
        {
            final URL url = new URL(source);
            return TextUtils.isEmpty(path)
                ? String.format("%s://%s", url.getProtocol(), url.getHost())
                : String.format("%s://%s/%s", url.getProtocol(), url.getHost(), path);
        }
        catch (Exception e)
        {
            Logger.info("host with path", "can't change of the url.");
            return null;
        }
    }

    static public AccessTokenStatus systemAccessTokenStatus(
        String token,
        String serverUrl)
    {
        final String kLogTag = "access token check";

        try
        {
            Logger.info(kLogTag, "started.");

            if (TextUtils.isEmpty(token))
            {
                Logger.info(kLogTag, "token is empty, status is invalid.");
                return AccessTokenStatus.invalid;
            }

            final URL url = new URL(hostWithPath(serverUrl,
                String.format("rest/v1/login/sessions/%s", token)));

            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setRequestMethod("GET");
            connection.connect();

            final int httpCode = connection.getResponseCode();
            Logger.info(kLogTag, String.format("response code is %d", httpCode));
            if (isSuccessHttpCode(httpCode))
            {
                try
                {
                    final int kMinTokenLength = 6;
                    final JSONObject data = new JSONObject(
                        extractResponse(connection.getInputStream(), kLogTag));
                    final String resultToken = data.optString("token", null);
                    String message = "";
                    if (!TextUtils.isEmpty(resultToken) && resultToken.length() >= kMinTokenLength)
                    {
                        final int expiresIn = data.optInt("expiresInS", -1);
                        final int age = data.optInt("ageS", -1);
                        message = String.format("****%s (expires in: %d, age: %d) ",
                            resultToken.substring(resultToken.length() - kMinTokenLength),
                            expiresIn, age);
                    }
                    Logger.info(kLogTag, String.format("token %sis valid.", message));
                }
                catch (Exception e)
                {
                    Logger.info(kLogTag, "token is valid.");
                }

                return AccessTokenStatus.valid;
            }
            else if (httpCode == HttpURLConnection.HTTP_NOT_FOUND)
            {
                Logger.info(kLogTag, "token functionality is not supported.");
                return AccessTokenStatus.unsupported;
            }

            final String answer = extractResponse(connection.getErrorStream(), kLogTag);

            String message = "";
            if (answer != null)
                message = String.format(", response is %s", answer);

            Logger.info(kLogTag, String.format("token is invalid%s", message));
            return AccessTokenStatus.invalid;
        }
        catch (Exception e)
        {
            Logger.info(kLogTag, "error while requesting the status of the token: " + e.toString());
            return AccessTokenStatus.invalid;
        }
    }

    static public PushIpcData.TokenInfo requestSystemAccessToken(
        String user,
        String cloudRefreshToken,
        String cloudSystemId)
    {
        final String kLogTag = "access token request";

        if (TextUtils.isEmpty(user) || TextUtils.isEmpty(cloudRefreshToken)
            || TextUtils.isEmpty(cloudSystemId))
        {
            Logger.info(kLogTag, "invalid parameters.");
            return null;
        }

        try
        {
            JSONObject body = new JSONObject();
            body.put("grant_type", "refresh_token");
            body.put("response_type", "token");
            body.put("refresh_token", cloudRefreshToken);
            body.put("scope", String.format("cloudSystemId=%s", cloudSystemId));
            body.put("username", user);

            final URL url = new URL(hostWithPath(Branding.kCloudHost, "cdb/oauth2/token"));
            HttpURLConnection connection = (HttpURLConnection) url.openConnection();
            connection.setRequestMethod("POST");
            connection.setRequestProperty("Content-Type", "application/json");
            connection.setDoOutput(true);
            connection.setDoInput(true);

            PrintStream out = new PrintStream(connection.getOutputStream());
            out.print(body.toString());
            out.close();

            connection.connect();

            final int httpCode = connection.getResponseCode();
            if (!isSuccessHttpCode(httpCode))
            {
                Logger.info(kLogTag, "failed");
                return null;
            }

            final JSONObject response = new JSONObject(
                extractResponse(connection.getInputStream(), kLogTag));
            final String token = response == null
                ? null
                : response.optString("access_token", null);
            final String expiresAtStr = response == null
                ? null
                : response.optString("expires_at", null);

            long expiresAt = 0;
            if (!TextUtils.isEmpty(expiresAtStr))
                expiresAt = Long.parseLong(expiresAtStr, 10);

            if (TextUtils.isEmpty(token))
            {
                Logger.info(kLogTag, "can't find token field.");
                return null;
            }
            Logger.info(kLogTag, "access token was created.");
            return new PushIpcData.TokenInfo(token, expiresAt);
        }
        catch (Exception e)
        {
            Logger.info(kLogTag, "error while requesting access token: " + e.toString());
            return null;
        }
    }

    static public String extractResponse(InputStream stream, String logTag)
    {
        try (BufferedReader reader = new BufferedReader(
            new InputStreamReader(stream, "utf-8")))
        {
            StringBuilder response = new StringBuilder();
            String responseLine = null;

            while ((responseLine = reader.readLine()) != null)
                response.append(responseLine.trim());

            return response.toString();
        }
        catch (Exception e)
        {
            Logger.info(logTag, "can't handle response");
            return null;
        }
    }
};
