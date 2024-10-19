// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import com.nxvms.mobile.utils.Logger;

import java.lang.Exception;
import java.net.HttpURLConnection;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HashMap;

import com.google.common.base.CharMatcher;
import com.google.common.base.Joiner;
import com.google.common.base.Splitter;
import com.google.common.collect.Iterables;
import com.google.common.collect.Maps;

public class HttpDigestAuth
{
    public static HttpURLConnection tryAuth(
        HttpURLConnection connection,
        String userName,
        String password)
    {
        if (connection == null)
            return null;

        try
        {
            String auth = connection.getHeaderField("WWW-Authenticate");
            if (auth == null || !auth.startsWith(kDigestHeader))
                return null;

            final HashMap<String, String> authFields = splitAuthFields(auth.substring(7));
            Joiner colonJoiner = Joiner.on(':');

            final String realm = authFields.get("realm");
            final String nonce = authFields.get("nonce");
            final String path = connection.getURL().getPath();
            final String ha1 = getHaDigest(colonJoiner.join(userName, realm, password));
            final String ha2 = getHaDigest(colonJoiner.join(connection.getRequestMethod(), path));
            final String ha3 = getHaDigest(colonJoiner.join(ha1, nonce, ha2));

            final String authorization = Joiner.on(',').join(
                getPairString("username", userName),
                getPairString("realm", realm),
                getPairString("nonce", nonce),
                getPairString("uri", path),
                getPairString("response", ha3));

            final HttpURLConnection result = (HttpURLConnection) connection.getURL().openConnection();
            result.setRequestProperty(
                "Authorization",
                String.format("%s%s", kDigestHeader, authorization));

            return result;
        }
        catch (Exception e)
        {
            return null;
        }
    }

    private static final String kEncoding = "ISO-8859-1";
    private static String getHaDigest(String haString)
    {
        try
        {
            final MessageDigest md5 = MessageDigest.getInstance("MD5");
            md5.update(haString.getBytes(kEncoding));
            return bytesToHexString(md5.digest());
        }
        catch (Exception e)
        {
            return null;
        }
    }

    private static String getPairString(String key, String value)
    {
        return String.format("%s=\"%s\"", key, value == null ? "" : value);
    }

    private static final String kDigestHeader = "Digest ";

    private static HashMap<String, String> splitAuthFields(String authString)
    {
        final HashMap<String, String> fields = Maps.newHashMap();
        final CharMatcher trimmer = CharMatcher.anyOf("\"\t ");
        final Splitter commas = Splitter.on(',').trimResults().omitEmptyStrings();
        final Splitter equals = Splitter.on('=').trimResults(trimmer).limit(2);
        String[] valuePair;
        for (String keyPair: commas.split(authString))
        {
            valuePair = Iterables.toArray(equals.split(keyPair), String.class);
            fields.put(valuePair[0], valuePair[1]);
        }
        return fields;
    }

    private static final String kHexLookup = "0123456789abcdef";
    private static String bytesToHexString(byte[] bytes)
    {
        StringBuilder builder = new StringBuilder(bytes.length * 2);
        for (byte b: bytes)
        {
            builder.append(kHexLookup.charAt((b & 0xF0) >> 4));
            builder.append(kHexLookup.charAt((b & 0x0F) >> 0));
        }
        return builder.toString();
    }
}
