// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import android.content.Context;
import android.util.Log;
import android.text.TextUtils;
import java.io.DataOutputStream;
import java.lang.Thread;
import java.net.HttpURLConnection;
import java.net.URL;
import java.text.SimpleDateFormat;
import java.util.Date;

import com.nxvms.mobile.utils.Branding;
import com.nxvms.mobile.utils.PushIpcData;
import com.nxvms.mobile.utils.CloudLoggerOptions;

public class Logger extends Object
{
    private static final String kInfoTag = "V";
    private static final String kErrorTag = "E";

    private static Context context;

    private static String makeTag(String tag)
    {
        final String kLogTag = "[Mobile client]:";
        return TextUtils.isEmpty(tag)
            ? kLogTag
            : kLogTag + tag;
    }

    public static void updateContext(Context value)
    {
        if (context != value)
            context = value;
    }

    public static void info(String tag, String message)
    {
        infoToSystemOnly(tag, message);
        sUploader.upload(kInfoTag, tag, message);
    }

    public static void error(String tag, String message)
    {
        errorToSystemOnly(tag, message);
        sUploader.upload(kErrorTag, tag, message);
    }

    public static void infoToSystemOnly(String tag, String message)
    {
        Log.v(makeTag(tag), message);
    }

    public static void errorToSystemOnly(String tag, String message)
    {
        Log.e(makeTag(tag), message);
    }

    private static class Uploader extends Object
    {
        private class Worker extends Thread
        {
            private Uploader parent;
            Worker(Uploader parent)
            {
                this.parent = parent;
            }

            public void run()
            {
                final String kLogTag = "log uploader";
                while (true)
                {
                    String sessionId = "";
                    String buffer = "";

                    try
                    {
                        synchronized(parent)
                        {
                            parent.wait();

                            if (TextUtils.isEmpty(parent.sessionId)
                                || TextUtils.isEmpty(parent.buffer))
                            {
                                continue;
                            }

                            sessionId = parent.sessionId;
                            buffer = parent.buffer;
                            parent.buffer = "";
                        }
                    }
                    catch (InterruptedException e)
                    {
                    }

                    if (TextUtils.isEmpty(sessionId) || TextUtils.isEmpty(buffer))
                        continue;

                    try
                    {
                        final URL url = new URL(String.format(
                            "%s/log-collector/v1/log-session/%s/log/fragment",
                            Branding.kCloudHost,
                            sessionId));

                        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
                        connection.setDoOutput(true);
                        connection.setRequestMethod("POST");
                        connection.setRequestProperty("Content-Type", "text/plain");

                        DataOutputStream stream = new DataOutputStream(
                            connection.getOutputStream());
                        stream.writeBytes(buffer);
                        stream.flush();
                        stream.close();

                        final int code = connection.getResponseCode();
                        infoToSystemOnly(kLogTag, String.format(
                            "Upload request response code is %d", code));
                        if (code >= HttpURLConnection.HTTP_OK
                            && code < HttpURLConnection.HTTP_MULT_CHOICE)
                        {
                            infoToSystemOnly(kLogTag,
                                String.format("successfully uploaded %d bytes", buffer.length()));
                        }
                        else
                        {
                            errorToSystemOnly(kLogTag,
                                String.format("failed to upload log messages: code %d", code));
                        }
                    }
                    catch (Exception e)
                    {
                        errorToSystemOnly(kLogTag, "error while uploading logs: " + e.toString());
                    }
                }
            }
        };

        private static final SimpleDateFormat timeFormat
            = new SimpleDateFormat("MM/dd/yyyy HH:mm:ss");
        private static final String kLogTag = "uploader";
        private Worker worker = new Worker(this);
        private long sessionEndTimeMs = 0;
        private String sessionId = "";
        public String buffer = "";

        Uploader()
        {
            worker.start();
        }

        public synchronized void upload(
            final String typeTag,
            final String tag,
            final String message)
        {
            if (context == null)
                return;

            final CloudLoggerOptions options = PushIpcData.cloudLoggerOptions(context);
            if (options == null || options.logSessionId == null)
                return;

            if (!options.logSessionId.equals(sessionId))
            {
                if (TextUtils.isEmpty(options.logSessionId))
                    resetCurrentSessionDataUnsafe();
                else
                    setCurrentSessionDataUnsafe(options);
            }

            // Check if current session is outdated.
            if (sessionEndTimeMs != 0 && (new Date()).getTime() > sessionEndTimeMs)
            {
                infoToSystemOnly(kLogTag, "cloud login session is outdated, resettings options.");
                PushIpcData.setCloudLoggerOptions(
                    context, /*logSessionId*/ "", /*sessionEndTimeMs*/ 0);
                resetCurrentSessionDataUnsafe();
            }

            if (TextUtils.isEmpty(sessionId))
                return;

            final String dateString = timeFormat.format(new Date());
            final String finalMessage = String.format("%s %s %s: %s",
                dateString, typeTag , makeTag(tag), message);
            buffer += finalMessage + "\r\n";
            notify();
        }

        private void resetCurrentSessionDataUnsafe()
        {
            infoToSystemOnly(kLogTag,
                "resetting current cloud log upload session with id " + sessionId);
            sessionId = "";
            sessionEndTimeMs = 0;
            buffer = "";
        }

        private void setCurrentSessionDataUnsafe(CloudLoggerOptions options)
        {
            infoToSystemOnly(kLogTag,
                "initializing new cloud log upload session with id " + options.logSessionId);
            sessionId = options.logSessionId;
            sessionEndTimeMs = options.sessionEndTimeMs;
            buffer = "";
        }
    };

    static private Uploader sUploader = new Uploader();
}
