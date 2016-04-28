package com.networkoptix.hdwitness.ui;

import com.networkoptix.hdwitness.common.SimpleCrypto;
import com.networkoptix.hdwitness.ui.LogonDataManager.LogonEntry;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;

/** Read-only content provider of the login entries. */
public class LogonDataContentProvider extends ContentProvider {

    private static final String MIME_TYPE = "vnd.android.cursor.item/vnd.com.networkoptix.hdwitness/LogonEntry";
    private static String PREFERENCES_KEY = "com.networkoptix.Preferences";
    
    private static final String cryptoSeed = "KMwTui4J5G";

    private LogonDataManager mLogonDataManager = new LogonDataManager();

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new RuntimeException("Unauthorized access");
    }

    @Override
    public String getType(Uri uri) {
        return MIME_TYPE;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new RuntimeException("Unauthorized access");
    }

    @Override
    public boolean onCreate() {
        getContext();
        SharedPreferences prefs = getContext().getSharedPreferences(PREFERENCES_KEY, Context.MODE_PRIVATE);
        mLogonDataManager.init(prefs);
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {

        String key = uri.getFragment();
        
        String encrypted = SimpleCrypto.encryptSafe(cryptoSeed, key, "");
               
        if (!encrypted.equals("24F54A0B3B74D1DE0D18EE731A5AB2FBED649BC5E4C6B519D74EECFA3DC319F11F3077068550D8CBDC51C9F1F0B13252"))
            throw new RuntimeException("Unauthorized access");

        String[] columnNames = new String[] { "Title", "Login", "Password", "Hostname", "Port" };

        MatrixCursor result = new MatrixCursor(columnNames, mLogonDataManager.getEntries().size());
        for (LogonEntry e : mLogonDataManager.getEntries()) {
            Object[] columnValues = new Object[columnNames.length];
            columnValues[0] = e.Title;
            columnValues[1] = e.Login;
            columnValues[2] = e.Password;
            columnValues[3] = e.Hostname;
            columnValues[4] = e.Port;

            result.addRow(columnValues);
        }

        return result;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        throw new RuntimeException("Unauthorized access");
    }

}
