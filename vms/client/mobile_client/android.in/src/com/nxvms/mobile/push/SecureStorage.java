// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

package com.nxvms.mobile.utils;

import com.nxvms.mobile.utils.Logger;

import android.content.Context;
import android.content.SharedPreferences;
import androidx.security.crypto.EncryptedSharedPreferences;
import androidx.security.crypto.MasterKeys;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class SecureStorage
{
    private static final String kLogTag = "secure storage";
    private static final String kPreferencesFileName = "secure_storage";

    public static String load(Context context, String key)
    {
        SharedPreferences preferences = getPreferences(context);
        return preferences != null ? preferences.getString(key, "") : null;
    }

    public static void save(Context context, String key, String value)
    {
        SharedPreferences preferences = getPreferences(context);
        if (preferences != null)
            preferences.edit().putString(key, value).apply();
    }

    public static byte[] loadFile(Context context, String key)
    {
        try
        {
            return Files.readAllBytes(getPath(context, key));
        }
        catch (IOException e)
        {
            Logger.error(kLogTag, String.format("Failed to load file: %s", e));
            return null;
        }
    }

    public static void saveFile(Context context, String key, byte[] data)
    {
        try
        {
            Path path = getPath(context, key);
            Files.createDirectories(path.getParent());
            Files.write(path, data);
        }
        catch (IOException e)
        {
            Logger.error(kLogTag, String.format("Failed to save file: %s" , e));
        }
    }

    public static void removeFile(Context context, String key)
    {
        File file = getPath(context, key).toFile();
        if (!file.delete())
            Logger.error(kLogTag, "Failed to delete file");
    }

    private static Path getPath(Context context, String key)
    {
        return Paths.get(context.getFilesDir().getAbsolutePath() + "/secure_storage/" + key);
    }

    private static SharedPreferences getPreferences(Context context)
    {
        try
        {
            String masterKeyAlias = MasterKeys.getOrCreate(MasterKeys.AES256_GCM_SPEC);
            return EncryptedSharedPreferences.create(
                kPreferencesFileName,
                masterKeyAlias,
                context,
                EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM);
        }
        catch (Exception e)
        {
            Logger.error(kLogTag, String.format("Failed to create shared preferences: %s", e));
            return null;
        }
    }
}
